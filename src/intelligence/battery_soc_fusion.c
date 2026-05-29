/*
 * Phase 8c: voltage + coulomb signal fusion.
 *
 * Complementary filter with current-adaptive blend coefficient.
 * Integer-only math, no state, no allocations.
 *
 * Stateless: each call is a pure function of its inputs. No heap,
 * no static state.
 *
 * Design doc: docs/plans/2026-05-29-phase-8c-fusion-design.md
 */

#include <battery_sdk/battery_soc_fusion.h>

uint16_t battery_soc_fusion_select_alpha(int32_t abs_current_x100)
{
    /* Caller is responsible for passing absolute current. Negative
     * values are treated as below-threshold (so they would select
     * ALPHA_REST), which is documented in the header but not defended
     * against here. */
    if (abs_current_x100 < CONFIG_BATTERY_SOC_FUSION_I_THRESH_X100) {
        return (uint16_t)CONFIG_BATTERY_SOC_FUSION_ALPHA_REST_X1000;
    }
    return (uint16_t)CONFIG_BATTERY_SOC_FUSION_ALPHA_LOAD_X1000;
}

uint16_t battery_soc_fusion_blend(uint16_t soc_v_x100,
                                   uint16_t soc_c_x100,
                                   int32_t abs_current_x100)
{
    uint16_t alpha = battery_soc_fusion_select_alpha(abs_current_x100);

    /* fused = (alpha * soc_v + (1000 - alpha) * soc_c) / 1000
     *
     * Overflow analysis:
     *   max alpha               = 1000
     *   max soc_v_x100          = 10000
     *   max product             = 10,000,000
     *   sum of two such products = 20,000,000
     *   uint32_t max            = 4,294,967,295  (214x headroom)
     */
    uint32_t numerator = (uint32_t)alpha * (uint32_t)soc_v_x100
                       + (uint32_t)(1000 - alpha) * (uint32_t)soc_c_x100;
    return (uint16_t)(numerator / 1000);
}
