#include <stddef.h>
#include <stdint.h>
#include <errno.h>

#include "battery_hal.h"

int battery_adc_init(void)
{
    return battery_hal_adc_init();
}

int battery_adc_read_raw(int16_t *raw_out)
{
    if (raw_out == NULL) {
        return -EINVAL;
    }

    return battery_hal_adc_read_raw(raw_out);
}

int battery_adc_raw_to_pin_mv(int16_t raw, int32_t *mv_out)
{
    if (mv_out == NULL) {
        return -EINVAL;
    }

    return battery_hal_adc_raw_to_pin_mv(raw, mv_out);
}