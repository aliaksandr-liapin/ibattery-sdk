#include <battery_sdk/battery_soc_estimator.h>
#include <battery_sdk/battery_voltage.h>
#include <battery_sdk/battery_status.h>
#include <battery_sdk/battery_sdk.h>

#include "../core/battery_internal.h"
#include "battery_soc_lut.h"

#if defined(CONFIG_BATTERY_SOC_TEMP_COMP)
#include <battery_sdk/battery_temperature.h>
#include "battery_soc_temp_comp.h"
#endif

#if defined(CONFIG_BATTERY_SOC_COULOMB)
#include <battery_sdk/battery_coulomb.h>
#include <battery_sdk/battery_hal_current.h>

#if defined(CONFIG_BATTERY_SOC_FUSION)
#include <battery_sdk/battery_soc_fusion.h>
#endif

#if defined(CONFIG_BATTERY_SOC_SOH)
#include <battery_sdk/battery_soh.h>
#endif

#ifndef CONFIG_BATTERY_CAPACITY_MAH
#define CONFIG_BATTERY_CAPACITY_MAH 1000
#endif

/* Anchor thresholds — chemistry dependent */
#if defined(CONFIG_BATTERY_CHEMISTRY_LIPO)
#define SOC_ANCHOR_FULL_MV      4180
#define SOC_ANCHOR_FULL_I_X100  5000   /* |I| < 50.00 mA */
#define SOC_ANCHOR_EMPTY_MV     3000
#else  /* CR2032 */
#define SOC_ANCHOR_FULL_MV      2950
#define SOC_ANCHOR_FULL_I_X100  0      /* no current check for primary cell */
#define SOC_ANCHOR_EMPTY_MV     2000
#endif

static uint16_t g_coulomb_soc_x100;
static bool     g_coulomb_soc_valid;
/*
 * Anchor edge-detect state (issue #1, v0.8.4):
 * The full / empty anchors are one-shot calibration events that fire on
 * *transition* into the anchor voltage region, not on every sample while
 * voltage stays there. Without this, on a CR2032 (where the anchor's
 * |I| gate is disabled) the full anchor re-fires every sample and pins
 * the coulomb counter at capacity forever.
 */
static bool     g_full_anchor_active;
static bool     g_empty_anchor_active;
#endif /* CONFIG_BATTERY_SOC_COULOMB */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(CONFIG_BATTERY_SOC_SLEW_LIMIT)

#ifndef CONFIG_BATTERY_SOC_SLEW_RATE_PCT_PER_MIN
#define CONFIG_BATTERY_SOC_SLEW_RATE_PCT_PER_MIN 5
#endif

static uint16_t g_prev_soc_x100;
static uint32_t g_prev_uptime_ms;
static bool     g_slew_initialized;

#endif  /* CONFIG_BATTERY_SOC_SLEW_LIMIT */

/**
 * Compute LUT-based SoC from voltage (always available as baseline).
 */
static int soc_from_lut(uint16_t voltage_mv, uint16_t *soc_pct_x100)
{
#if defined(CONFIG_BATTERY_CHEMISTRY_LIPO)
    return battery_soc_lut_interpolate(&battery_soc_lut_lipo_1s,
                                       voltage_mv,
                                       soc_pct_x100);
#else
    return battery_soc_lut_interpolate(&battery_soc_lut_cr2032,
                                       voltage_mv,
                                       soc_pct_x100);
#endif
}

#if defined(CONFIG_BATTERY_SOC_SLEW_LIMIT)
/**
 * Apply slew-rate limit. First call seeds state and returns input unchanged.
 * Subsequent calls cap delta to (rate * dt_ms) / 60000 in x100 units.
 */
