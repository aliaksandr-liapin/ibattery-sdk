#include <battery_sdk/battery_status.h>
#include <stdint.h>
#include <stdbool.h>

/* ── Configurable mock state ─────────────────────────────────────────────── */

static int g_mock_nvs_init_rc = BATTERY_STATUS_OK;
static int g_mock_nvs_read_rc = BATTERY_STATUS_ERROR;  /* not-found by default */
static int g_mock_nvs_write_rc = BATTERY_STATUS_OK;
static uint32_t g_mock_nvs_stored_value = 0;
static bool g_mock_nvs_has_value = false;

/* ── Control functions ───────────────────────────────────────────────────── */

void mock_nvs_set_init_rc(int rc)    { g_mock_nvs_init_rc = rc; }
void mock_nvs_set_read_rc(int rc)    { g_mock_nvs_read_rc = rc; }
void mock_nvs_set_write_rc(int rc)   { g_mock_nvs_write_rc = rc; }
void mock_nvs_set_stored_value(uint32_t v)
{
    g_mock_nvs_stored_value = v;
    g_mock_nvs_has_value = true;
    g_mock_nvs_read_rc = BATTERY_STATUS_OK;
}

void mock_nvs_reset(void)
{
    g_mock_nvs_init_rc = BATTERY_STATUS_OK;
    g_mock_nvs_read_rc = BATTERY_STATUS_ERROR;
    g_mock_nvs_write_rc = BATTERY_STATUS_OK;
    g_mock_nvs_stored_value = 0;
    g_mock_nvs_has_value = false;
}

uint32_t mock_nvs_get_last_written(void)
{
    return g_mock_nvs_stored_value;
}

/* ── HAL stub implementations ────────────────────────────────────────────── */

int battery_hal_nvs_init(void)
{
    return g_mock_nvs_init_rc;
}

int battery_hal_nvs_read_u32(uint16_t key, uint32_t *value)
{
    (void)key;
    if (g_mock_nvs_read_rc != BATTERY_STATUS_OK) {
        return g_mock_nvs_read_rc;
    }
    if (value && g_mock_nvs_has_value) {
        *value = g_mock_nvs_stored_value;
    }
    return g_mock_nvs_has_value ? BATTERY_STATUS_OK : BATTERY_STATUS_ERROR;
}

int battery_hal_nvs_write_u32(uint16_t key, uint32_t value)
{
    (void)key;
    if (g_mock_nvs_write_rc != BATTERY_STATUS_OK) {
        return g_mock_nvs_write_rc;
    }
    g_mock_nvs_stored_value = value;
    g_mock_nvs_has_value = true;
    return BATTERY_STATUS_OK;
}
