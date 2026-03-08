#ifndef BATTERY_SDK_BATTERY_ADC_H
#define BATTERY_SDK_BATTERY_ADC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int battery_adc_init(void);
int battery_adc_read_raw(int16_t *raw_value);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SDK_BATTERY_ADC_H */