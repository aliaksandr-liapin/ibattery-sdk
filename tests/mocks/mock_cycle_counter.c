#include <battery_sdk/battery_status.h>
#include <battery_sdk/battery_cycle_counter.h>
#include <stdint.h>
#include <stddef.h>

/* ── Configurable mock state ─────────────────────────────────────────────── */

static int g_mock_cycle_init_rc = BATTERY_STATUS_OK;
static int g_mock_cycle_update_rc = BATTERY_STATUS_OK;
static int g_mock_cycle_get_rc = BATTERY_STATUS_OK;
static uint32_t g_mock_cycle_count = 0;

/* ── Control functions ───────────────────────────────────────────────────── */

void mock_cycle_counter_set_init_rc(int rc)   { g_mock_cycle_init_rc = rc; }
void mock_cycle_counter_set_update_rc(int rc) { g_mock_cycle_update_rc = rc; }
void mock_cycle_counter_set_get_rc(int rc)    { g_mock_cycle_get_rc = rc; }
void mock_cycle_counter_set_count(uint32_t v) { g_mock_cycle_count = v; }

/* ── Stub implementations ────────────────────────────────────────────────── */

int battery_cycle_counter_init(void)
{
    return g_mock_cycle_init_rc;
}

int battery_cycle_counter_update(uint8_t power_state)
{
    (void)power_state;
    return g_mock_cycle_update_rc;
}

int battery_cycle_counter_get(uint32_t *count_out)
{
    if (count_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }
    if (g_mock_cycle_get_rc != BATTERY_STATUS_OK) {
        return g_mock_cycle_get_rc;
    }
    *count_out = g_mock_cycle_count;
    return BATTERY_STATUS_OK;
}
