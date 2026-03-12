#include "../../src/core/battery_internal.h"
#include <battery_sdk/battery_status.h>
#include <stdint.h>

static struct battery_sdk_runtime_state g_mock_state = {
    .adc_initialized = true,
    .voltage_initialized = true,
    .temperature_initialized = true,
    .soc_initialized = true,
    .power_manager_initialized = true,
    .telemetry_initialized = true,
    .transport_initialized = true
};

struct battery_sdk_runtime_state *battery_sdk_state(void)
{
    return &g_mock_state;
}

/* ── SDK-level uptime mock ───────────────────────────────────────────────── */

static int g_mock_sdk_uptime_rc = BATTERY_STATUS_OK;
static uint32_t g_mock_sdk_uptime_ms = 4000;

void mock_sdk_set_uptime_rc(int rc) { g_mock_sdk_uptime_rc = rc; }
void mock_sdk_set_uptime_ms(uint32_t ms) { g_mock_sdk_uptime_ms = ms; }

int battery_sdk_get_uptime_ms(uint32_t *uptime_ms_out)
{
    if (g_mock_sdk_uptime_rc != BATTERY_STATUS_OK) {
        return g_mock_sdk_uptime_rc;
    }
    if (uptime_ms_out) {
        *uptime_ms_out = g_mock_sdk_uptime_ms;
    }
    return BATTERY_STATUS_OK;
}
