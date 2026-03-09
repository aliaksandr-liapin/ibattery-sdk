#ifndef BATTERY_ADC_H
#define BATTERY_ADC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int battery_adc_init(void);
int battery_adc_read_mv(uint16_t *voltage_mv_out);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_ADC_H */