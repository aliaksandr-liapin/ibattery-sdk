#ifndef BATTERY_SDK_BATTERY_TYPES_H
#define BATTERY_SDK_BATTERY_TYPES_H

#include <stdint.h>

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

/* Telemetry status_flags bit definitions.
 * A set bit indicates the corresponding reading failed during collection. */
#define BATTERY_TELEMETRY_FLAG_VOLTAGE_ERR     (1U << 0)
#define BATTERY_TELEMETRY_FLAG_TEMP_ERR        (1U << 1)
#define BATTERY_TELEMETRY_FLAG_SOC_ERR         (1U << 2)
#define BATTERY_TELEMETRY_FLAG_POWER_STATE_ERR (1U << 3)
#define BATTERY_TELEMETRY_FLAG_TIMESTAMP_ERR   (1U << 4)

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SDK_BATTERY_TYPES_H */