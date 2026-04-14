#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <battery_sdk/battery_sdk.h>
#include <battery_sdk/battery_status.h>
#include <battery_sdk/battery_telemetry.h>

#if IS_ENABLED(CONFIG_BATTERY_TRANSPORT)
#include <battery_sdk/battery_transport.h>
#endif

#include <stdint.h>

static void print_platform_info(void)
{
    printk("\n=== iBattery SDK — On-Target Validation ===\n");

#if defined(CONFIG_SOC_SERIES_STM32L4X)
    printk("Platform: STM32L4 (NUCLEO-L476RG)\n");
    printk("VDD read: VREFINT sensor\n");
#elif defined(CONFIG_SOC_SERIES_NRF52X)
    printk("Platform: nRF52 (nRF52840-DK)\n");
    printk("VDD read: SAADC direct\n");
#elif defined(CONFIG_SOC_SERIES_ESP32C3)
    printk("Platform: ESP32-C3 (DevKitM)\n");
    printk("VDD read: External voltage divider\n");
#else
    printk("Platform: unknown\n");
#endif

#if IS_ENABLED(CONFIG_BATTERY_TEMP_NTC)
    printk("Temp src: NTC thermistor (external)\n");
#elif IS_ENABLED(CONFIG_BATTERY_TEMP_DIE)
    printk("Temp src: Die sensor (internal)\n");
#endif

#if IS_ENABLED(CONFIG_BATTERY_CHEMISTRY_LIPO)
    printk("Chemistry: LiPo (3.7V)\n");
#else
    printk("Chemistry: CR2032 (3.0V)\n");
#endif

#if IS_ENABLED(CONFIG_BATTERY_TRANSPORT)
    printk("Transport: BLE enabled\n");
#else
    printk("Transport: disabled\n");
#endif

#if IS_ENABLED(CONFIG_BATTERY_CHARGER_TP4056)
    printk("Charger:   TP4056 GPIO\n");
#else
    printk("Charger:   disabled\n");
#endif

#if IS_ENABLED(CONFIG_BATTERY_CURRENT_SENSE)
    printk("Current:   INA219 (coulomb counting)\n");
#else
    printk("Current:   disabled\n");
#endif

    printk("============================================\n\n");
}

int main(void)
{
    struct battery_telemetry_packet pkt;
    int rc;

    print_platform_info();

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
            printk("[v%u t=%u] V=%d mV T=%d.%02d C SOC=%u.%02u%% PWR=%u CYC=%u flags=0x%08x",
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
                   pkt.cycle_count,
                   pkt.status_flags);

#if IS_ENABLED(CONFIG_BATTERY_CURRENT_SENSE)
            printk(" I=%d.%02d mA Q=%d.%02d mAh",
                   pkt.current_ma_x100 / 100,
                   (pkt.current_ma_x100 >= 0)
                       ? (pkt.current_ma_x100 % 100)
                       : -(pkt.current_ma_x100 % 100),
                   pkt.coulomb_mah_x100 / 100,
                   (pkt.coulomb_mah_x100 >= 0)
                       ? (pkt.coulomb_mah_x100 % 100)
                       : -(pkt.coulomb_mah_x100 % 100));
#endif
            printk("\n");

#if IS_ENABLED(CONFIG_BATTERY_TRANSPORT)
            rc = battery_transport_send(&pkt);
            if (rc != BATTERY_STATUS_OK) {
                printk("Transport send failed: %d\n", rc);
            }
#endif
        }

        k_sleep(K_SECONDS(2));
    }

    return 0;
}
