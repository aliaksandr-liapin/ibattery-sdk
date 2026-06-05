/*
 * Read-only Zephyr fuel_gauge driver bindings for the iBattery SDK.
 *
 * Maps iBattery's telemetry outputs (SoC, voltage, current, capacity, cycle
 * count, temperature) onto the standard Zephyr fuel_gauge properties. The
 * driver is instantiated from a devicetree node with compatible
 * "aliaksandr,ibattery-fuel-gauge" and is gated on CONFIG_BATTERY_FUEL_GAUGE_API.
 *
 * Only `.get_property` is implemented (the gauge is a software, read-only view
 * of iBattery's estimators); set/get_buffer/cutoff are left unset.
 */

#ifndef BATTERY_SDK_BATTERY_FUEL_GAUGE_H
#define BATTERY_SDK_BATTERY_FUEL_GAUGE_H

#include <zephyr/drivers/fuel_gauge.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Custom State-of-Health property for the iBattery Zephyr fuel_gauge driver.
 * The standard fuel_gauge API has no SoH property; this driver exposes it as a
 * downstream-custom property. Value is delivered in val->flags as SoH in
 * centi-percent (e.g. 7310 == 73.10 %). */
#define BATTERY_FUEL_GAUGE_PROP_SOH (FUEL_GAUGE_CUSTOM_BEGIN + 0)

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SDK_BATTERY_FUEL_GAUGE_H */
