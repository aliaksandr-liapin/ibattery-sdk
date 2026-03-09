#include <battery_sdk/battery_status.h>
#include <stdint.h>
#include <stddef.h>

static int g_mock_soc_rc = BATTERY_STATUS_OK;
static uint16_t g_mock_soc_pct_x100 = 8500;   /* 85.00% */

void mock_soc_set_rc(int rc) { g_mock_soc_rc = rc; }
void mock_soc_set_pct_x100(uint16_t val) { g_mock_soc_pct_x100 = val; }

int battery_soc_estimator_init(void) { return BATTERY_STATUS_OK; }

int battery_soc_estimator_get_pct_x100(uint16_t *soc_pct_x100)
{
    if (g_mock_soc_rc != BATTERY_STATUS_OK) return g_mock_soc_rc;
    if (soc_pct_x100) *soc_pct_x100 = g_mock_soc_pct_x100;
    return BATTERY_STATUS_OK;
}
