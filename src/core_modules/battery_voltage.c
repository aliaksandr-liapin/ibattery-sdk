#include <stddef.h>
#include <stdint.h>
#include <errno.h>

#include "battery_sdk/battery_voltage.h"

/* Internal module functions */
int battery_adc_init(void);
int battery_adc_read_raw(int16_t *raw_out);
int battery_adc_raw_to_pin_mv(int16_t raw, int32_t *mv_out);

#define BATTERY_DIVIDER_R_TOP_OHMS     1000000
#define BATTERY_DIVIDER_R_BOTTOM_OHMS  1000000

int battery_voltage_init(void)
{
    return battery_adc_init();
}

int battery_voltage_get_mv(int32_t *voltage_mv_out)
{
    int ret;
    int16_t raw_adc;
    int32_t pin_mv;
    int64_t battery_mv;

    if (voltage_mv_out == NULL) {
        return -EINVAL;
    }

    ret = battery_adc_read_raw(&raw_adc);
    if (ret < 0) {
        return ret;
    }

    ret = battery_adc_raw_to_pin_mv(raw_adc, &pin_mv);
    if (ret < 0) {
        return ret;
    }

    battery_mv =
        ((int64_t)pin_mv *
         (BATTERY_DIVIDER_R_TOP_OHMS + BATTERY_DIVIDER_R_BOTTOM_OHMS)) /
        BATTERY_DIVIDER_R_BOTTOM_OHMS;

    *voltage_mv_out = (int32_t)battery_mv;
    return 0;
}