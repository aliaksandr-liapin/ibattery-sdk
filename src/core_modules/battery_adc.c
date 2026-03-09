#include "battery_adc.h"
#include "../hal/battery_hal.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

int battery_adc_init(void)
{
    return battery_hal_adc_init();
}

int battery_adc_read_mv(uint16_t *voltage_mv_out)
{
    int ret;
    int16_t raw_adc;
    int32_t voltage_mv;

    if (voltage_mv_out == NULL) {
        return -EINVAL;
    }

    ret = battery_hal_adc_read_raw(&raw_adc);
    if (ret != 0) {
        return ret;
    }

    ret = battery_hal_adc_raw_to_pin_mv(raw_adc, &voltage_mv);
    if (ret != 0) {
        return ret;
    }

    if (voltage_mv < 0) {
        return -ERANGE;
    }

    if (voltage_mv > 65535) {
        voltage_mv = 65535;
    }

    *voltage_mv_out = (uint16_t)voltage_mv;
    return 0;
}