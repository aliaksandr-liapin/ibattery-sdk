#include <battery_sdk/battery_status.h>
#include <stdint.h>
#include <stddef.h>

static int g_mock_temp_rc = BATTERY_STATUS_OK;
static int32_t g_mock_temp_c_x100 = 2500;   /* 25.00 C */

void mock_temperature_set_rc(int rc) { g_mock_temp_rc = rc; }
void mock_temperature_set_c_x100(int32_t val) { g_mock_temp_c_x100 = val; }

int battery_temperature_init(void) { return BATTERY_STATUS_OK; }

int battery_temperature_get_c_x100(int32_t *temperature_c_x100)
{
    if (g_mock_temp_rc != BATTERY_STATUS_OK) return g_mock_temp_rc;
    if (temperature_c_x100) *temperature_c_x100 = g_mock_temp_c_x100;
    return BATTERY_STATUS_OK;
}
