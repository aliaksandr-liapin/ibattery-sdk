/*
 * HAL abstraction for current measurement.
 *
 * On Zephyr: wraps INA219 via sensor API.
 * Without CONFIG_BATTERY_CURRENT_SENSE: stub returns UNSUPPORTED.
 */

#ifndef BATTERY_SDK_BATTERY_HAL_CURRENT_H
#define BATTERY_SDK_BATTERY_HAL_CURRENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int battery_hal_current_init(void);
int battery_hal_current_read_ma_x100(int32_t *current_ma_x100_out);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SDK_BATTERY_HAL_CURRENT_H */
