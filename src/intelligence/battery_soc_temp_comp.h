#ifndef BATTERY_SOC_TEMP_COMP_H
#define BATTERY_SOC_TEMP_COMP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Compute temperature-compensated SoC by blending between cold, room,
 * and hot LiPo LUT tables.
 *
 * Temperature zones:
 *   - below 0 deg C:  cold LUT only (clamped)
 *   - 0..25 deg C:    blend cold + room
 *   - 25..45 deg C:   blend room + hot
 *   - above 45 deg C: hot LUT only (clamped)
 *
 * Uses Q8 fixed-point blending (integer-only math).
 *
 * @param voltage_mv    Battery voltage in millivolts.
 * @param temp_c_x100   Temperature in 0.01 deg C units (e.g. 2500 = 25.00 C).
 * @param soc_pct_x100  Output: SoC in 0.01% units (0..10000).
 * @return BATTERY_STATUS_OK on success, error code otherwise.
 */
int battery_soc_temp_compensated(uint16_t voltage_mv,
                                 int32_t temp_c_x100,
                                 uint16_t *soc_pct_x100);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SOC_TEMP_COMP_H */
