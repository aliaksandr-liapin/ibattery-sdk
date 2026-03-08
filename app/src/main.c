#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <battery_sdk/battery_adc.h>
#include <battery_sdk/battery_voltage.h>
#include <battery_sdk/battery_temperature.h>
#include <battery_sdk/battery_soc_estimator.h>
#include <battery_sdk/battery_power_manager.h>
#include <battery_sdk/battery_telemetry.h>

int main(void)
{
    int rc;
    int32_t voltage_mv = 0;
    int32_t temperature_c_x100 = 0;
    uint16_t soc_pct_x100 = 0;
    struct battery_telemetry_packet telemetry;

    printk("Battery SDK Phase 1 skeleton starting...\n");

    rc = battery_adc_init();
    printk("battery_adc_init: %d\n", rc);

    rc = battery_voltage_init();
    printk("battery_voltage_init: %d\n", rc);

    rc = battery_temperature_init();
    printk("battery_temperature_init: %d\n", rc);

    rc = battery_soc_estimator_init();
    printk("battery_soc_estimator_init: %d\n", rc);

    rc = battery_power_manager_init();
    printk("battery_power_manager_init: %d\n", rc);

    rc = battery_telemetry_init();
    printk("battery_telemetry_init: %d\n", rc);

    rc = battery_voltage_get_mv(&voltage_mv);
    printk("battery_voltage_get_mv: rc=%d, value=%d mV\n", rc, voltage_mv);

    rc = battery_temperature_get_c_x100(&temperature_c_x100);
    printk("battery_temperature_get_c_x100: rc=%d, value=%d\n", rc, temperature_c_x100);

    rc = battery_soc_estimator_get_pct_x100(&soc_pct_x100);
    printk("battery_soc_estimator_get_pct_x100: rc=%d, value=%u\n", rc, soc_pct_x100);

    rc = battery_telemetry_collect(&telemetry);
    printk("battery_telemetry_collect: rc=%d\n", rc);

    while (1) {

    int32_t voltage_mv = 0;
    int32_t temperature_c = 0;
    uint16_t soc = 0;

    battery_voltage_get_mv(&voltage_mv);
    battery_temperature_get_c_x100(&temperature_c);
    battery_soc_estimator_get_pct_x100(&soc);

    printk("Voltage: %d mV\n", voltage_mv);
    printk("Temperature: %d.%02d C\n",
           temperature_c / 100,
           temperature_c % 100);

    printk("SOC: %d.%02d %%\n",
           soc / 100,
           soc % 100);

    printk("Battery SDK skeleton alive...\n\n");

    k_sleep(K_SECONDS(5));
}

    return 0;
}