#include <battery_sdk/battery_types.h>
#include <battery_sdk/battery_status.h>
#include <stddef.h>

static int g_mock_power_rc = BATTERY_STATUS_OK;
static enum battery_power_state g_mock_power_state = BATTERY_POWER_STATE_ACTIVE;

void mock_power_set_rc(int rc) { g_mock_power_rc = rc; }
void mock_power_set_state(enum battery_power_state s) { g_mock_power_state = s; }

int battery_power_manager_init(void) { return BATTERY_STATUS_OK; }

int battery_power_manager_get_state(enum battery_power_state *state)
{
    if (g_mock_power_rc != BATTERY_STATUS_OK) return g_mock_power_rc;
    if (state) *state = g_mock_power_state;
    return BATTERY_STATUS_OK;
}
