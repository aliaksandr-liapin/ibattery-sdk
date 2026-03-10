#ifndef BATTERY_SOC_LUT_H
#define BATTERY_SOC_LUT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Single entry in a voltage-to-SoC lookup table.
 * Entries MUST be sorted by voltage_mv in descending order
 * (highest voltage first).
 */
typedef struct {
    uint16_t voltage_mv;
    uint16_t soc_pct_x100;   /* SoC in 0.01% units: 10000 = 100.00% */
} battery_soc_lut_entry_t;

/**
 * Lookup table descriptor.
 */
typedef struct {
    const battery_soc_lut_entry_t *entries;
    size_t count;
} battery_soc_lut_t;

/** CR2032 primary lithium cell discharge curve. */
extern const battery_soc_lut_t battery_soc_lut_cr2032;

/** LiPo single-cell (3.7 V nominal) discharge curve. */
extern const battery_soc_lut_t battery_soc_lut_lipo_1s;

/**
 * Interpolate SoC from a voltage-to-SoC lookup table.
 *
 * Uses linear interpolation between table entries.
 * Clamps to table boundaries (100% above max, 0% below min).
 * Integer math only — no floating point.
 *
 * @param lut           Pointer to the lookup table descriptor.
 * @param voltage_mv    Measured battery voltage in millivolts.
 * @param soc_pct_x100  Output: SoC in 0.01% units (0..10000).
 * @return BATTERY_STATUS_OK on success, error code otherwise.
 */
int battery_soc_lut_interpolate(const battery_soc_lut_t *lut,
                                uint16_t voltage_mv,
                                uint16_t *soc_pct_x100);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SOC_LUT_H */
