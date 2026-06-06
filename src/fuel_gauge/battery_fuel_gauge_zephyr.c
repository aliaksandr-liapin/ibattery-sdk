/*
 * Read-only Zephyr fuel_gauge driver for the iBattery SDK.
 *
 * Presents iBattery's telemetry (collected via battery_telemetry_collect) as a
 * standard Zephyr fuel_gauge device. Only `.get_property` is implemented; the
 * gauge is a software, read-only view, so set/get_buffer/battery_cutoff are left
 * unset (the fuel_gauge API returns -ENOSYS for those, which is correct here).
 *
 * State-of-Health is exposed via the downstream-custom BATTERY_FUEL_GAUGE_PROP_SOH
 * property (value in val->flags, centi-percent). SoH is only available when
 * CONFIG_BATTERY_SOC_SOH is enabled; otherwise the SoH property returns -ENOTSUP
 * and full-charge capacity falls back to the rated capacity.
 *
 * Gated on CONFIG_BATTERY_FUEL_GAUGE_API (see app/Kconfig.battery).
 */

#define DT_DRV_COMPAT aliaksandr_ibattery_fuel_gauge

#include <zephyr/device.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <errno.h>

#include "battery_sdk/battery_fuel_gauge.h"
#include "battery_sdk/battery_fuel_gauge_convert.h"
#include "battery_sdk/battery_telemetry.h"

#if defined(CONFIG_BATTERY_SOC_SOH)
#include "battery_sdk/battery_soh.h"
#endif

#if defined(CONFIG_BATTERY_RUNTIME_TO_EMPTY)
#include "battery_sdk/battery_runtime.h"
#include "battery_sdk/battery_status.h"
#endif

static int ibattery_fg_get_prop(const struct device *dev, fuel_gauge_prop_t prop,
				union fuel_gauge_prop_val *val)
{
	ARG_UNUSED(dev);

	struct battery_telemetry_packet pkt;

	if (battery_telemetry_collect(&pkt) != 0) {
		return -EIO;
	}

	switch (prop) {
	case FUEL_GAUGE_VOLTAGE:
		val->voltage = battery_fg_mv_to_uv(pkt.voltage_mv);
		break;
	case FUEL_GAUGE_CURRENT:
		val->current = battery_fg_ma_x100_to_ua(pkt.current_ma_x100);
		break;
	case FUEL_GAUGE_AVG_CURRENT:
		val->avg_current = battery_fg_ma_x100_to_ua(pkt.current_ma_x100);
		break;
	case FUEL_GAUGE_TEMPERATURE:
		val->temperature = battery_fg_cdegc_to_decikelvin(pkt.temperature_c_x100);
		break;
	case FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE:
		val->relative_state_of_charge = battery_fg_socx100_to_pct(pkt.soc_pct_x100);
		break;
	case FUEL_GAUGE_ABSOLUTE_STATE_OF_CHARGE:
		val->absolute_state_of_charge = battery_fg_socx100_to_pct(pkt.soc_pct_x100);
		break;
	case FUEL_GAUGE_REMAINING_CAPACITY:
		val->remaining_capacity = battery_fg_mahx100_to_uah(pkt.coulomb_mah_x100);
		break;
	case FUEL_GAUGE_FULL_CHARGE_CAPACITY: {
		uint16_t soh = 0; /* 0 == unknown -> helper falls back to rated */
#if defined(CONFIG_BATTERY_SOC_SOH)
		(void)battery_soh_get_pct_x100(&soh);
#endif
		val->full_charge_capacity =
			battery_fg_full_charge_uah(CONFIG_BATTERY_CAPACITY_MAH, soh);
		break;
	}
	case FUEL_GAUGE_DESIGN_CAPACITY:
		val->design_cap = (uint16_t)CONFIG_BATTERY_CAPACITY_MAH;
		break;
	case FUEL_GAUGE_CYCLE_COUNT:
		val->cycle_count = battery_fg_cycles_to_centi(pkt.cycle_count);
		break;
	case BATTERY_FUEL_GAUGE_PROP_SOH: {
#if defined(CONFIG_BATTERY_SOC_SOH)
		uint16_t soh = 0;

		if (battery_soh_get_pct_x100(&soh) != 0) {
			return -ENOTSUP;
		}
		val->flags = soh;
		break;
#else
		return -ENOTSUP;
#endif
	}
#if defined(CONFIG_BATTERY_RUNTIME_TO_EMPTY)
	case FUEL_GAUGE_RUNTIME_TO_EMPTY: {
		uint32_t rte;

		if (battery_runtime_to_empty_min(pkt.coulomb_mah_x100, &rte) !=
		    BATTERY_STATUS_OK) {
			return -ENOTSUP; /* idle/charging -> not available */
		}
		val->runtime_to_empty = rte;
		break;
	}
#endif
	default:
		return -ENOTSUP;
	}

	return 0;
}

static const struct fuel_gauge_driver_api ibattery_fg_api = {
	.get_property = ibattery_fg_get_prop,
};

static int ibattery_fg_init(const struct device *dev)
{
	ARG_UNUSED(dev);
	return 0;
}

#define IBATTERY_FG_INIT(inst)                                                  \
	DEVICE_DT_INST_DEFINE(inst, ibattery_fg_init, NULL, NULL, NULL,         \
			      POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, \
			      &ibattery_fg_api);

DT_INST_FOREACH_STATUS_OKAY(IBATTERY_FG_INIT)
