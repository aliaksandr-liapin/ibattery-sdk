#ifndef BATTERY_NTC_LUT_H
#define BATTERY_NTC_LUT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Single entry in a resistance-to-temperature lookup table.
 * Entries MUST be sorted by resistance_ohm in descending order
 * (highest resistance / lowest temperature first).
 */
typedef struct {
    uint32_t resistance_ohm;
    int32_t  temp_c_x100;     /* Temperature in 0.01 °C units */
} battery_ntc_lut_entry_t;

/**
 * NTC lookup table descriptor.
 */
typedef struct {
    const battery_ntc_lut_entry_t *entries;
    size_t count;
} battery_ntc_lut_t;

/** 10K NTC thermistor, B = 3950 K. */
extern const battery_ntc_lut_t battery_ntc_lut_10k_3950;

/**
 * Compute NTC thermistor resistance from a voltage-divider ADC reading.
 *
 * Circuit assumed:  VDD --- [R_pullup] ---+--- [NTC] --- GND
 *                                         |
 *                                       ADC pin
 *
 * @param pullup_ohm    Fixed pull-up resistor value in ohms.
 * @param vdd_mv        Supply voltage in millivolts.
 * @param adc_mv        ADC reading at the junction in millivolts.
 * @param resistance_out Output: computed NTC resistance in ohms.
 * @return BATTERY_STATUS_OK on success, error code otherwise.
 */
int battery_ntc_resistance_from_mv(uint32_t pullup_ohm,
                                   uint32_t vdd_mv,
                                   uint32_t adc_mv,
                                   uint32_t *resistance_out);

/**
 * Interpolate temperature from a resistance-to-temperature lookup table.
 *
 * Uses linear interpolation between table entries.
 * Clamps to table boundaries (lowest temp above max R, highest temp below min R).
 * Integer math only — no floating point.
 *
 * @param lut             Pointer to the NTC lookup table descriptor.
 * @param resistance_ohm  Measured NTC resistance in ohms.
 * @param temp_c_x100     Output: temperature in 0.01 °C units.
 * @return BATTERY_STATUS_OK on success, error code otherwise.
 */
int battery_ntc_lut_interpolate(const battery_ntc_lut_t *lut,
                                uint32_t resistance_ohm,
                                int32_t *temp_c_x100);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_NTC_LUT_H */
