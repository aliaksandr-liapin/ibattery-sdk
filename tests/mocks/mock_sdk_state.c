#include "../../src/core/battery_internal.h"

static struct battery_sdk_runtime_state g_mock_state = {
    .adc_initialized = true,
    .voltage_initialized = true,
    .temperature_initialized = true,
    .soc_initialized = true,
    .power_manager_initialized = true,
    .telemetry_initialized = true
};

struct battery_sdk_runtime_state *battery_sdk_state(void)
{
    return &g_mock_state;
}
