#include <stddef.h>

#include <battery_sdk/battery_temperature.h>
#include <battery_sdk/battery_status.h>

#include "../core/battery_internal.h"

int battery_temperature_init(void)
{
    struct battery_sdk_runtime_state *state = battery_sdk_state();
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

    /* Stub fixed temperature: 25.00 C */
    *temperature_c_x100 = 2500;
    return BATTERY_STATUS_OK;
}