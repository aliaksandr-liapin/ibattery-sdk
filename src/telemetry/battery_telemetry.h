#ifndef BATTERY_TELEMETRY_H
#define BATTERY_TELEMETRY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    int32_t voltage_mv;
    int32_t temperature_c_x100;
    uint16_t soc_pct_x100;
} battery_telemetry_packet_t;

int battery_telemetry_collect(battery_telemetry_packet_t *packet);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_TELEMETRY_H */