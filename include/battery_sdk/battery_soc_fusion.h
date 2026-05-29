/*
 * Phase 8c: voltage + coulomb signal fusion.
 *
 * Combines two SoC estimates (voltage-LUT, coulomb integrator) into a
 * single fused estimate using a complementary filter with current-
 * adaptive coefficient. The blend coefficient (alpha) is small under
 * load (voltage unreliable due to IR drop) and larger at rest
 * (voltage-LUT is accurate).
 *
 * Design doc: docs/plans/2026-05-29-phase-8c-fusion-design.md
 */

#ifndef BATTERY_SDK_BATTERY_SOC_FUSION_H
#define BATTERY_SDK_BATTERY_SOC_FUSION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Fuse voltage-LUT and coulomb-counter SoC estimates.
 *
 * The blend coefficient (alpha) is selected internally based on the
 * absolute current magnitude. See `battery_soc_fusion_select_alpha`.
 *
 * @param soc_v_x100        Voltage-LUT SoC in 0.01% units (0..10000).
 * @param soc_c_x100        Coulomb SoC in 0.01% units (0..10000).
 * @param abs_current_x100  Absolute current in 0.01 mA units (0..INT32_MAX,
 *                          practically 0..1,000,000 = 0..10 A for this SDK).
 * @return Fused SoC in 0.01% units (0..10000).
 *
 * @note The caller is responsible for absoluting signed current
 *       readings before calling. Negative values are accepted as a
 *       courtesy — they compare as "below threshold" and select
 *       ALPHA_REST — but this is not an absolute-value operation
 *       inside the function. Always pass abs(I) at the call site.
 *
 * @note Inputs outside 0..10000 produce a blend in their corresponding
 *       range; the function does not clamp. Callers responsible for
 *       providing values in range.
 */
uint16_t battery_soc_fusion_blend(uint16_t soc_v_x100,
                                   uint16_t soc_c_x100,
                                   int32_t abs_current_x100);

/**
 * Select the blend coefficient (alpha) based on current magnitude.
 *
 * Exposed for testability. Returns alpha in x1000 units (0..1000).
 *
 * @param abs_current_x100  Absolute current in 0.01 mA units (0..INT32_MAX,
 *                          practically 0..1,000,000 = 0..10 A for this SDK).
 * @return Alpha in x1000 units (0 = pure coulomb, 1000 = pure voltage).
 *
 * @note The caller is responsible for absoluting signed current
 *       readings before calling. Negative values are accepted as a
 *       courtesy — they compare as "below threshold" and select
 *       ALPHA_REST — but this is not an absolute-value operation
 *       inside the function. Always pass abs(I) at the call site.
 */
uint16_t battery_soc_fusion_select_alpha(int32_t abs_current_x100);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SDK_BATTERY_SOC_FUSION_H */