static uint16_t apply_slew_limit(uint16_t new_soc_x100)
{
    uint32_t now_ms;
    if (battery_sdk_get_uptime_ms(&now_ms) != BATTERY_STATUS_OK) {
        return new_soc_x100;  /* Cannot compute dt — bypass */
    }

    if (!g_slew_initialized) {
        g_prev_soc_x100 = new_soc_x100;
        g_prev_uptime_ms = now_ms;
        g_slew_initialized = true;
        return new_soc_x100;
    }

    uint32_t dt_ms = now_ms - g_prev_uptime_ms;

    /* max delta in x100 units: rate_pct_per_min * 100 * dt_ms / 60000 */
    int32_t max_delta = (int32_t)(((uint64_t)CONFIG_BATTERY_SOC_SLEW_RATE_PCT_PER_MIN
                                   * 100ULL * dt_ms) / 60000ULL);

    int32_t delta = (int32_t)new_soc_x100 - (int32_t)g_prev_soc_x100;
    int32_t result = (int32_t)g_prev_soc_x100;

    if (delta > max_delta) {
        result += max_delta;
    } else if (delta < -max_delta) {
        result -= max_delta;
    } else {
        result += delta;
    }

    if (result < 0) result = 0;
    if (result > 10000) result = 10000;

    g_prev_soc_x100 = (uint16_t)result;
    g_prev_uptime_ms = now_ms;
    return g_prev_soc_x100;
}
#endif  /* CONFIG_BATTERY_SOC_SLEW_LIMIT */

int battery_soc_estimator_init(void)
{
    struct battery_sdk_runtime_state *state = battery_sdk_state();
    state->soc_initialized = true;
#if defined(CONFIG_BATTERY_SOC_COULOMB)
    g_coulomb_soc_valid = false;
    g_full_anchor_active = false;
    g_empty_anchor_active = false;
#if defined(CONFIG_BATTERY_SOC_SOH)
    battery_soh_init((int32_t)CONFIG_BATTERY_CAPACITY_MAH * 100);
#endif
#endif
#if defined(CONFIG_BATTERY_SOC_SLEW_LIMIT)
    g_slew_initialized = false;
#endif
    return BATTERY_STATUS_OK;
}

int battery_soc_estimator_get_pct_x100(uint16_t *soc_pct_x100)
{
    int rc;
    uint16_t voltage_mv;

    if (soc_pct_x100 == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    rc = battery_voltage_get_mv(&voltage_mv);
    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }

#if defined(CONFIG_BATTERY_SOC_TEMP_COMP)
    {
        int32_t temp_c_x100;
        rc = battery_temperature_get_c_x100(&temp_c_x100);
        if (rc == BATTERY_STATUS_OK) {
            return battery_soc_temp_compensated(voltage_mv,
                                                temp_c_x100,
                                                soc_pct_x100);
        }
        /* Temperature read failed — fall through to room-temp LUT */
    }
#endif

    /* Always compute LUT SoC as baseline / anchor source */
    uint16_t lut_soc;
    rc = soc_from_lut(voltage_mv, &lut_soc);
    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }

