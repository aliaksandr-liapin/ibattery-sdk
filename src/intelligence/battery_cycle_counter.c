/*
 * Charge cycle counter.
 *
 * Detects CHARGING → CHARGED transitions and increments a counter
 * that is persisted to flash via the HAL NVS interface.
 */

#include <battery_sdk/battery_cycle_counter.h>
#include <battery_sdk/battery_status.h>
#include <battery_sdk/battery_types.h>

#include "../hal/battery_hal_nvs.h"

#include <stdbool.h>
#include <stddef.h>

static uint32_t g_cycle_count;
static uint8_t  g_prev_state = BATTERY_POWER_STATE_UNKNOWN;
static bool     g_initialized;

int battery_cycle_counter_init(void)
{
    int rc;

    rc = battery_hal_nvs_init();
    if (rc != BATTERY_STATUS_OK) {
        /* NVS unavailable — start at 0, best-effort */
        g_cycle_count = 0;
        g_initialized = true;
        return BATTERY_STATUS_OK;
    }

    rc = battery_hal_nvs_read_u32(BATTERY_NVS_KEY_CYCLE_COUNT, &g_cycle_count);
    if (rc != BATTERY_STATUS_OK) {
        /* No stored value — first boot */
        g_cycle_count = 0;
    }

    g_prev_state = BATTERY_POWER_STATE_UNKNOWN;
    g_initialized = true;
    return BATTERY_STATUS_OK;
}

int battery_cycle_counter_update(uint8_t power_state)
{
    if (!g_initialized) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }

    /* Detect CHARGING → CHARGED transition */
    if (g_prev_state == BATTERY_POWER_STATE_CHARGING &&
        power_state == BATTERY_POWER_STATE_CHARGED) {
        g_cycle_count++;
        /* Best-effort persist — charge cycles are infrequent so flash
         * wear is not a concern here. */
        (void)battery_hal_nvs_write_u32(BATTERY_NVS_KEY_CYCLE_COUNT,
                                         g_cycle_count);
    }

    g_prev_state = power_state;
    return BATTERY_STATUS_OK;
}

int battery_cycle_counter_get(uint32_t *count_out)
{
    if (count_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    if (!g_initialized) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }

    *count_out = g_cycle_count;
    return BATTERY_STATUS_OK;
}
