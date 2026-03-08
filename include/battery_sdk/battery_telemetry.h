#ifndef BATTERY_SDK_BATTERY_TELEMETRY_H
#define BATTERY_SDK_BATTERY_TELEMETRY_H

#include <battery_sdk/battery_types.h>

#ifdef __cplusplus
extern "C" {
#endif

int battery_telemetry_init(void);
int battery_telemetry_collect(struct battery_telemetry_packet *packet);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SDK_BATTERY_TELEMETRY_H */