#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <battery_sdk/battery_voltage.h>
#include <battery_sdk/battery_temperature.h>
#include <battery_sdk/battery_soc_estimator.h>

#include <stdint.h>

int main(void)
{
    uint16_t voltage_mv;
    int32_t temperature_c_x100;
    uint16_t soc_pct_x100;

    while (1) {
        if (battery_voltage_get_mv(&voltage_mv) < 0) {
            printk("Voltage read failed\n");
        } else {
            printk("Voltage: %u mV\n", voltage_mv);
        }

        if (battery_temperature_get_c_x100(&temperature_c_x100) < 0) {
            printk("Temperature read failed\n");
        } else {
            printk("Temperature: %d.%02d C\n",
                   temperature_c_x100 / 100,
                   (temperature_c_x100 >= 0) ? (temperature_c_x100 % 100) : -(temperature_c_x100 % 100));
        }

        if (battery_soc_estimator_get_pct_x100(&soc_pct_x100) < 0) {
            printk("SOC read failed\n");
        } else {
            printk("SOC: %u.%02u %%\n",
                   soc_pct_x100 / 100U,
                   soc_pct_x100 % 100U);
        }

        printk("Battery SDK skeleton alive...\n");
        k_sleep(K_SECONDS(2));
    }

    return 0;
}