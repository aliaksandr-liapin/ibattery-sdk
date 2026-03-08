#include <battery_sdk/battery_voltage.h>
#include <battery_sdk/battery_adc.h>
#include <battery_sdk/battery_status.h>

#include "../core/battery_internal.h"

#define BATTERY_VOLTAGE_STUB_SCALE_NUM 4200
#define BATTERY_VOLTAGE_STUB_SCALE_DEN 4095

int battery_voltage_init(void)
{
    struct battery_sdk_runtime_state *state = battery_sdk_state();
    state->voltage_initialized = true;
    return BATTERY_STATUS_OK;
}

int battery_voltage_get_mv(int32_t *voltage_mv)
{
    int rc;
    int16_t raw_value = 0;
    struct battery_sdk_runtime_state *state = battery_sdk_state();

    if (voltage_mv == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    if (!state->voltage_initialized) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }

    rc = battery_adc_read_raw(&raw_value);
    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }

    *voltage_mv = ((int32_t)raw_value * BATTERY_VOLTAGE_STUB_SCALE_NUM) /
                  BATTERY_VOLTAGE_STUB_SCALE_DEN;

    return BATTERY_STATUS_OK;
}