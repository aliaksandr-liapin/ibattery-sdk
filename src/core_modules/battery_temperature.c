#include <stddef.h>

#include <battery_sdk/battery_temperature.h>
#include <battery_sdk/battery_status.h>

#include "../core/battery_internal.h"
#include "../hal/battery_hal.h"

int battery_temperature_init(void)
{
    struct battery_sdk_runtime_state *state = battery_sdk_state();
    int rc;

    rc = battery_hal_temp_init();
    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }

    state->temperature_initialized = true;
    return BATTERY_STATUS_OK;
}

int battery_temperature_get_c_x100(int32_t *temperature_c_x100)
{
    struct battery_sdk_runtime_state *state = battery_sdk_state();

    if (temperature_c_x100 == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    if (!state->temperature_initialized) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }

    return battery_hal_temp_read_c_x100(temperature_c_x100);
}
