#include <stddef.h>

#include <battery_sdk/battery_power_manager.h>
#include <battery_sdk/battery_status.h>

#include "../core/battery_internal.h"

int battery_power_manager_init(void)
{
    struct battery_sdk_runtime_state *state = battery_sdk_state();
    state->power_manager_initialized = true;
    return BATTERY_STATUS_OK;
}

int battery_power_manager_get_state(enum battery_power_state *state_out)
{
    struct battery_sdk_runtime_state *state = battery_sdk_state();

    if (state_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    if (!state->power_manager_initialized) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }

    *state_out = BATTERY_POWER_STATE_ACTIVE;
    return BATTERY_STATUS_OK;
}