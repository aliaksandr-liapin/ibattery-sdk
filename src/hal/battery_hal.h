#ifndef BATTERY_HAL_H
#define BATTERY_HAL_H

#include <stdint.h>

int battery_hal_init(void);
int battery_hal_get_uptime_ms(uint32_t *uptime_ms_out);

int battery_hal_adc_init(void);
int battery_hal_adc_read_raw(int16_t *raw_out);
int battery_hal_adc_raw_to_pin_mv(int16_t raw, int32_t *mv_out);

#endif