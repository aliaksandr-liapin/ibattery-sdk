#include "battery_ntc_lut.h"
#include <battery_sdk/battery_status.h>
#include <stddef.h>

/*
 * 10K NTC thermistor, B = 3950 K.
 * Entries sorted by resistance_ohm descending (cold → hot).
 *
 * Resistance values computed from B-parameter equation:
 *   R(T) = R0 * exp(B * (1/T - 1/T0))
 *   R0 = 10000 Ω, T0 = 298.15 K (25 °C), B = 3950 K
 *
 * Covers -40 °C to +125 °C.  For battery monitoring the practical
 * range is -20 °C to +60 °C; the extremes are included for safety
 * and clamping.
 *
 * | R (Ω)   | °C   | temp_c_x100 | Region         |
 * |---------|------|-------------|----------------|
 * | 401,600 | -40  | -4000       | Extreme cold   |
 * | 200,500 | -30  | -3000       | Cold           |
 * | 105,300 | -20  | -2000       | Cold           |
 * |  58,320 | -10  | -1000       | Cool           |
 * |  33,640 |   0  |     0       | Freezing point |
 * |  20,200 |  10  |  1000       | Cool           |
 * |  12,520 |  20  |  2000       | Room           |
 * |  10,000 |  25  |  2500       | Reference      |
 * |   8,024 |  30  |  3000       | Room           |
 * |   5,296 |  40  |  4000       | Warm           |
 * |   3,594 |  50  |  5000       | Hot            |
 * |   2,491 |  60  |  6000       | Hot            |
 * |   1,759 |  70  |  7000       | Very hot       |
 * |   1,270 |  80  |  8000       | Very hot       |
 * |     697 | 100  | 10000       | Extreme hot    |
 * |     359 | 125  | 12500       | Extreme hot    |
 */
static const battery_ntc_lut_entry_t ntc_10k_3950_entries[] = {
    { 401600, -4000 },   /* -40 °C */
    { 200500, -3000 },   /* -30 °C */
    { 105300, -2000 },   /* -20 °C */
    {  58320, -1000 },   /* -10 °C */
    {  33640,     0 },   /*   0 °C */
    {  20200,  1000 },   /*  10 °C */
    {  12520,  2000 },   /*  20 °C */
    {  10000,  2500 },   /*  25 °C — reference point */
    {   8024,  3000 },   /*  30 °C */
    {   5296,  4000 },   /*  40 °C */
    {   3594,  5000 },   /*  50 °C */
    {   2491,  6000 },   /*  60 °C */
    {   1759,  7000 },   /*  70 °C */
    {   1270,  8000 },   /*  80 °C */
    {    697, 10000 },   /* 100 °C */
    {    359, 12500 },   /* 125 °C */
};

const battery_ntc_lut_t battery_ntc_lut_10k_3950 = {
    .entries = ntc_10k_3950_entries,
    .count   = sizeof(ntc_10k_3950_entries) / sizeof(ntc_10k_3950_entries[0]),
};

int battery_ntc_resistance_from_mv(uint32_t pullup_ohm,
                                   uint32_t vdd_mv,
                                   uint32_t adc_mv,
                                   uint32_t *resistance_out)
{
    if (resistance_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    if (vdd_mv == 0) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    /* ADC at or above VDD means NTC is open circuit / disconnected */
    if (adc_mv >= vdd_mv) {
        return BATTERY_STATUS_IO;
    }

    /* ADC at 0 means NTC is shorted to GND */
    if (adc_mv == 0) {
        *resistance_out = 0;
        return BATTERY_STATUS_OK;
    }

    /*
     * Voltage divider:  V_adc = VDD * R_ntc / (R_pullup + R_ntc)
     * Solving for R_ntc: R_ntc = R_pullup * V_adc / (VDD - V_adc)
     *
     * Use uint64_t to avoid overflow: pullup_ohm (up to ~100K) * adc_mv
     * (up to ~3300) = up to ~330M which fits uint32_t, but we use uint64_t
     * for safety with larger pull-up values.
     */
    uint64_t numerator   = (uint64_t)pullup_ohm * adc_mv;
    uint32_t denominator = vdd_mv - adc_mv;

    *resistance_out = (uint32_t)(numerator / denominator);
    return BATTERY_STATUS_OK;
}

int battery_ntc_lut_interpolate(const battery_ntc_lut_t *lut,
                                uint32_t resistance_ohm,
                                int32_t *temp_c_x100)
{
    const battery_ntc_lut_entry_t *upper;
    const battery_ntc_lut_entry_t *lower;

    if (lut == NULL || lut->entries == NULL || lut->count == 0 ||
        temp_c_x100 == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    /* Resistance above highest entry → clamp to coldest temperature */
    if (resistance_ohm >= lut->entries[0].resistance_ohm) {
        *temp_c_x100 = lut->entries[0].temp_c_x100;
        return BATTERY_STATUS_OK;
    }

    /* Resistance below lowest entry → clamp to hottest temperature */
    if (resistance_ohm <= lut->entries[lut->count - 1].resistance_ohm) {
        *temp_c_x100 = lut->entries[lut->count - 1].temp_c_x100;
        return BATTERY_STATUS_OK;
    }

    /*
     * Find bracketing entries.
     * Table sorted descending by resistance: entries[i].R > entries[i+1].R
     * Upper = higher resistance (colder), Lower = lower resistance (warmer)
     */
    for (size_t i = 0; i < lut->count - 1; i++) {
        upper = &lut->entries[i];
        lower = &lut->entries[i + 1];

        if (resistance_ohm <= upper->resistance_ohm &&
            resistance_ohm >= lower->resistance_ohm) {
            /*
             * Linear interpolation between two entries.
             *
             * Fraction = (upper_R - measured_R) / (upper_R - lower_R)
             * Temp = upper_T + fraction * (lower_T - upper_T)
             *
             * Note: lower_T > upper_T (lower resistance = warmer).
             * All math uses int64_t to handle both negative temps
             * and large resistance values safely.
             */
            int64_t dr_total = (int64_t)upper->resistance_ohm -
                               (int64_t)lower->resistance_ohm;
            int64_t dr_from_upper = (int64_t)upper->resistance_ohm -
                                    (int64_t)resistance_ohm;
            int64_t dt = (int64_t)lower->temp_c_x100 -
                         (int64_t)upper->temp_c_x100;

            if (dr_total == 0) {
                *temp_c_x100 = upper->temp_c_x100;
                return BATTERY_STATUS_OK;
            }

            *temp_c_x100 = upper->temp_c_x100 +
                           (int32_t)((dr_from_upper * dt) / dr_total);
            return BATTERY_STATUS_OK;
        }
    }

    /* Should never reach here given the clamp checks above */
    return BATTERY_STATUS_ERROR;
}
