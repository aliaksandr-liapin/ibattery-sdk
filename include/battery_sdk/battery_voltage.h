#ifndef BATTERY_SDK_BATTERY_VOLTAGE_H
#define BATTERY_SDK_BATTERY_VOLTAGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int battery_voltage_init(void);
int battery_voltage_get_mv(uint16_t *voltage_mv_out);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SDK_BATTERY_VOLTAGE_H */