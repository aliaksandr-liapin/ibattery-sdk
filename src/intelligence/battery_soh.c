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

int battery_soh_init(int32_t rated_mah_x100)
{
    if (rated_mah_x100 <= 0) {
        return BATTERY_STATUS_INVALID_ARG;
    }
    g_rated_x100 = rated_mah_x100;
    g_learned_x100 = rated_mah_x100;
    g_armed = false;
    return BATTERY_STATUS_OK;
}

int battery_soh_get_pct_x100(uint16_t *soh_x100_out)
{
    if (soh_x100_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }
    int32_t soh = (g_learned_x100 * 10000) / g_rated_x100;
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
    *cap_x100_out = g_learned_x100;
    return BATTERY_STATUS_OK;
}

int battery_soh_reset(void)
{
    g_learned_x100 = g_rated_x100;
    g_armed = false;
    return BATTERY_STATUS_OK;
}

void battery_soh_note_full_anchor(void) { g_armed = true; }

int battery_soh_observe_empty_anchor(int32_t q_before_empty_x100)
{
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
