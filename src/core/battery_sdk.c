#include "battery_internal.h"

static struct battery_sdk_runtime_state g_battery_sdk_state = {
    .adc_initialized = false,
    .voltage_initialized = false,
    .temperature_initialized = false,
    .soc_initialized = false,
    .power_manager_initialized = false,
    .telemetry_initialized = false
};

struct battery_sdk_runtime_state *battery_sdk_state(void)
{
    return &g_battery_sdk_state;
}