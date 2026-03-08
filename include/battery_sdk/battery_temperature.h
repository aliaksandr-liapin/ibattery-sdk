#ifndef BATTERY_SDK_BATTERY_TEMPERATURE_H
#define BATTERY_SDK_BATTERY_TEMPERATURE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int battery_temperature_init(void);
int battery_temperature_get_c_x100(int32_t *temperature_c_x100);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SDK_BATTERY_TEMPERATURE_H */