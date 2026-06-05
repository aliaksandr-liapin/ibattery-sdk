/*
 * Module-consumption smoke test for the read-only Zephyr fuel_gauge driver.
 * Exercise the public fuel_gauge surface (standard properties + the SDK's
 * custom SoH property) so the linker must resolve the iBattery fuel_gauge
 * driver and the fuel_gauge subsystem through the Zephyr-module build path.
 */
#include <zephyr/device.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <battery_sdk/battery_sdk.h>
#include <battery_sdk/battery_fuel_gauge.h>

int main(void)
{
    const struct device *fg = DEVICE_DT_GET_ANY(aliaksandr_ibattery_fuel_gauge);
    union fuel_gauge_prop_val val;

    (void)battery_sdk_init();
    if (fg != NULL && device_is_ready(fg)) {
        (void)fuel_gauge_get_prop(fg, FUEL_GAUGE_VOLTAGE, &val);
        (void)fuel_gauge_get_prop(fg, FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE, &val);
        (void)fuel_gauge_get_prop(fg, BATTERY_FUEL_GAUGE_PROP_SOH, &val);
    }
    return 0;
}
