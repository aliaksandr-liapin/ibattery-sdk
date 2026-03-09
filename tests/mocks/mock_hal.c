#include <battery_sdk/battery_status.h>
#include <stdint.h>

/* ── Configurable mock state ─────────────────────────────────────────────── */

static int g_mock_hal_init_rc = BATTERY_STATUS_OK;
static int g_mock_uptime_rc = BATTERY_STATUS_OK;
static uint32_t g_mock_uptime_ms = 4000;
static int g_mock_adc_read_rc = BATTERY_STATUS_OK;
static int16_t g_mock_adc_raw = 2048;
static int g_mock_adc_to_mv_rc = BATTERY_STATUS_OK;
static int32_t g_mock_adc_mv = 3000;

/* ── Control functions ───────────────────────────────────────────────────── */

void mock_hal_set_init_rc(int rc) { g_mock_hal_init_rc = rc; }
void mock_hal_set_uptime_rc(int rc) { g_mock_uptime_rc = rc; }
void mock_hal_set_uptime_ms(uint32_t ms) { g_mock_uptime_ms = ms; }
void mock_hal_set_adc_read_rc(int rc) { g_mock_adc_read_rc = rc; }
void mock_hal_set_adc_raw(int16_t raw) { g_mock_adc_raw = raw; }
void mock_hal_set_adc_to_mv_rc(int rc) { g_mock_adc_to_mv_rc = rc; }
void mock_hal_set_adc_mv(int32_t mv) { g_mock_adc_mv = mv; }

/* ── HAL stub implementations ────────────────────────────────────────────── */

int battery_hal_init(void) { return g_mock_hal_init_rc; }
int battery_hal_adc_init(void) { return BATTERY_STATUS_OK; }

int battery_hal_get_uptime_ms(uint32_t *uptime_ms_out)
{
    if (g_mock_uptime_rc != BATTERY_STATUS_OK) return g_mock_uptime_rc;
    if (uptime_ms_out) *uptime_ms_out = g_mock_uptime_ms;
    return BATTERY_STATUS_OK;
}

int battery_hal_adc_read_raw(int16_t *raw_out)
{
    if (g_mock_adc_read_rc != BATTERY_STATUS_OK) return g_mock_adc_read_rc;
    if (raw_out) *raw_out = g_mock_adc_raw;
    return BATTERY_STATUS_OK;
}

int battery_hal_adc_raw_to_pin_mv(int16_t raw, int32_t *mv_out)
{
    (void)raw;
    if (g_mock_adc_to_mv_rc != BATTERY_STATUS_OK) return g_mock_adc_to_mv_rc;
    if (mv_out) *mv_out = g_mock_adc_mv;
    return BATTERY_STATUS_OK;
}
