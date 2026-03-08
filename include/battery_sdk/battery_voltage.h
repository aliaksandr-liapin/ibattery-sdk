#ifndef BATTERY_SDK_BATTERY_VOLTAGE_H
#define BATTERY_SDK_BATTERY_VOLTAGE_H

#include <stddef.h>
#include <stdint.h>
#include <errno.h>

#include "battery_sdk/battery_voltage.h"

#ifdef __cplusplus
extern "C" {
#endif

int battery_voltage_init(void);
int battery_voltage_get_mv(int32_t *voltage_mv_out);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SDK_BATTERY_VOLTAGE_H */