/*
 * Temperature HAL — nRF52840 die temperature sensor via Zephyr sensor API.
 *
 * Uses the on-chip TEMP peripheral (±2 °C accuracy).  The HAL abstraction
 * allows a future swap to an external NTC thermistor without touching the
 * temperature module above.
 */

#include "battery_hal.h"

#include <stdint.h>
#include <stddef.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>

#define BATTERY_TEMP_NODE DT_NODELABEL(temp)

#if !DT_NODE_EXISTS(BATTERY_TEMP_NODE)
#error "Temperature sensor node 'temp' is missing in devicetree"
#endif

static const struct device *g_temp_dev = DEVICE_DT_GET(BATTERY_TEMP_NODE);

int battery_hal_temp_init(void)
{
    if (!device_is_ready(g_temp_dev)) {
        return BATTERY_STATUS_IO;
    }
    return BATTERY_STATUS_OK;
}

int battery_hal_temp_read_c_x100(int32_t *temp_c_x100_out)
{
    struct sensor_value val;

    if (temp_c_x100_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    if (sensor_sample_fetch(g_temp_dev) < 0) {
        return BATTERY_STATUS_IO;
    }

    if (sensor_channel_get(g_temp_dev, SENSOR_CHAN_DIE_TEMP, &val) < 0) {
        return BATTERY_STATUS_IO;
    }

    /* sensor_value: val1 = integer degrees C, val2 = fractional (millionths).
     * Convert to 0.01 °C units: val1 * 100 + val2 / 10000              */
    *temp_c_x100_out = (val.val1 * 100) + (val.val2 / 10000);
    return BATTERY_STATUS_OK;
}
