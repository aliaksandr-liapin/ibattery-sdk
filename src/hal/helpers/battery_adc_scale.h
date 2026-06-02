/*
 * ADC voltage-divider scaling helper.
 *
 * When battery voltage is sensed through an external resistor divider
 * (V_pin = V_batt * R2 / (R1 + R2)), the firmware reconstructs the battery
 * voltage by multiplying the measured pin voltage by the divider ratio
 * (R1 + R2) / R2.  Integer-only; saturates on overflow; a nonsensical ratio
 * (< 1) degrades to a direct (ratio 1) reading rather than zeroing/inverting.
 */
#ifndef BATTERY_ADC_SCALE_H
#define BATTERY_ADC_SCALE_H

#include <stdint.h>

static inline int32_t battery_adc_apply_divider(int32_t pin_mv, int32_t ratio)
{
    if (ratio < 1) {
        return pin_mv;  /* misconfig: degrade to direct reading */
    }

    int64_t scaled = (int64_t)pin_mv * (int64_t)ratio;

    if (scaled > INT32_MAX) {
        return INT32_MAX;
    }
    if (scaled < INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t)scaled;
}

#endif /* BATTERY_ADC_SCALE_H */
