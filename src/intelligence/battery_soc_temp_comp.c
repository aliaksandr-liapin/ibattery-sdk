#include "battery_soc_temp_comp.h"
#include "battery_soc_lut.h"
#include <battery_sdk/battery_status.h>
#include <stddef.h>

/*
 * Temperature zone boundaries in 0.01 deg C units.
 *
 *   Zone 1:  temp < 0 C       -> cold LUT only
 *   Zone 2:  0 C .. 25 C      -> blend cold + room
 *   Zone 3:  25 C .. 45 C     -> blend room + hot
 *   Zone 4:  temp > 45 C      -> hot LUT only
 */
#define TEMP_COLD_C_X100     0       /*   0.00 deg C */
#define TEMP_ROOM_C_X100     2500    /*  25.00 deg C */
#define TEMP_HOT_C_X100      4500    /*  45.00 deg C */

/**
 * Blend two SoC values using Q8 fixed-point fraction.
 *
 * result = soc_low + ((soc_high - soc_low) * fraction_q8) / 256
 *
 * fraction_q8 = 0   -> result = soc_low
 * fraction_q8 = 256 -> result = soc_high
 */
static uint16_t blend_soc_q8(uint16_t soc_low, uint16_t soc_high,
                              uint32_t fraction_q8)
{
    int32_t diff = (int32_t)soc_high - (int32_t)soc_low;
    int32_t result = (int32_t)soc_low + (diff * (int32_t)fraction_q8) / 256;

    /* Clamp to valid range */
    if (result < 0) {
        return 0;
    }
    if (result > 10000) {
        return 10000;
    }
    return (uint16_t)result;
}

int battery_soc_temp_compensated(uint16_t voltage_mv,
                                 int32_t temp_c_x100,
                                 uint16_t *soc_pct_x100)
{
    int rc;
    uint16_t soc_a;
    uint16_t soc_b;
    uint32_t fraction_q8;
    int32_t t_low;
    int32_t t_high;
    const battery_soc_lut_t *lut_low;
    const battery_soc_lut_t *lut_high;

    if (soc_pct_x100 == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    /* Zone 1: below 0 C — cold LUT only */
    if (temp_c_x100 <= TEMP_COLD_C_X100) {
        return battery_soc_lut_interpolate(&battery_soc_lut_lipo_1s_cold,
                                           voltage_mv, soc_pct_x100);
    }

    /* Zone 4: above 45 C — hot LUT only */
    if (temp_c_x100 >= TEMP_HOT_C_X100) {
        return battery_soc_lut_interpolate(&battery_soc_lut_lipo_1s_hot,
                                           voltage_mv, soc_pct_x100);
    }

    /* Zone 2: 0..25 C — blend cold + room */
    if (temp_c_x100 <= TEMP_ROOM_C_X100) {
        lut_low  = &battery_soc_lut_lipo_1s_cold;
        lut_high = &battery_soc_lut_lipo_1s;
        t_low    = TEMP_COLD_C_X100;
        t_high   = TEMP_ROOM_C_X100;
    }
    /* Zone 3: 25..45 C — blend room + hot */
    else {
        lut_low  = &battery_soc_lut_lipo_1s;
        lut_high = &battery_soc_lut_lipo_1s_hot;
        t_low    = TEMP_ROOM_C_X100;
        t_high   = TEMP_HOT_C_X100;
    }

    /* Interpolate SoC from both bracketing LUTs */
    rc = battery_soc_lut_interpolate(lut_low, voltage_mv, &soc_a);
    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }

    rc = battery_soc_lut_interpolate(lut_high, voltage_mv, &soc_b);
    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }

    /* Q8 blending fraction: (temp - t_low) * 256 / (t_high - t_low) */
    fraction_q8 = (uint32_t)(temp_c_x100 - t_low) * 256
                  / (uint32_t)(t_high - t_low);

    *soc_pct_x100 = blend_soc_q8(soc_a, soc_b, fraction_q8);

    return BATTERY_STATUS_OK;
}
