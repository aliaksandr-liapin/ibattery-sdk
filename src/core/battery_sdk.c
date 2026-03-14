#include "battery_internal.h"

#include <battery_sdk/battery_sdk.h>
#include <battery_sdk/battery_status.h>
#include <battery_sdk/battery_voltage.h>
#include <battery_sdk/battery_temperature.h>
#include <battery_sdk/battery_power_manager.h>
#include <battery_sdk/battery_soc_estimator.h>
#include <battery_sdk/battery_telemetry.h>

#if __has_include(<autoconf.h>)
#include <autoconf.h>
#endif

#if defined(CONFIG_BATTERY_TRANSPORT)
#include <battery_sdk/battery_transport.h>
#endif

#if defined(CONFIG_BATTERY_CHARGER_TP4056)
#include "../hal/battery_hal_charger.h"
#endif

#include "../hal/battery_hal.h"

static struct battery_sdk_runtime_state g_battery_sdk_state = {
    .adc_initialized = false,
    .voltage_initialized = false,
    .temperature_initialized = false,
    .soc_initialized = false,
    .power_manager_initialized = false,
    .telemetry_initialized = false,
    .transport_initialized = false
};

struct battery_sdk_runtime_state *battery_sdk_state(void)
{
    return &g_battery_sdk_state;
}

int battery_sdk_get_uptime_ms(uint32_t *uptime_ms_out)
{
    return battery_hal_get_uptime_ms(uptime_ms_out);
}

int battery_sdk_init(void)
{
    int rc;

    rc = battery_hal_init();
    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }

    rc = battery_voltage_init();
    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }

    rc = battery_temperature_init();
    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }

#if defined(CONFIG_BATTERY_CHARGER_TP4056)
    rc = battery_hal_charger_init();
    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }
#endif

    rc = battery_power_manager_init();
    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }

    rc = battery_soc_estimator_init();
    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }

    rc = battery_telemetry_init();
    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }

#if defined(CONFIG_BATTERY_TRANSPORT)
    rc = battery_transport_init();
    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }
    g_battery_sdk_state.transport_initialized = true;
#endif

    return BATTERY_STATUS_OK;
}