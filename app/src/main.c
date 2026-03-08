#include <stdio.h>
#include <stdint.h>

#include <zephyr/kernel.h>

#include "battery_sdk/battery_voltage.h"
#include "battery_sdk/battery_temperature.h"
#include "battery_sdk/battery_soc_estimator.h"

int main(void)
{
    int ret;
    int32_t voltage_mv = 0;
    int32_t temperature_c_x100 = 0;
    uint16_t soc_pct_x100 = 0;

    ret = battery_voltage_init();
    if (ret < 0) {
        printf("battery_voltage_init failed: %d\n", ret);
        return 0;
    }

    while (1) {
        if (battery_voltage_get_mv(&voltage_mv) < 0) {
            printf("Voltage read failed\n");
        }

        if (battery_temperature_get_c_x100(&temperature_c_x100) < 0) {
            printf("Temperature read failed\n");
        }

        if (battery_soc_estimator_get_pct_x100(&soc_pct_x100) < 0) {
            printf("SOC read failed\n");
        }

        printf("Voltage: %d mV\n", voltage_mv);
        printf("Temperature: %d.%02d C\n",
               (int)(temperature_c_x100 / 100),
               (int)((temperature_c_x100 < 0 ? -temperature_c_x100 : temperature_c_x100) % 100));
        printf("SOC: %u.%02u %%\n",
               (unsigned int)(soc_pct_x100 / 100),
               (unsigned int)(soc_pct_x100 % 100));
        printf("Battery SDK skeleton alive...\n\n");

        k_sleep(K_SECONDS(2));
    }

    return 0;
}