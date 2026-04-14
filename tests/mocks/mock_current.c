#include <battery_sdk/battery_hal_current.h>
#include <battery_sdk/battery_status.h>
#include <stdbool.h>

static int g_mock_current_init_rc = BATTERY_STATUS_OK;
static int g_mock_current_read_rc = BATTERY_STATUS_OK;
static int32_t g_mock_current_value = 0;

void mock_current_set_init_rc(int rc) { g_mock_current_init_rc = rc; }
void mock_current_set_read_rc(int rc) { g_mock_current_read_rc = rc; }
void mock_current_set_value(int32_t v) { g_mock_current_value = v; }

void mock_current_reset(void)
{
    g_mock_current_init_rc = BATTERY_STATUS_OK;
    g_mock_current_read_rc = BATTERY_STATUS_OK;
    g_mock_current_value = 0;
}

int battery_hal_current_init(void) { return g_mock_current_init_rc; }

int battery_hal_current_read_ma_x100(int32_t *current_ma_x100_out)
{
    if (current_ma_x100_out == NULL) return BATTERY_STATUS_INVALID_ARG;
    if (g_mock_current_read_rc != BATTERY_STATUS_OK) return g_mock_current_read_rc;
    *current_ma_x100_out = g_mock_current_value;
    return BATTERY_STATUS_OK;
}
