#include <stddef.h>

#include <battery_sdk/battery_power_manager.h>
#include <battery_sdk/battery_voltage.h>
#include <battery_sdk/battery_status.h>

#include "../core/battery_internal.h"

/* Voltage thresholds for state transitions (millivolts).
 * Hysteresis band: drop below CRITICAL_ENTER to enter CRITICAL,
 * rise above CRITICAL_EXIT to return to ACTIVE.                 */
#define BATTERY_POWER_CRITICAL_ENTER_MV  2100
#define BATTERY_POWER_CRITICAL_EXIT_MV   2200

static enum battery_power_state g_current_state = BATTERY_POWER_STATE_UNKNOWN;

int battery_power_manager_init(void)
{
    struct battery_sdk_runtime_state *state = battery_sdk_state();

    g_current_state = BATTERY_POWER_STATE_ACTIVE;
    state->power_manager_initialized = true;
    return BATTERY_STATUS_OK;
}

int battery_power_manager_get_state(enum battery_power_state *state_out)
{
    struct battery_sdk_runtime_state *state = battery_sdk_state();
    uint16_t voltage_mv;
    int rc;

    if (state_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    if (!state->power_manager_initialized) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }

    rc = battery_voltage_get_mv(&voltage_mv);
    if (rc != BATTERY_STATUS_OK) {
        /* Cannot determine state — return last known */
        *state_out = g_current_state;
        return BATTERY_STATUS_OK;
    }

    if (g_current_state == BATTERY_POWER_STATE_CRITICAL) {
        /* Exit CRITICAL only if voltage rises above exit threshold */
        if (voltage_mv > BATTERY_POWER_CRITICAL_EXIT_MV) {
            g_current_state = BATTERY_POWER_STATE_ACTIVE;
        }
    } else {
        /* Enter CRITICAL if voltage drops below enter threshold */
        if (voltage_mv < BATTERY_POWER_CRITICAL_ENTER_MV) {
            g_current_state = BATTERY_POWER_STATE_CRITICAL;
        }
    }

    *state_out = g_current_state;
    return BATTERY_STATUS_OK;
}
