#ifndef BATTERY_SDK_BATTERY_TYPES_H
#define BATTERY_SDK_BATTERY_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BATTERY_TELEMETRY_VERSION 1U

enum battery_power_state {
    BATTERY_POWER_STATE_UNKNOWN = 0,
    BATTERY_POWER_STATE_ACTIVE,
    BATTERY_POWER_STATE_IDLE,
    BATTERY_POWER_STATE_SLEEP,
    BATTERY_POWER_STATE_CRITICAL
};

struct battery_telemetry_packet {
    uint8_t telemetry_version;
    uint32_t timestamp_ms;

    int32_t voltage_mv;
    int32_t temperature_c_x100;
    uint16_t soc_pct_x100;

    uint8_t power_state;
    uint32_t status_flags;
};

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SDK_BATTERY_TYPES_H */