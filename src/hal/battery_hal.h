#ifndef BATTERY_HAL_H
#define BATTERY_HAL_H

#include <stdint.h>
#include <battery_sdk/battery_status.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Platform initialization */
int battery_hal_init(void);
int battery_hal_get_uptime_ms(uint32_t *uptime_ms_out);

/* ADC subsystem */
int battery_hal_adc_init(void);
int battery_hal_adc_read_raw(int16_t *raw_out);
int battery_hal_adc_raw_to_pin_mv(int16_t raw, int32_t *mv_out);

/* Temperature subsystem */
int battery_hal_temp_init(void);
int battery_hal_temp_read_c_x100(int32_t *temp_c_x100_out);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_HAL_H */