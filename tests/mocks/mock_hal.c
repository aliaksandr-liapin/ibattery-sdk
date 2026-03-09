#include <battery_sdk/battery_status.h>
#include <stdint.h>

/* Configurable mock state */
static int g_mock_hal_init_rc = BATTERY_STATUS_OK;
static int g_mock_uptime_rc = BATTERY_STATUS_OK;
static uint32_t g_mock_uptime_ms = 4000;

void mock_hal_set_init_rc(int rc) { g_mock_hal_init_rc = rc; }
void mock_hal_set_uptime_rc(int rc) { g_mock_uptime_rc = rc; }
void mock_hal_set_uptime_ms(uint32_t ms) { g_mock_uptime_ms = ms; }

int battery_hal_init(void) { return g_mock_hal_init_rc; }
int battery_hal_adc_init(void) { return BATTERY_STATUS_OK; }

int battery_hal_get_uptime_ms(uint32_t *uptime_ms_out)
{
    if (g_mock_uptime_rc != BATTERY_STATUS_OK) return g_mock_uptime_rc;
    if (uptime_ms_out) *uptime_ms_out = g_mock_uptime_ms;
    return BATTERY_STATUS_OK;
}
