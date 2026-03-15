/*
 * Stub NVS HAL — used when CONFIG_BATTERY_CYCLE_COUNTER is off.
 *
 * Returns errors so the cycle counter falls back to in-memory only.
 */

#include "battery_hal_nvs.h"
#include <battery_sdk/battery_status.h>

int battery_hal_nvs_init(void)
{
    return BATTERY_STATUS_ERROR;
}

int battery_hal_nvs_read_u32(uint16_t key, uint32_t *value)
{
    (void)key;
    (void)value;
    return BATTERY_STATUS_ERROR;
}

int battery_hal_nvs_write_u32(uint16_t key, uint32_t value)
{
    (void)key;
    (void)value;
    return BATTERY_STATUS_ERROR;
}
