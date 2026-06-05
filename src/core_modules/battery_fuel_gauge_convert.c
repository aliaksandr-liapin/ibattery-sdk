/*
 * Pure unit-conversion helpers bridging iBattery's internal fixed-point
 * units to the units expected by Zephyr's fuel_gauge subsystem.
 *
 * No Zephyr headers — host-testable with Unity. The upcoming Zephyr
 * fuel_gauge driver calls these to translate cached telemetry into the
 * values returned by fuel_gauge_get_prop().
 */

#include "battery_sdk/battery_fuel_gauge_convert.h"

int32_t battery_fg_mv_to_uv(int32_t mv) { return mv * 1000; }

int32_t battery_fg_ma_x100_to_ua(int32_t ma_x100) { return -(ma_x100 * 10); }

uint16_t battery_fg_cdegc_to_decikelvin(int32_t c_x100) {
    int32_t dk = (c_x100 + 27315) / 10;
    if (dk < 0) dk = 0;
    if (dk > UINT16_MAX) dk = UINT16_MAX;
    return (uint16_t)dk;
}

uint8_t battery_fg_socx100_to_pct(uint16_t soc_x100) {
    uint32_t pct = ((uint32_t)soc_x100 + 50u) / 100u;
    return (uint8_t)(pct > 100u ? 100u : pct);
}

uint32_t battery_fg_mahx100_to_uah(int32_t mah_x100) {
    if (mah_x100 < 0) return 0;
    return (uint32_t)mah_x100 * 10u;
}

uint32_t battery_fg_full_charge_uah(int32_t rated_mah, uint16_t soh_x100) {
    if (rated_mah < 0) rated_mah = 0;
    if (soh_x100 == 0) return (uint32_t)rated_mah * 1000u;
    return (uint32_t)(((uint64_t)rated_mah * 1000u * soh_x100) / 10000u);
}

uint32_t battery_fg_cycles_to_centi(uint32_t cycles) { return cycles * 100u; }
