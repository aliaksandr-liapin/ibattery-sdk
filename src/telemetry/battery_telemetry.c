#include <stddef.h>

#include <battery_sdk/battery_telemetry.h>
#include <battery_sdk/battery_voltage.h>
#include <battery_sdk/battery_temperature.h>
#include <battery_sdk/battery_soc_estimator.h>
#include <battery_sdk/battery_power_manager.h>
#include <battery_sdk/battery_status.h>

#include "../core/battery_internal.h"
#include "../hal/battery_hal.h"

int battery_telemetry_init(void)
{
    struct battery_sdk_runtime_state *state = battery_sdk_state();
    state->telemetry_initialized = true;
    return BATTERY_STATUS_OK;
}

int battery_telemetry_collect(struct battery_telemetry_packet *packet)
{
    int rc;
    uint32_t uptime_ms = 0;
    enum battery_power_state power_state = BATTERY_POWER_STATE_UNKNOWN;
    struct battery_sdk_runtime_state *state = battery_sdk_state();

    if (packet == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    if (!state->telemetry_initialized) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }

    rc = battery_hal_get_uptime_ms(&uptime_ms);
    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }

    rc = battery_voltage_get_mv(&packet->voltage_mv);
    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }

    rc = battery_temperature_get_c_x100(&packet->temperature_c_x100);
    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }

    rc = battery_soc_estimator_get_pct_x100(&packet->soc_pct_x100);
    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }

    rc = battery_power_manager_get_state(&power_state);
    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }

    packet->telemetry_version = BATTERY_TELEMETRY_VERSION;
    packet->timestamp_ms = uptime_ms;
    packet->power_state = (uint8_t)power_state;
    packet->status_flags = 0U;

    return BATTERY_STATUS_OK;
}