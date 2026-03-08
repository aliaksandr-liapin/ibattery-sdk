#include "battery_hal.h"

#include <zephyr/kernel.h>
#include <battery_sdk/battery_status.h>

int battery_hal_init(void)
{
    return BATTERY_STATUS_OK;
}

int battery_hal_adc_read_raw(int16_t *raw_value)
{
    if (raw_value == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    /* Phase 1 skeleton stub:
     * Replace with Zephyr ADC driver integration in next implementation step.
     */
    *raw_value = 2048;
    return BATTERY_STATUS_OK;
}

int battery_hal_get_uptime_ms(uint32_t *uptime_ms)
{
    if (uptime_ms == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    *uptime_ms = (uint32_t)k_uptime_get_32();
    return BATTERY_STATUS_OK;
}