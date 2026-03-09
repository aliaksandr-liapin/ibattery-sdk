#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <battery_sdk/battery_sdk.h>
#include <battery_sdk/battery_status.h>
#include <battery_sdk/battery_telemetry.h>

#include <stdint.h>

int main(void)
{
    struct battery_telemetry_packet pkt;
    int rc;

    rc = battery_sdk_init();
    if (rc != BATTERY_STATUS_OK) {
        printk("Battery SDK init failed: %d\n", rc);
    } else {
        printk("Battery SDK initialized OK\n");
    }

    while (1) {
        rc = battery_telemetry_collect(&pkt);
        if (rc != BATTERY_STATUS_OK) {
            printk("Telemetry collect failed: %d\n", rc);
        } else {
            printk("[v%u t=%u] V=%d mV T=%d.%02d C SOC=%u.%02u%% PWR=%u flags=0x%08x\n",
                   pkt.telemetry_version,
                   pkt.timestamp_ms,
                   pkt.voltage_mv,
                   pkt.temperature_c_x100 / 100,
                   (pkt.temperature_c_x100 >= 0)
                       ? (pkt.temperature_c_x100 % 100)
                       : -(pkt.temperature_c_x100 % 100),
                   pkt.soc_pct_x100 / 100U,
                   pkt.soc_pct_x100 % 100U,
                   pkt.power_state,
                   pkt.status_flags);
        }

        k_sleep(K_SECONDS(2));
    }

    return 0;
}
