/*
 * Phase 8d: on-device State of Health (capacity-fade learning).
 * Design doc: docs/plans/2026-05-29-phase-8d-soh-design.md
 */
#include <battery_sdk/battery_soh.h>
#include <battery_sdk/battery_status.h>
#include <stdbool.h>
#include <stddef.h>

static int32_t g_rated_x100;
static int32_t g_learned_x100;
static bool    g_armed;
static bool    g_initialized;

int battery_soh_init(int32_t rated_mah_x100)
{
    if (rated_mah_x100 <= 0) {
        return BATTERY_STATUS_INVALID_ARG;
    }
    g_rated_x100 = rated_mah_x100;
    g_learned_x100 = rated_mah_x100;
    g_armed = false;
    g_initialized = true;
    return BATTERY_STATUS_OK;
}

int battery_soh_get_pct_x100(uint16_t *soh_x100_out)
{
    if (soh_x100_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }
    if (!g_initialized) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }
    /* int64 intermediate: learned can reach 120% of rated (guard ceiling);
     * for large packs (learned * 10000) would overflow int32. Matches the
     * int64 discipline in battery_coulomb.c. */
    int32_t soh = (int32_t)(((int64_t)g_learned_x100 * 10000) / g_rated_x100);
    if (soh < 0) soh = 0;
    if (soh > 10000) soh = 10000;
    *soh_x100_out = (uint16_t)soh;
    return BATTERY_STATUS_OK;
}

int battery_soh_get_learned_capacity_mah_x100(int32_t *cap_x100_out)
{
    if (cap_x100_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }
    if (!g_initialized) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }
    *cap_x100_out = g_learned_x100;
    return BATTERY_STATUS_OK;
}

int battery_soh_reset(void)
{
    if (!g_initialized) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }
    g_learned_x100 = g_rated_x100;
    g_armed = false;
    return BATTERY_STATUS_OK;
}

void battery_soh_note_full_anchor(void)
{
    if (!g_initialized) {
        return;
    }
    g_armed = true;
}

int battery_soh_observe_empty_anchor(int32_t q_before_empty_x100)
{
    if (!g_initialized) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }
    if (!g_armed) {
        return BATTERY_STATUS_OK;  /* not a full->empty excursion */
    }
    g_armed = false;  /* excursion consumed regardless of outcome */

    int32_t measured = g_rated_x100 - q_before_empty_x100;

    /* Plausibility guard: reject partial cycles / noise. */
    int32_t lo = (g_rated_x100 * CONFIG_BATTERY_SOC_SOH_REJECT_LO_PCT) / 100;
    int32_t hi = (g_rated_x100 * CONFIG_BATTERY_SOC_SOH_REJECT_HI_PCT) / 100;
    if (measured < lo || measured > hi) {
        return BATTERY_STATUS_OK;  /* rejected, learned unchanged */
    }

    /* EMA, round-to-nearest (half away from zero) to avoid one-sided bias. */
    int32_t delta = measured - g_learned_x100;
    int32_t num = delta * (int32_t)CONFIG_BATTERY_SOC_SOH_ALPHA_X1000;
    int32_t step = (num >= 0) ? (num + 500) / 1000 : (num - 500) / 1000;
    g_learned_x100 += step;
    return BATTERY_STATUS_OK;
}
