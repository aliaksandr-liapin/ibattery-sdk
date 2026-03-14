/**
 * TP4056 charger status driver — reads CHRG and STDBY GPIO pins.
 *
 * Pin truth table (active-low outputs from TP4056):
 *   CHRG  STDBY  Meaning
 *   LOW   HIGH   Charging in progress
 *   HIGH  LOW    Charge complete (standby)
 *   HIGH  HIGH   No battery / no input power
 *   LOW   LOW    Undefined — treated as charging
 *
 * Enabled only when CONFIG_BATTERY_CHARGER_TP4056 is set in Kconfig.
 */

#if defined(CONFIG_BATTERY_CHARGER_TP4056)

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <battery_sdk/battery_status.h>
#include <stdbool.h>

#include "battery_hal_charger.h"

/* GPIO device — port 0 on nRF52840 */
static const struct device *gpio_dev;

#ifndef CONFIG_BATTERY_CHARGER_TP4056_CHRG_PIN
#define CONFIG_BATTERY_CHARGER_TP4056_CHRG_PIN 28
#endif

#ifndef CONFIG_BATTERY_CHARGER_TP4056_STDBY_PIN
#define CONFIG_BATTERY_CHARGER_TP4056_STDBY_PIN 29
#endif

#define CHRG_PIN  CONFIG_BATTERY_CHARGER_TP4056_CHRG_PIN
#define STDBY_PIN CONFIG_BATTERY_CHARGER_TP4056_STDBY_PIN

int battery_hal_charger_init(void)
{
    int rc;

    gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    if (!device_is_ready(gpio_dev)) {
        return BATTERY_STATUS_IO;
    }

    /* Configure as input with internal pull-up (TP4056 outputs are open-drain) */
    rc = gpio_pin_configure(gpio_dev, CHRG_PIN,
                            GPIO_INPUT | GPIO_PULL_UP);
    if (rc < 0) {
        return BATTERY_STATUS_IO;
    }

    rc = gpio_pin_configure(gpio_dev, STDBY_PIN,
                            GPIO_INPUT | GPIO_PULL_UP);
    if (rc < 0) {
        return BATTERY_STATUS_IO;
    }

    return BATTERY_STATUS_OK;
}

int battery_hal_charger_is_charging(bool *charging_out)
{
    int val;

    if (charging_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    if (gpio_dev == NULL) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }

    val = gpio_pin_get(gpio_dev, CHRG_PIN);
    if (val < 0) {
        return BATTERY_STATUS_IO;
    }

    /* Active low: 0 = charging */
    *charging_out = (val == 0);
    return BATTERY_STATUS_OK;
}

int battery_hal_charger_is_charged(bool *charged_out)
{
    int val;

    if (charged_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    if (gpio_dev == NULL) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }

    val = gpio_pin_get(gpio_dev, STDBY_PIN);
    if (val < 0) {
        return BATTERY_STATUS_IO;
    }

    /* Active low: 0 = charge complete */
    *charged_out = (val == 0);
    return BATTERY_STATUS_OK;
}

#endif /* CONFIG_BATTERY_CHARGER_TP4056 */
