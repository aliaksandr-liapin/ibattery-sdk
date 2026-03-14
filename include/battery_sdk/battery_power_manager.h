#ifndef BATTERY_SDK_BATTERY_POWER_MANAGER_H
#define BATTERY_SDK_BATTERY_POWER_MANAGER_H

#include <battery_sdk/battery_types.h>

#ifdef __cplusplus
extern "C" {
#endif

int battery_power_manager_init(void);
int battery_power_manager_get_state(enum battery_power_state *state);

/** Reset the inactivity timer.  Call on BLE connection, user activity, etc. */
int battery_power_manager_report_activity(void);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SDK_BATTERY_POWER_MANAGER_H */