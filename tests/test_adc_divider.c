/* Unit tests for the ADC voltage-divider scaling helper. */
#include "unity.h"
#include "helpers/battery_adc_scale.h"
#include <stdint.h>

void setUp(void)    {}
void tearDown(void) {}

void test_ratio_two_doubles_pin_voltage(void)
{
    /* 100k/100k divider: V_batt = V_pin * 2. */
    TEST_ASSERT_EQUAL_INT32(3000, battery_adc_apply_divider(1500, 2));
    TEST_ASSERT_EQUAL_INT32(4180, battery_adc_apply_divider(2090, 2));
}

void test_ratio_one_is_identity(void)
{
    TEST_ASSERT_EQUAL_INT32(3000, battery_adc_apply_divider(3000, 1));
}

void test_ratio_below_one_is_clamped_to_identity(void)
{
    /* A nonsensical ratio (0 or negative) must not zero/invert the reading;
     * treat as direct (ratio 1) so a misconfig degrades safely. */
    TEST_ASSERT_EQUAL_INT32(2500, battery_adc_apply_divider(2500, 0));
    TEST_ASSERT_EQUAL_INT32(2500, battery_adc_apply_divider(2500, -3));
}

void test_overflow_saturates_to_int32_max(void)
{
    /* pin_mv * ratio must not wrap negative on overflow. */
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, battery_adc_apply_divider(2000000000, 2));
}

void test_negative_pin_voltage_passes_through(void)
{
    /* Defensive: a negative pin reading scales but never via the clamp path. */
    TEST_ASSERT_EQUAL_INT32(-200, battery_adc_apply_divider(-100, 2));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ratio_two_doubles_pin_voltage);
    RUN_TEST(test_ratio_one_is_identity);
    RUN_TEST(test_ratio_below_one_is_clamped_to_identity);
    RUN_TEST(test_overflow_saturates_to_int32_max);
    RUN_TEST(test_negative_pin_voltage_passes_through);
    return UNITY_END();
}
