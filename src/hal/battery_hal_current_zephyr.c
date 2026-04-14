/*
 * Current HAL — INA219 current sensor via I2C.
 *
 * Uses raw I2C register access to the INA219. The Zephyr built-in
 * INA219 sensor driver is NOT used because it runs at boot before
 * I2C is stable on some platforms (ESP32-C3), leaving the bus in a
 * broken state. Direct I2C avoids this entirely.
 *
 * The INA219 devicetree node should have status = "disabled" to
 * prevent the Zephyr driver from attempting boot initialization.
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
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

/* I2C bus from the ina219 node's parent */
#define INA219_NODE    DT_NODELABEL(ina219)
#define INA219_I2C_BUS DT_BUS(INA219_NODE)

#define INA219_ADDR       0x40
#define INA219_REG_CONFIG 0x00
#define INA219_REG_SHUNT  0x01
#define INA219_REG_CAL    0x05

static const struct device *g_i2c_dev;
static bool g_initialized;
static bool g_calibrated;

/**
 * Write a 16-bit register via I2C burst write.
 */
static int ina219_reg_write(uint8_t reg, uint16_t value)
{
    uint8_t data[2] = { (uint8_t)(value >> 8), (uint8_t)(value & 0xFF) };
    return i2c_burst_write(g_i2c_dev, INA219_ADDR, reg, data, 2);
}

/**
 * Read a 16-bit register via separate write + read.
 * (i2c_write_read with repeated-start fails on some ESP32 I2C drivers)
 */
static int ina219_reg_read(uint8_t reg, int16_t *value)
{
    uint8_t buf[2];
    int rc;

    rc = i2c_write(g_i2c_dev, &reg, 1, INA219_ADDR);
    if (rc != 0) {
        return rc;
    }

    rc = i2c_read(g_i2c_dev, buf, 2, INA219_ADDR);
    if (rc != 0) {
        return rc;
    }

    *value = (int16_t)((buf[0] << 8) | buf[1]);
    return 0;
}

int battery_hal_current_init(void)
{
    g_i2c_dev = DEVICE_DT_GET(INA219_I2C_BUS);

    if (!device_is_ready(g_i2c_dev)) {
        printk("[INA219] I2C bus not ready\n");
        return BATTERY_STATUS_IO;
    }

    /* Probe: simple read (no register pointer write) to verify INA219 responds */
    uint8_t probe[2];
    int rc = i2c_read(g_i2c_dev, probe, 2, INA219_ADDR);
    if (rc != 0) {
        printk("[INA219] not found at 0x%02x (rc=%d) — check wiring "
               "(SDA=GPIO1, SCL=GPIO3)\n", INA219_ADDR, rc);
        return BATTERY_STATUS_IO;
    }
    printk("[INA219] probe OK: 0x%02x%02x\n", probe[0], probe[1]);

    /* Reset the device */
    rc = ina219_reg_write(INA219_REG_CONFIG, 0x8000);
    if (rc != 0) {
        printk("[INA219] reset failed: %d\n", rc);
        /* Continue — device may still work with defaults */
    } else {
        k_msleep(10);
    }

    /* Calibration: Cal = 0.04096 / (LSB * R_shunt)
     * For 100 uA LSB, 0.1 ohm: Cal = 0.04096 / (100e-6 * 0.1) = 4096 */
    rc = ina219_reg_write(INA219_REG_CAL, 4096);
    g_calibrated = (rc == 0);

    /* Config: BRNG=0(16V), PG=01(±80mV), BADC=1101(12b 8x avg),
     * SADC=1101(12b 8x avg), MODE=111(continuous shunt+bus)
     * = 0b0_00_01_1101_1101_111 = 0x0EEF */
    (void)ina219_reg_write(INA219_REG_CONFIG, 0x0EEF);

    printk("[INA219] init OK%s\n", g_calibrated ? "" : " (uncalibrated)");
    g_initialized = true;
    return BATTERY_STATUS_OK;
}

int battery_hal_current_read_ma_x100(int32_t *current_ma_x100_out)
{
    if (current_ma_x100_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    if (!g_initialized) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }

    /* Read shunt voltage register (0x01).
     * Signed 16-bit, LSB = 10 uV.
     * With 0.1 ohm shunt: I = V / R = (raw * 10uV) / 0.1 = raw * 100 uA
     * In 0.01 mA units: raw * 10 */
    int16_t raw;
    int rc = ina219_reg_read(INA219_REG_SHUNT, &raw);
    if (rc != 0) {
        return BATTERY_STATUS_IO;
    }

    *current_ma_x100_out = (int32_t)raw * 10;
    return BATTERY_STATUS_OK;
}
