#include "unity.h"
#include <battery_sdk/battery_soc_estimator.h>
#include <battery_sdk/battery_status.h>
#include <stddef.h>
#include <stdint.h>

extern void mock_voltage_set_mv(uint16_t mv);
extern void mock_voltage_set_rc(int rc);
extern void mock_sdk_set_uptime_ms(uint32_t ms);

void setUp(void)
{
    mock_voltage_set_mv(3700);
    mock_voltage_set_rc(BATTERY_STATUS_OK);
    mock_sdk_set_uptime_ms(0);
    battery_soc_estimator_init();
}

void tearDown(void) {}

void test_first_sample_no_slew_limit(void)
{
    uint16_t soc;
    mock_voltage_set_mv(4200);  /* full charge LUT = 100% */
    mock_sdk_set_uptime_ms(0);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_estimator_get_pct_x100(&soc));
    /* First call bypasses slew limit — instant 100% */
    TEST_ASSERT_EQUAL_UINT16(10000, soc);
}

void test_slew_cap_on_voltage_drop(void)
{
    uint16_t soc;

    /* Seed at full */
    mock_voltage_set_mv(4200);
    mock_sdk_set_uptime_ms(0);
    battery_soc_estimator_get_pct_x100(&soc);
    TEST_ASSERT_EQUAL_UINT16(10000, soc);

    /* 1 second later, voltage drops to 3000 (LUT = ~0%) */
    mock_voltage_set_mv(3000);
    mock_sdk_set_uptime_ms(1000);
    battery_soc_estimator_get_pct_x100(&soc);

    /* Slew rate 5%/min = 5*100/60 ≈ 8.33 x100/sec.
     * In 1000ms: max delta ≈ 8 x100 units.
     * Should drop AT MOST ~10 from 10000. */
    TEST_ASSERT_TRUE(soc > 9980);
    TEST_ASSERT_TRUE(soc < 10001);
}

void test_slew_cap_on_voltage_rise(void)
{
    uint16_t soc;

    /* Seed at empty */
    mock_voltage_set_mv(3000);
    mock_sdk_set_uptime_ms(0);
    battery_soc_estimator_get_pct_x100(&soc);

    /* 1s later, voltage jumps to full */
    mock_voltage_set_mv(4200);
    mock_sdk_set_uptime_ms(1000);
    battery_soc_estimator_get_pct_x100(&soc);

    /* Should rise AT MOST ~10 from initial 0% */
    TEST_ASSERT_TRUE(soc < 50);
}

void test_long_elapsed_allows_full_change(void)
{
    uint16_t soc;
    mock_voltage_set_mv(4200);
    mock_sdk_set_uptime_ms(0);
    battery_soc_estimator_get_pct_x100(&soc);

    /* 1 hour later: max delta = 60 * 5 = 300% — way more than 100% */
    mock_voltage_set_mv(3000);
    mock_sdk_set_uptime_ms(3600 * 1000);
    battery_soc_estimator_get_pct_x100(&soc);

    /* Allowed full change */
    TEST_ASSERT_TRUE(soc < 500);
}

void test_zero_dt_freezes_value(void)
{
    uint16_t soc1, soc2;
    mock_voltage_set_mv(3700);
    mock_sdk_set_uptime_ms(0);
    battery_soc_estimator_get_pct_x100(&soc1);

    /* Same timestamp, big voltage jump */
    mock_voltage_set_mv(4200);
    mock_sdk_set_uptime_ms(0);  /* dt = 0 */
    battery_soc_estimator_get_pct_x100(&soc2);

    /* dt=0 means max_delta=0, value frozen */
    TEST_ASSERT_EQUAL_UINT16(soc1, soc2);
}

void test_slow_change_unaffected(void)
{
    uint16_t soc1, soc2;

    mock_voltage_set_mv(3830);
    mock_sdk_set_uptime_ms(0);
    battery_soc_estimator_get_pct_x100(&soc1);

    /* 60s later, small voltage drop */
    mock_voltage_set_mv(3820);
    mock_sdk_set_uptime_ms(60 * 1000);
    battery_soc_estimator_get_pct_x100(&soc2);

    /* Real change is small, slew cap (60 * 5 = 300% in 1min) doesn't bite */
    TEST_ASSERT_TRUE(soc2 < soc1);
    TEST_ASSERT_TRUE((soc1 - soc2) < 500);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_first_sample_no_slew_limit);
    RUN_TEST(test_slew_cap_on_voltage_drop);
    RUN_TEST(test_slew_cap_on_voltage_rise);
    RUN_TEST(test_long_elapsed_allows_full_change);
    RUN_TEST(test_zero_dt_freezes_value);
    RUN_TEST(test_slow_change_unaffected);
    return UNITY_END();
}
