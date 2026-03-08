#ifndef BATTERY_HAL_H
#define BATTERY_HAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int battery_hal_init(void);
int battery_hal_adc_read_raw(int16_t *raw_value);
int battery_hal_get_uptime_ms(uint32_t *uptime_ms);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_HAL_H */