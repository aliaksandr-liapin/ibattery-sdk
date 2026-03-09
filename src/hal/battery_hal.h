#ifndef BATTERY_HAL_H
#define BATTERY_HAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int battery_hal_adc_init(void);
int battery_hal_adc_read_raw(int16_t *raw_out);
int battery_hal_adc_raw_to_pin_mv(int16_t raw, int32_t *mv_out);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_HAL_H */