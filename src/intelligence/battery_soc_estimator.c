#include <battery_sdk/battery_soc_estimator.h>
#include <battery_sdk/battery_voltage.h>
#include <battery_sdk/battery_status.h>

#include "../core/battery_internal.h"
#include "battery_soc_lut.h"

#if defined(CONFIG_BATTERY_SOC_TEMP_COMP)
#include <battery_sdk/battery_temperature.h>
#include "battery_soc_temp_comp.h"
#endif

#if defined(CONFIG_BATTERY_SOC_COULOMB)
#include <battery_sdk/battery_coulomb.h>
#include <battery_sdk/battery_hal_current.h>

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
#endif /* CONFIG_BATTERY_SOC_COULOMB */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

int battery_soc_estimator_init(void)
{
    struct battery_sdk_runtime_state *state = battery_sdk_state();
    state->soc_initialized = true;
#if defined(CONFIG_BATTERY_SOC_COULOMB)
    g_coulomb_soc_valid = false;
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
            *soc_pct_x100 = lut_soc;
            return BATTERY_STATUS_OK;
        }

        /* Check anchor conditions */
        int32_t abs_current = current_x100 < 0 ? -current_x100 : current_x100;

        if (voltage_mv <= SOC_ANCHOR_EMPTY_MV) {
            /* Empty anchor — reset coulomb counter to 0 */
            g_coulomb_soc_x100 = 0;
            g_coulomb_soc_valid = true;
            (void)battery_coulomb_reset(0);
        } else if (voltage_mv >= SOC_ANCHOR_FULL_MV &&
                   (SOC_ANCHOR_FULL_I_X100 == 0 ||
                    abs_current < SOC_ANCHOR_FULL_I_X100)) {
            /* Full anchor — reset coulomb counter to capacity */
            int32_t full_mah_x100 = (int32_t)CONFIG_BATTERY_CAPACITY_MAH * 100;
            g_coulomb_soc_x100 = 10000;
            g_coulomb_soc_valid = true;
            (void)battery_coulomb_reset(full_mah_x100);
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
            *soc_pct_x100 = lut_soc;
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

        g_coulomb_soc_x100 = (uint16_t)soc;
        *soc_pct_x100 = g_coulomb_soc_x100;
        return BATTERY_STATUS_OK;
    }
#else
    *soc_pct_x100 = lut_soc;
    return BATTERY_STATUS_OK;
#endif
}
