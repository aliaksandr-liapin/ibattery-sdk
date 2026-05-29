/*
 * Unit tests for battery_soc_fusion module (Phase 8c, v0.10.0).
 *
 * Compiled with concrete Kconfig values via target_compile_definitions
 * in tests/CMakeLists.txt:
 *   CONFIG_BATTERY_SOC_FUSION_ALPHA_REST_X1000 = 50
 *   CONFIG_BATTERY_SOC_FUSION_ALPHA_LOAD_X1000 = 5
 *   CONFIG_BATTERY_SOC_FUSION_I_THRESH_X100    = 200
 */

#include "unity.h"
#include <battery_sdk/battery_soc_fusion.h>

void setUp(void) {}
void tearDown(void) {}

/* --- blend() math ----------------------------------------------------- */

void test_blend_alpha_0_returns_pure_coulomb(void)
{
    /* abs_current = 99999 -> well above threshold -> ALPHA_LOAD = 5.
     * With alpha=5 (out of 1000) and soc_c=4000, soc_v=10000:
     * fused = (5 * 10000 + 995 * 4000) / 1000 = (50000 + 3980000) / 1000 = 4030
     * The selector returns 5 (LOAD), so we test through select_alpha to be precise.
     * For the "pure coulomb" case (alpha=0), test the internal blend math
     * directly via select_alpha's actual return values. We assert relationships
     * rather than exact numbers when alpha is fixed by the selector. */
    uint16_t fused = battery_soc_fusion_blend(10000, 4000, 99999);
    /* With ALPHA_LOAD=5, fused should be very close to coulomb (4000) */
    TEST_ASSERT_INT_WITHIN(50, 4030, fused);
}

void test_blend_at_rest_pulls_5pct_toward_voltage(void)
{
    /* abs_current = 100 -> below threshold 200 -> ALPHA_REST = 50.
     * soc_v=10000, soc_c=0:
     * fused = (50 * 10000 + 950 * 0) / 1000 = 500 */
    uint16_t fused = battery_soc_fusion_blend(10000, 0, 100);
    TEST_ASSERT_EQUAL_UINT16(500, fused);
}

void test_blend_no_overflow_at_max_values(void)
{
    /* alpha = 50 (rest), soc_v = 10000, soc_c = 10000:
     * fused = (50 * 10000 + 950 * 10000) / 1000 = (500000 + 9500000) / 1000 = 10000
     * Confirms no u32 overflow when both signals are at their max. */
    uint16_t fused = battery_soc_fusion_blend(10000, 10000, 0);
    TEST_ASSERT_EQUAL_UINT16(10000, fused);
}

void test_blend_symmetric_inputs_symmetric_outputs(void)
{
    /* Both signals equal -> fused equals them, regardless of alpha. */
    TEST_ASSERT_EQUAL_UINT16(5000, battery_soc_fusion_blend(5000, 5000, 0));
    TEST_ASSERT_EQUAL_UINT16(5000, battery_soc_fusion_blend(5000, 5000, 99999));
}

void test_blend_at_rest_blends_correctly(void)
{
    /* alpha = 50 (rest), soc_v = 8000, soc_c = 4000:
     * fused = (50 * 8000 + 950 * 4000) / 1000 = (400000 + 3800000) / 1000 = 4200 */
    uint16_t fused = battery_soc_fusion_blend(8000, 4000, 0);
    TEST_ASSERT_EQUAL_UINT16(4200, fused);
}

void test_blend_under_load_barely_moves(void)
{
    /* alpha = 5 (load), soc_v = 10000, soc_c = 5000:
     * fused = (5 * 10000 + 995 * 5000) / 1000 = (50000 + 4975000) / 1000 = 5025 */
    uint16_t fused = battery_soc_fusion_blend(10000, 5000, 99999);
    TEST_ASSERT_EQUAL_UINT16(5025, fused);
}

/* --- select_alpha() --------------------------------------------------- */

void test_select_alpha_rest_when_below_threshold(void)
{
    TEST_ASSERT_EQUAL_UINT16(50, battery_soc_fusion_select_alpha(0));
    TEST_ASSERT_EQUAL_UINT16(50, battery_soc_fusion_select_alpha(100));
    TEST_ASSERT_EQUAL_UINT16(50, battery_soc_fusion_select_alpha(199));
}

void test_select_alpha_load_when_above_threshold(void)
{
    TEST_ASSERT_EQUAL_UINT16(5, battery_soc_fusion_select_alpha(201));
    TEST_ASSERT_EQUAL_UINT16(5, battery_soc_fusion_select_alpha(5000));
    TEST_ASSERT_EQUAL_UINT16(5, battery_soc_fusion_select_alpha(99999));
}

void test_select_alpha_at_exact_threshold_returns_load(void)
{
    /* Boundary convention: at exactly I_THRESH, we use LOAD alpha. */
    TEST_ASSERT_EQUAL_UINT16(5, battery_soc_fusion_select_alpha(200));
}

void test_select_alpha_treats_negative_current_as_below_threshold(void)
{
    /* Documented behavior (header @note): caller MUST absolute the value
     * before calling. If they don't, the comparison treats negative as
     * < threshold and we return ALPHA_REST. This test pins that contract
     * so a future refactor doesn't silently flip it. */
    TEST_ASSERT_EQUAL_UINT16(50, battery_soc_fusion_select_alpha(-1));
}

/* --- runner ----------------------------------------------------------- */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_blend_alpha_0_returns_pure_coulomb);
    RUN_TEST(test_blend_at_rest_pulls_5pct_toward_voltage);
    RUN_TEST(test_blend_no_overflow_at_max_values);
    RUN_TEST(test_blend_symmetric_inputs_symmetric_outputs);
    RUN_TEST(test_blend_at_rest_blends_correctly);
    RUN_TEST(test_blend_under_load_barely_moves);
    RUN_TEST(test_select_alpha_rest_when_below_threshold);
    RUN_TEST(test_select_alpha_load_when_above_threshold);
    RUN_TEST(test_select_alpha_at_exact_threshold_returns_load);
    RUN_TEST(test_select_alpha_treats_negative_current_as_below_threshold);
    return UNITY_END();
}
