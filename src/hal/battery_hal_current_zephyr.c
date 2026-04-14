/*
 * Current HAL — INA219 via Zephyr sensor API with raw I2C fallback.
 *
 * Primary path: Zephyr built-in INA219 driver (sensor API).
 * Fallback: raw I2C register access if Zephyr driver fails at boot
 * (common on ESP32-C3 where I2C may not be stable during kernel init).
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
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define INA219_NODE DT_NODELABEL(ina219)

#if !DT_NODE_EXISTS(INA219_NODE)
#error "No ina219 node found in devicetree — add ina219@40 to your overlay"
#endif

#define INA219_I2C_ADDR  0x40
#define INA219_REG_SHUNT 0x01

static const struct device *g_ina219_dev = DEVICE_DT_GET(INA219_NODE);
static const struct device *g_i2c_dev;
static bool g_initialized;
static bool g_use_raw_i2c;

/**
 * Try raw I2C init as fallback when Zephyr sensor driver fails at boot.
 * Configures the INA219 via direct register writes and reads shunt
 * voltage register for current measurement.
 */
static int raw_i2c_init(void)
{
    const struct device *i2c_dev = DEVICE_DT_GET(DT_BUS(INA219_NODE));

    if (!device_is_ready(i2c_dev)) {
        return BATTERY_STATUS_IO;
    }

    /* Verify chip responds: read shunt voltage register */
    uint8_t reg = INA219_REG_SHUNT;
    uint8_t rbuf[2];
    int rc = i2c_write_read(i2c_dev, INA219_I2C_ADDR, &reg, 1, rbuf, 2);
    if (rc != 0) {
        return BATTERY_STATUS_IO;
    }

    /* Try calibration: reg 0x05 = 0x1000 (4096)
     * For 0.1 ohm shunt, 100 uA LSB */
    (void)i2c_burst_write(i2c_dev, INA219_I2C_ADDR, 0x05,
                           (uint8_t[]){0x10, 0x00}, 2);

    /* Try config: 16V bus, ±80mV shunt, 12-bit 8x avg, continuous */
    (void)i2c_burst_write(i2c_dev, INA219_I2C_ADDR, 0x00,
                           (uint8_t[]){0x0E, 0xEF}, 2);

    g_i2c_dev = i2c_dev;
    g_use_raw_i2c = true;
    return BATTERY_STATUS_OK;
}

int battery_hal_current_init(void)
{
    /* Primary: Zephyr sensor driver */
    if (device_is_ready(g_ina219_dev)) {
        printk("[INA219] init OK (Zephyr driver)\n");
        g_initialized = true;
        return BATTERY_STATUS_OK;
    }

    /* Fallback: raw I2C */
    printk("[INA219] Zephyr driver failed, trying raw I2C...\n");
    int rc = raw_i2c_init();
    if (rc == BATTERY_STATUS_OK) {
        printk("[INA219] init OK (raw I2C)\n");
        g_initialized = true;
        return BATTERY_STATUS_OK;
    }

    printk("[INA219] init failed — check wiring (SDA=GPIO1, SCL=GPIO3)\n");
    return BATTERY_STATUS_IO;
}

int battery_hal_current_read_ma_x100(int32_t *current_ma_x100_out)
{
    if (current_ma_x100_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    if (!g_initialized) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }

    if (g_use_raw_i2c) {
        /* Read shunt voltage register (0x01).
         * Signed 16-bit, LSB = 10 uV. With 0.1 ohm shunt:
         * I = V_shunt / R = (raw * 10uV) / 0.1 ohm = raw * 100 uA
         * In 0.01 mA units: raw * 10 */
        uint8_t reg = INA219_REG_SHUNT;
        uint8_t buf[2];
        int rc = i2c_write_read(g_i2c_dev, INA219_I2C_ADDR,
                                 &reg, 1, buf, 2);
        if (rc != 0) {
            return BATTERY_STATUS_IO;
        }

        int16_t raw = (int16_t)((buf[0] << 8) | buf[1]);
        *current_ma_x100_out = (int32_t)raw * 10;
        return BATTERY_STATUS_OK;
    }

    /* Zephyr sensor API path */
    struct sensor_value val;

    if (sensor_sample_fetch(g_ina219_dev) < 0) {
        return BATTERY_STATUS_IO;
    }

    if (sensor_channel_get(g_ina219_dev, SENSOR_CHAN_CURRENT, &val) < 0) {
        return BATTERY_STATUS_IO;
    }

    /* sensor_value: val1 = integer amps, val2 = fractional (millionths).
     * Convert to 0.01 mA: val1 * 100000 + val2 / 10 */
    *current_ma_x100_out = (val.val1 * 100000) + (val.val2 / 10);
    return BATTERY_STATUS_OK;
}
