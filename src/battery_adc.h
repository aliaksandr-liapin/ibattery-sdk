#ifndef BATTERY_ADC_H
#define BATTERY_ADC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Placeholder battery ADC interface for Phase 0.
 * In the next step, these stubs can be connected to SAADC.
 */

int battery_adc_init(void);
int battery_adc_read_mv(void);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_ADC_H */
