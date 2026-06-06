#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <battery_sdk/battery_sdk.h>
#include <battery_sdk/battery_status.h>
#include <battery_sdk/battery_telemetry.h>

#if IS_ENABLED(CONFIG_BATTERY_TRANSPORT)
#include <battery_sdk/battery_transport.h>
#endif

#if IS_ENABLED(CONFIG_BATTERY_FUEL_GAUGE_SELFCHECK)
#include <zephyr/device.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <battery_sdk/battery_fuel_gauge.h>
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

#if IS_ENABLED(CONFIG_BATTERY_FUEL_GAUGE_SELFCHECK)
/* Read each property back through the standard fuel_gauge API and print raw
 * values + units, so they can be compared against the native telemetry line.
 * Expected conversions: V uV = mV*1000; I uA = -(mA*1000) [neg=discharge];
 * T 0.1K = degC*10 + 2731(.5); SOC % = round(native SOC); REM/FULL uAh;
 * DESIGN mAh; CYC 1/100ths; SOH(flags) = native soh*100. set_prop = -ENOSYS. */
static void fuel_gauge_selfcheck(void)
{
    const struct device *fg = DEVICE_DT_GET_ANY(aliaksandr_ibattery_fuel_gauge);
    union fuel_gauge_prop_val v;

    if (fg == NULL || !device_is_ready(fg)) {
        printk("  [FG] device not ready\n");
        return;
    }

    printk("  [FG]");
    if (fuel_gauge_get_prop(fg, FUEL_GAUGE_VOLTAGE, &v) == 0) {
        printk(" V=%duV", v.voltage);
    }
    if (fuel_gauge_get_prop(fg, FUEL_GAUGE_CURRENT, &v) == 0) {
        printk(" I=%duA", v.current);
    }
    if (fuel_gauge_get_prop(fg, FUEL_GAUGE_AVG_CURRENT, &v) == 0) {
        printk(" AVGI=%duA", v.avg_current);
    }
    if (fuel_gauge_get_prop(fg, FUEL_GAUGE_TEMPERATURE, &v) == 0) {
        printk(" T=%u(0.1K)", v.temperature);
    }
    if (fuel_gauge_get_prop(fg, FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE, &v) == 0) {
        printk(" SOC=%u%%", v.relative_state_of_charge);
    }
    if (fuel_gauge_get_prop(fg, FUEL_GAUGE_REMAINING_CAPACITY, &v) == 0) {
        printk(" REM=%uuAh", v.remaining_capacity);
    }
    if (fuel_gauge_get_prop(fg, FUEL_GAUGE_FULL_CHARGE_CAPACITY, &v) == 0) {
        printk(" FULL=%uuAh", v.full_charge_capacity);
    }
    if (fuel_gauge_get_prop(fg, FUEL_GAUGE_DESIGN_CAPACITY, &v) == 0) {
        printk(" DESIGN=%umAh", v.design_cap);
    }
    if (fuel_gauge_get_prop(fg, FUEL_GAUGE_CYCLE_COUNT, &v) == 0) {
        printk(" CYC=%u(1/100)", v.cycle_count);
    }
    if (fuel_gauge_get_prop(fg, BATTERY_FUEL_GAUGE_PROP_SOH, &v) == 0) {
        printk(" SOH(flags)=%u", v.flags);
    }
    /* read-only contract: set_property must report not-supported */
    printk(" set_rc=%d", fuel_gauge_set_prop(fg, FUEL_GAUGE_VOLTAGE, v));
    printk("\n");
}
#endif /* CONFIG_BATTERY_FUEL_GAUGE_SELFCHECK */

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
#if IS_ENABLED(CONFIG_BATTERY_SOC_SOH)
            printk(" SOH=%u.%02u%%",
                   pkt.soh_pct_x100 / 100U,
                   pkt.soh_pct_x100 % 100U);
#endif
#if IS_ENABLED(CONFIG_BATTERY_RUNTIME_TO_EMPTY)
            if (pkt.runtime_to_empty_min == UINT32_MAX) {
                printk(" RTE=n/a");
            } else {
                printk(" RTE=%u min", pkt.runtime_to_empty_min);
            }
#endif
            printk("\n");

#if IS_ENABLED(CONFIG_BATTERY_FUEL_GAUGE_SELFCHECK)
            fuel_gauge_selfcheck();
#endif

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
