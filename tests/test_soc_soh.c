/*
 * Unit tests for battery_soh (Phase 8d).
 * Kconfig values injected via tests/CMakeLists.txt:
 *   CONFIG_BATTERY_SOC_SOH_ALPHA_X1000   = 500
 *   CONFIG_BATTERY_SOC_SOH_REJECT_LO_PCT = 30
 *   CONFIG_BATTERY_SOC_SOH_REJECT_HI_PCT = 120
 */
#include "unity.h"
#include <battery_sdk/battery_soh.h>
#include <battery_sdk/battery_status.h>

#define RATED 22000  /* 220.00 mAh (CR2032), x100 */

void setUp(void) { battery_soh_init(RATED); }
void tearDown(void) {}

void test_init_reports_100pct(void)
{
    uint16_t soh;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_soh_get_pct_x100(&soh));
    TEST_ASSERT_EQUAL_UINT16(10000, soh);
}

void test_init_learned_equals_rated(void)
{
    int32_t cap;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
        battery_soh_get_learned_capacity_mah_x100(&cap));
    TEST_ASSERT_EQUAL_INT32(RATED, cap);
}

void test_null_arg_rejected(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
        battery_soh_get_pct_x100(NULL));
}

void test_healthy_excursion_keeps_100pct(void)
{
    /* Healthy: q_before_empty == 0 -> measured == rated -> learned unchanged. */
    battery_soh_note_full_anchor();
    battery_soh_observe_empty_anchor(0);
    uint16_t soh; battery_soh_get_pct_x100(&soh);
    TEST_ASSERT_EQUAL_UINT16(10000, soh);
}

void test_aged_excursion_moves_toward_measured(void)
{
    /* Aged 80%: q_before = 20% of rated = 4400 -> measured = 17600.
     * alpha=500 -> learned = 22000 + (17600-22000)*500/1000 = 19800.
     * SoH = 19800/22000*10000 = 9000. */
    battery_soh_note_full_anchor();
    battery_soh_observe_empty_anchor(4400);
    int32_t cap; battery_soh_get_learned_capacity_mah_x100(&cap);
    TEST_ASSERT_EQUAL_INT32(19800, cap);
    uint16_t soh; battery_soh_get_pct_x100(&soh);
    TEST_ASSERT_EQUAL_UINT16(9000, soh);
}

void test_unarmed_observe_is_noop(void)
{
    /* No note_full_anchor() -> observe must not change learned. */
    battery_soh_observe_empty_anchor(4400);
    int32_t cap; battery_soh_get_learned_capacity_mah_x100(&cap);
    TEST_ASSERT_EQUAL_INT32(RATED, cap);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_reports_100pct);
    RUN_TEST(test_init_learned_equals_rated);
    RUN_TEST(test_null_arg_rejected);
    RUN_TEST(test_healthy_excursion_keeps_100pct);
    RUN_TEST(test_aged_excursion_moves_toward_measured);
    RUN_TEST(test_unarmed_observe_is_noop);
    return UNITY_END();
}
