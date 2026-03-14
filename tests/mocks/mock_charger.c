#include <battery_sdk/battery_status.h>
#include <stdbool.h>

/* ── Configurable mock state ─────────────────────────────────────────────── */

static int g_mock_charger_init_rc = BATTERY_STATUS_OK;
static int g_mock_charger_rc = BATTERY_STATUS_OK;
static bool g_mock_charging = false;
static bool g_mock_charged  = false;

/* ── Control functions ───────────────────────────────────────────────────── */

void mock_charger_set_init_rc(int rc)   { g_mock_charger_init_rc = rc; }
void mock_charger_set_rc(int rc)        { g_mock_charger_rc = rc; }
void mock_charger_set_charging(bool v)  { g_mock_charging = v; }
void mock_charger_set_charged(bool v)   { g_mock_charged = v; }

/* ── HAL stub implementations ────────────────────────────────────────────── */

int battery_hal_charger_init(void)
{
    return g_mock_charger_init_rc;
}

int battery_hal_charger_is_charging(bool *charging_out)
{
    if (g_mock_charger_rc != BATTERY_STATUS_OK) {
        return g_mock_charger_rc;
    }
    if (charging_out) {
        *charging_out = g_mock_charging;
    }
    return BATTERY_STATUS_OK;
}

int battery_hal_charger_is_charged(bool *charged_out)
{
    if (g_mock_charger_rc != BATTERY_STATUS_OK) {
        return g_mock_charger_rc;
    }
    if (charged_out) {
        *charged_out = g_mock_charged;
    }
    return BATTERY_STATUS_OK;
}
