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

void test_repeated_aged_excursions_converge(void)
{
    /* Repeated 80% excursions -> learned converges toward 17600 (80%). */
    for (int i = 0; i < 12; i++) {
        battery_soh_note_full_anchor();
        battery_soh_observe_empty_anchor(4400);
    }
    int32_t cap; battery_soh_get_learned_capacity_mah_x100(&cap);
    TEST_ASSERT_INT32_WITHIN(50, 17600, cap);  /* ~80% */
}

void test_implausible_low_rejected(void)
{
    /* q_before = 90% rated -> measured = 10% < REJECT_LO (30%) -> rejected. */
    battery_soh_note_full_anchor();
    battery_soh_observe_empty_anchor((RATED * 90) / 100);
    int32_t cap; battery_soh_get_learned_capacity_mah_x100(&cap);
    TEST_ASSERT_EQUAL_INT32(RATED, cap);
}

void test_implausible_high_rejected(void)
{
    /* q_before negative -> measured > rated; > REJECT_HI (120%) when large. */
    battery_soh_note_full_anchor();
    battery_soh_observe_empty_anchor(-(RATED / 2));  /* measured = 1.5*rated */
    int32_t cap; battery_soh_get_learned_capacity_mah_x100(&cap);
    TEST_ASSERT_EQUAL_INT32(RATED, cap);
}

void test_reset_restores_100pct(void)
{
    battery_soh_note_full_anchor();
    battery_soh_observe_empty_anchor(4400);  /* drops below 100% */
    battery_soh_reset();
    uint16_t soh; battery_soh_get_pct_x100(&soh);
    TEST_ASSERT_EQUAL_UINT16(10000, soh);
}

void test_large_capacity_no_int32_overflow(void)
{
    /* Regression: get_pct's (learned * 10000) must use an int64 intermediate.
     * 2000 mAh pack -> rated_x100 = 200000. Push learned above rated so the
     * product exceeds INT32_MAX (2.147e9): 220000 * 10000 = 2.2e9. With int32
     * this overflows to a negative value (clamped to 0); with int64 it is
     * 11000, clamped to 10000. */
    battery_soh_init(200000);
    battery_soh_note_full_anchor();
    /* measured = 200000 - (-40000) = 240000 = hi guard (120%), passes.
     * learned = 200000 + (240000-200000)*500/1000 = 220000. */
    battery_soh_observe_empty_anchor(-40000);
    uint16_t soh; battery_soh_get_pct_x100(&soh);
    TEST_ASSERT_EQUAL_UINT16(10000, soh);
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
    RUN_TEST(test_repeated_aged_excursions_converge);
    RUN_TEST(test_implausible_low_rejected);
    RUN_TEST(test_implausible_high_rejected);
    RUN_TEST(test_reset_restores_100pct);
    RUN_TEST(test_large_capacity_no_int32_overflow);
    return UNITY_END();
}
