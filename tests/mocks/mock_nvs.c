#include <battery_sdk/battery_status.h>
#include <stdint.h>
#include <stdbool.h>

/* ── Key-aware mock NVS ───────────────────────────────────────────────────
 * A tiny fixed key->value map. Flash survives a simulated "reboot" (the
 * stored map is only cleared by mock_nvs_reset()); RAM does not (the unit
 * under test re-runs its init()). Back-compat: mock_nvs_set_stored_value()
 * sets a wildcard fallback returned for any key with no explicit entry, so
 * the existing single-key suites keep working unchanged.
 */

#define MOCK_NVS_MAX_KEYS 8

static struct { uint16_t key; uint32_t value; bool present; }
    g_entries[MOCK_NVS_MAX_KEYS];

static int      g_init_rc       = BATTERY_STATUS_OK;
static int      g_read_rc       = BATTERY_STATUS_OK;   /* forced read error if != OK */
static int      g_write_rc      = BATTERY_STATUS_OK;
static uint32_t g_legacy_value;
static bool     g_legacy_present;
static uint32_t g_last_written;

static int find_slot(uint16_t key)
{
    for (int i = 0; i < MOCK_NVS_MAX_KEYS; i++) {
        if (g_entries[i].present && g_entries[i].key == key) return i;
    }
    return -1;
}

static int alloc_slot(uint16_t key)
{
    int i = find_slot(key);
    if (i >= 0) return i;
    for (i = 0; i < MOCK_NVS_MAX_KEYS; i++) {
        if (!g_entries[i].present) { g_entries[i].key = key; return i; }
    }
    return -1;
}

/* ── Control functions ───────────────────────────────────────────────────── */

void mock_nvs_set_init_rc(int rc)  { g_init_rc = rc; }
void mock_nvs_set_read_rc(int rc)  { g_read_rc = rc; }
void mock_nvs_set_write_rc(int rc) { g_write_rc = rc; }

/* Legacy single-value helper: wildcard fallback for any key. */
void mock_nvs_set_stored_value(uint32_t v)
{
    g_legacy_value = v;
    g_legacy_present = true;
    g_read_rc = BATTERY_STATUS_OK;
}

/* Key-aware helpers (new). */
void mock_nvs_set_stored_value_key(uint16_t key, uint32_t v)
{
    int i = alloc_slot(key);
    if (i >= 0) { g_entries[i].value = v; g_entries[i].present = true; }
}

bool mock_nvs_get_value_key(uint16_t key, uint32_t *out)
{
    int i = find_slot(key);
    if (i < 0) return false;
    if (out) *out = g_entries[i].value;
    return true;
}

uint32_t mock_nvs_get_last_written(void) { return g_last_written; }

void mock_nvs_reset(void)
{
    for (int i = 0; i < MOCK_NVS_MAX_KEYS; i++) {
        g_entries[i].present = false;
        g_entries[i].key = 0;
        g_entries[i].value = 0;
    }
    g_init_rc = BATTERY_STATUS_OK;
    g_read_rc = BATTERY_STATUS_OK;
    g_write_rc = BATTERY_STATUS_OK;
    g_legacy_value = 0;
    g_legacy_present = false;
    g_last_written = 0;
}

/* ── HAL stub implementations ────────────────────────────────────────────── */

int battery_hal_nvs_init(void) { return g_init_rc; }

int battery_hal_nvs_read_u32(uint16_t key, uint32_t *value)
{
    if (g_read_rc != BATTERY_STATUS_OK) return g_read_rc;
    int i = find_slot(key);
    if (i >= 0) { if (value) *value = g_entries[i].value; return BATTERY_STATUS_OK; }
    if (g_legacy_present) { if (value) *value = g_legacy_value; return BATTERY_STATUS_OK; }
    return BATTERY_STATUS_ERROR;  /* not found */
}

int battery_hal_nvs_write_u32(uint16_t key, uint32_t value)
{
    if (g_write_rc != BATTERY_STATUS_OK) return g_write_rc;
    int i = alloc_slot(key);
    if (i < 0) return BATTERY_STATUS_ERROR;
    g_entries[i].value = value;
    g_entries[i].present = true;
    g_last_written = value;
    return BATTERY_STATUS_OK;
}
