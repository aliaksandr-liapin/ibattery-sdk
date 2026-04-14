/*
 * Coulomb counter — trapezoidal integration of current over time.
 *
 * Internal accumulator uses int64_t in 0.001 mAh units (sub-mAh precision).
 * External API uses 0.01 mAh units (x100 convention).
 * NVS stores accumulated_mah_x1000 / 10 as int32 (giving x100 precision).
 *
 * Integration math (integer-only):
 *   avg_current_x100 = (prev_current_x100 + current_x100) / 2
 *   delta_mah_x1000  = avg_current_x100 * dt_ms / 360000
 */

#include <battery_sdk/battery_coulomb.h>
#include <battery_sdk/battery_status.h>

#include "../hal/battery_hal_nvs.h"

#include <stdbool.h>
#include <stddef.h>

/* NVS persistence thresholds */
#define NVS_SAVE_INTERVAL_MS   60000   /* 60 seconds */
#define NVS_SAVE_DELTA_X1000   1000    /* 1.000 mAh change */

static int64_t  g_accumulated_mah_x1000;
static int64_t  g_remainder;            /* sub-unit remainder from division */
static int32_t  g_prev_current_x100;
static bool     g_has_prev;
static bool     g_initialized;

/* NVS persistence tracking */
static int64_t  g_last_saved_mah_x1000;
static uint32_t g_ms_since_last_save;

static void persist_to_nvs(void)
{
    /* Store as int32 in x100 units (divide x1000 by 10) */
    int32_t val_x100 = (int32_t)(g_accumulated_mah_x1000 / 10);
    (void)battery_hal_nvs_write_u32(BATTERY_NVS_KEY_COULOMB_MAH,
                                     (uint32_t)val_x100);
    g_last_saved_mah_x1000 = g_accumulated_mah_x1000;
    g_ms_since_last_save = 0;
}

int battery_coulomb_init(void)
{
    int rc;

    g_accumulated_mah_x1000 = 0;
    g_remainder = 0;
    g_prev_current_x100 = 0;
    g_has_prev = false;
    g_last_saved_mah_x1000 = 0;
    g_ms_since_last_save = 0;

    rc = battery_hal_nvs_init();
    if (rc != BATTERY_STATUS_OK) {
        /* NVS unavailable — start at 0, best-effort */
        g_initialized = true;
        return BATTERY_STATUS_OK;
    }

    uint32_t stored;
    rc = battery_hal_nvs_read_u32(BATTERY_NVS_KEY_COULOMB_MAH, &stored);
    if (rc == BATTERY_STATUS_OK) {
        /* Stored value is in x100 units; convert to x1000 */
        g_accumulated_mah_x1000 = (int64_t)((int32_t)stored) * 10;
        g_last_saved_mah_x1000 = g_accumulated_mah_x1000;
    }

    g_initialized = true;
    return BATTERY_STATUS_OK;
}

int battery_coulomb_update(int32_t current_ma_x100, uint32_t dt_ms)
{
    if (!g_initialized) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }

    if (!g_has_prev) {
        /* First sample — just record, no integration */
        g_prev_current_x100 = current_ma_x100;
        g_has_prev = true;
        return BATTERY_STATUS_OK;
    }

    if (dt_ms == 0) {
        /* Zero time step — update prev but no integration */
        g_prev_current_x100 = current_ma_x100;
        return BATTERY_STATUS_OK;
    }

    /* Trapezoidal rule: average of previous and current */
    int64_t avg_x100 = ((int64_t)g_prev_current_x100 + (int64_t)current_ma_x100) / 2;

    /*
     * delta_mah_x1000 = avg_x100 * dt_ms / 360000
     *
     * Accumulate remainder to avoid per-step truncation drift.
     */
    int64_t numerator = avg_x100 * (int64_t)dt_ms + g_remainder;
    int64_t delta_mah_x1000 = numerator / 360000;
    g_remainder = numerator - delta_mah_x1000 * 360000;

    g_accumulated_mah_x1000 += delta_mah_x1000;
    g_prev_current_x100 = current_ma_x100;

    /* Check if NVS persistence is needed */
    g_ms_since_last_save += dt_ms;

    int64_t change = g_accumulated_mah_x1000 - g_last_saved_mah_x1000;
    if (change < 0) {
        change = -change;
    }

    if (g_ms_since_last_save >= NVS_SAVE_INTERVAL_MS ||
        change >= NVS_SAVE_DELTA_X1000) {
        persist_to_nvs();
    }

    return BATTERY_STATUS_OK;
}

int battery_coulomb_get_mah_x100(int32_t *mah_x100_out)
{
    if (mah_x100_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    if (!g_initialized) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }

    /* Convert internal x1000 to external x100 */
    *mah_x100_out = (int32_t)(g_accumulated_mah_x1000 / 10);
    return BATTERY_STATUS_OK;
}

int battery_coulomb_reset(int32_t mah_x100)
{
    if (!g_initialized) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }

    g_accumulated_mah_x1000 = (int64_t)mah_x100 * 10;
    persist_to_nvs();
    return BATTERY_STATUS_OK;
}
