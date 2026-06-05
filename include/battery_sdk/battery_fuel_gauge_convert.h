#ifndef BATTERY_SDK_BATTERY_FUEL_GAUGE_CONVERT_H
#define BATTERY_SDK_BATTERY_FUEL_GAUGE_CONVERT_H
#include <stdint.h>

/* Battery voltage mV -> Zephyr fuel_gauge µV. */
int32_t battery_fg_mv_to_uv(int32_t mv);

/* Current centi-mA (0.01 mA) -> Zephyr µA, sign-flipped to Zephyr's
 * negative=discharging convention (iBattery reports discharge as positive). */
int32_t battery_fg_ma_x100_to_ua(int32_t ma_x100);

/* Temperature centi-°C (0.01 °C) -> Zephyr 0.1 K (uint16, clamped >=0). */
uint16_t battery_fg_cdegc_to_decikelvin(int32_t c_x100);

/* SoC centi-percent (0.01 %) -> integer percent 0..100 (rounded, clamped). */
uint8_t battery_fg_socx100_to_pct(uint16_t soc_x100);

/* Charge centi-mAh (0.01 mAh) -> Zephyr µAh (uint32, negatives clamp to 0). */
uint32_t battery_fg_mahx100_to_uah(int32_t mah_x100);

/* Full-charge capacity µAh from rated mAh scaled by SoH (soh centi-percent).
 * soh_x100==0 (unknown) -> falls back to rated capacity. */
uint32_t battery_fg_full_charge_uah(int32_t rated_mah, uint16_t soh_x100);

/* Cycle count -> Zephyr "1/100ths" unit. */
uint32_t battery_fg_cycles_to_centi(uint32_t cycles);

#endif
