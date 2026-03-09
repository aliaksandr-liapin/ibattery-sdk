#include <battery_sdk/battery_voltage.h>
#include "battery_adc.h"
#include "battery_voltage_filter.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static battery_voltage_filter_t g_voltage_filter;
static bool g_voltage_initialized = false;

int battery_voltage_init(void)
{
    int ret;

    ret = battery_adc_init();
    if (ret != 0) {
        return ret;
    }

    ret = battery_voltage_filter_init(&g_voltage_filter,
                                      BATTERY_VOLTAGE_FILTER_DEFAULT_WINDOW_SIZE);
    if (ret != 0) {
        return ret;
    }

    g_voltage_initialized = true;
    return 0;
}

int battery_voltage_get_mv(uint16_t *voltage_mv_out)
{
    int ret;
    uint16_t raw_voltage_mv;
    uint16_t filtered_voltage_mv;

    if (voltage_mv_out == NULL) {
        return -EINVAL;
    }

    if (!g_voltage_initialized) {
        ret = battery_voltage_init();
        if (ret != 0) {
            return ret;
        }
    }

    ret = battery_adc_read_mv(&raw_voltage_mv);
    if (ret != 0) {
        return ret;
    }

    ret = battery_voltage_filter_update(&g_voltage_filter,
                                        raw_voltage_mv,
                                        &filtered_voltage_mv);
    if (ret != 0) {
        return ret;
    }

    *voltage_mv_out = filtered_voltage_mv;
    return 0;
}