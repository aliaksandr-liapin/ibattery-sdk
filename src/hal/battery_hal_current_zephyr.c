/*
 * Current HAL — INA219 via Zephyr sensor API.
 *
 * Wraps the Zephyr built-in INA219 driver (drivers/sensor/ti/ina219).
 * Devicetree node must have compatible = "ti,ina219".
 *
 * Sign convention: positive = discharging, negative = charging.
 * Output units: 0.01 mA (matches SDK x100 convention).
 */

#include <battery_sdk/battery_hal_current.h>
#include <battery_sdk/battery_status.h>

#include <stdbool.h>
#include <stddef.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>

#define INA219_NODE DT_NODELABEL(ina219)

#if !DT_NODE_EXISTS(INA219_NODE)
#error "No ina219 node found in devicetree — add ina219@40 to your overlay"
#endif

static const struct device *g_ina219_dev = DEVICE_DT_GET(INA219_NODE);
static bool g_initialized;

int battery_hal_current_init(void)
{
    if (!device_is_ready(g_ina219_dev)) {
        return BATTERY_STATUS_IO;
    }

    g_initialized = true;
    return BATTERY_STATUS_OK;
}

int battery_hal_current_read_ma_x100(int32_t *current_ma_x100_out)
{
    struct sensor_value val;

    if (current_ma_x100_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    if (!g_initialized) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }

    if (sensor_sample_fetch(g_ina219_dev) < 0) {
        return BATTERY_STATUS_IO;
    }

    if (sensor_channel_get(g_ina219_dev, SENSOR_CHAN_CURRENT, &val) < 0) {
        return BATTERY_STATUS_IO;
    }

    /* sensor_value: val1 = integer amps, val2 = fractional (millionths of A).
     * Convert to 0.01 mA units:
     *   val1 * 1A * 1000mA/A * 100 (x100) = val1 * 100000
     *   val2 * 1uA / 10 = val2 / 10  (uA to 0.01mA)                       */
    *current_ma_x100_out = (val.val1 * 100000) + (val.val2 / 10);

    return BATTERY_STATUS_OK;
}