#if defined(CONFIG_BATTERY_SOC_COULOMB)
    {
        int32_t current_x100;
        rc = battery_hal_current_read_ma_x100(&current_x100);
        if (rc != BATTERY_STATUS_OK) {
            /* Current sensor unavailable — graceful fallback to LUT */
#if defined(CONFIG_BATTERY_SOC_SLEW_LIMIT)
            *soc_pct_x100 = apply_slew_limit(lut_soc);
#else
            *soc_pct_x100 = lut_soc;
#endif
            return BATTERY_STATUS_OK;
        }

        /*
         * Check anchor conditions.
         *
         * Anchors are one-shot calibration events (issue #1, v0.8.4): the
         * coulomb counter is reset to the anchor value only when the
         * voltage *transitions into* the anchor region, not while it stays
         * there. Once anchored, the integrator is allowed to track current
         * draw without being clobbered every sample. The anchor re-arms
         * when voltage leaves the region, so a battery that recovers from
         * a brief sag can re-anchor at the next idle.
         */
        int32_t abs_current = current_x100 < 0 ? -current_x100 : current_x100;
        bool in_empty_region = (voltage_mv <= SOC_ANCHOR_EMPTY_MV);
        bool in_full_region  = (voltage_mv >= SOC_ANCHOR_FULL_MV) &&
                               (SOC_ANCHOR_FULL_I_X100 == 0 ||
                                abs_current < SOC_ANCHOR_FULL_I_X100);

        if (in_empty_region) {
            if (!g_empty_anchor_active) {
                /* Edge into empty region — calibrate to 0 mAh. */
                g_coulomb_soc_x100 = 0;
                g_coulomb_soc_valid = true;
#if defined(CONFIG_BATTERY_SOC_SOH)
                {
                    /* Feed SoH the remaining charge BEFORE the reset below
                     * zeroes it. The read cannot fail on this path (coulomb
                     * is initialized whenever this branch runs); if it ever
                     * did, we intentionally skip the update — feeding a bad Q
                     * into the EMA would be worse than dropping one excursion.
                     * SoH stays armed in that (unreachable) case, which is
                     * acceptable: the next empty edge would simply re-measure. */
                    int32_t q_before = 0;
                    if (battery_coulomb_get_mah_x100(&q_before) == BATTERY_STATUS_OK) {
                        battery_soh_observe_empty_anchor(q_before);
                    }
                }
#endif
                (void)battery_coulomb_reset(0);
                g_empty_anchor_active = true;
            }
            g_full_anchor_active = false;  /* re-arm opposite anchor */
        } else if (in_full_region) {
            if (!g_full_anchor_active) {
                /* Edge into full region — calibrate to capacity. */
                int32_t full_mah_x100 = (int32_t)CONFIG_BATTERY_CAPACITY_MAH * 100;
                g_coulomb_soc_x100 = 10000;
                g_coulomb_soc_valid = true;
                (void)battery_coulomb_reset(full_mah_x100);
                g_full_anchor_active = true;
#if defined(CONFIG_BATTERY_SOC_SOH)
                battery_soh_note_full_anchor();
#endif
            }
            g_empty_anchor_active = false;
        } else {
            /* Mid-range — re-arm both anchors so the next edge fires. */
            g_full_anchor_active = false;
            g_empty_anchor_active = false;
        }

        if (!g_coulomb_soc_valid) {
            /* Not yet anchored — seed from LUT */
            int32_t seed_mah_x100 = (int32_t)((uint32_t)lut_soc *
                                     CONFIG_BATTERY_CAPACITY_MAH / 100);
            g_coulomb_soc_valid = true;
            (void)battery_coulomb_reset(seed_mah_x100);
        }

        /* Read accumulated mAh from coulomb counter */
        int32_t mah_x100;
        rc = battery_coulomb_get_mah_x100(&mah_x100);
        if (rc != BATTERY_STATUS_OK) {
#if defined(CONFIG_BATTERY_SOC_SLEW_LIMIT)
            *soc_pct_x100 = apply_slew_limit(lut_soc);
#else
            *soc_pct_x100 = lut_soc;
#endif
            return BATTERY_STATUS_OK;
        }

        /* SoC = mah / capacity * 10000 */
        int32_t capacity_x100 = (int32_t)CONFIG_BATTERY_CAPACITY_MAH * 100;
        int32_t soc = (mah_x100 * 10000) / capacity_x100;

        /* Clamp 0..10000 */
        if (soc < 0) {
            soc = 0;
        } else if (soc > 10000) {
            soc = 10000;
        }

#if defined(CONFIG_BATTERY_SOC_FUSION)
        /* Phase 8c fusion: blend coulomb SoC with LUT SoC, weighted by
         * current magnitude (small current -> trust voltage more). The
         * existing `lut_soc` and `abs_current` variables are in scope from
         * earlier in this function. */
        soc = (int32_t)battery_soc_fusion_blend(
            (uint16_t)lut_soc,
            (uint16_t)soc,
            abs_current);
#endif

        g_coulomb_soc_x100 = (uint16_t)soc;
#if defined(CONFIG_BATTERY_SOC_SLEW_LIMIT)
        g_coulomb_soc_x100 = apply_slew_limit(g_coulomb_soc_x100);
#endif
        *soc_pct_x100 = g_coulomb_soc_x100;
        return BATTERY_STATUS_OK;
    }
#else
#if defined(CONFIG_BATTERY_SOC_SLEW_LIMIT)
    *soc_pct_x100 = apply_slew_limit(lut_soc);
#else
    *soc_pct_x100 = lut_soc;
#endif
    return BATTERY_STATUS_OK;
#endif
}
