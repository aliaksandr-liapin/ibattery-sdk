#include <stdint.h>

#include <zephyr/kernel.h>

#include "battery_hal.h"

int battery_hal_init(void)
{
    int rc;

    rc = battery_hal_adc_init();
    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }

    rc = battery_hal_temp_init();
    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }

    return BATTERY_STATUS_OK;
}

int battery_hal_get_uptime_ms(uint32_t *uptime_ms_out)
{
    if (uptime_ms_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    *uptime_ms_out = (uint32_t)k_uptime_get();
    return BATTERY_STATUS_OK;
}
