#include <battery_sdk/battery_status.h>
#include <stdint.h>
#include <stddef.h>

static int g_mock_voltage_rc = BATTERY_STATUS_OK;
static uint16_t g_mock_voltage_mv = 2950;

void mock_voltage_set_rc(int rc) { g_mock_voltage_rc = rc; }
void mock_voltage_set_mv(uint16_t mv) { g_mock_voltage_mv = mv; }

int battery_voltage_init(void) { return BATTERY_STATUS_OK; }

int battery_voltage_get_mv(uint16_t *voltage_mv_out)
{
    if (g_mock_voltage_rc != BATTERY_STATUS_OK) return g_mock_voltage_rc;
    if (voltage_mv_out) *voltage_mv_out = g_mock_voltage_mv;
    return BATTERY_STATUS_OK;
}
