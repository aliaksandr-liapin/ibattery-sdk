#include <stddef.h>
#include <stdint.h>

#include <zephyr/kernel.h>

#include "battery_hal.h"

int battery_hal_init(void)
{
    return battery_hal_adc_init();
}

int battery_hal_get_uptime_ms(uint32_t *uptime_ms_out)
{
    if (uptime_ms_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    *uptime_ms_out = (uint32_t)k_uptime_get();
    return BATTERY_STATUS_OK;
}
