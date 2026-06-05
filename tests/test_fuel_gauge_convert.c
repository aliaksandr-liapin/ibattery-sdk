#include "unity.h"
#include "battery_sdk/battery_fuel_gauge_convert.h"

void setUp(void) {}
void tearDown(void) {}

void test_mv_to_uv(void) {
    TEST_ASSERT_EQUAL_INT32(3300000, battery_fg_mv_to_uv(3300));
}
void test_current_sign_flip_and_scale(void) {
    TEST_ASSERT_EQUAL_INT32(-31700, battery_fg_ma_x100_to_ua(3170)); /* 31.70 mA discharge -> -31700 µA */
    TEST_ASSERT_EQUAL_INT32(5000, battery_fg_ma_x100_to_ua(-500));   /* charge -> positive µA */
}
void test_temp_cdegc_to_decikelvin(void) {
    TEST_ASSERT_EQUAL_UINT16(2967, battery_fg_cdegc_to_decikelvin(2356)); /* 23.56°C -> 296.7K */
    TEST_ASSERT_EQUAL_UINT16(2731, battery_fg_cdegc_to_decikelvin(0));
}
void test_soc_to_pct_round_clamp(void) {
    TEST_ASSERT_EQUAL_UINT8(73, battery_fg_socx100_to_pct(7310));
    TEST_ASSERT_EQUAL_UINT8(100, battery_fg_socx100_to_pct(10000));
    TEST_ASSERT_EQUAL_UINT8(100, battery_fg_socx100_to_pct(12000));
    TEST_ASSERT_EQUAL_UINT8(1, battery_fg_socx100_to_pct(99));
}
void test_mah_to_uah_clamp(void) {
    TEST_ASSERT_EQUAL_UINT32(5380, battery_fg_mahx100_to_uah(538));
    TEST_ASSERT_EQUAL_UINT32(0, battery_fg_mahx100_to_uah(-10));
}
void test_full_charge_uah_with_soh(void) {
    TEST_ASSERT_EQUAL_UINT32(7310, battery_fg_full_charge_uah(10, 7310)); /* 10mAh * 73.10% */
    TEST_ASSERT_EQUAL_UINT32(10000, battery_fg_full_charge_uah(10, 0));   /* unknown -> rated */
}
void test_cycles_to_centi(void) {
    TEST_ASSERT_EQUAL_UINT32(500, battery_fg_cycles_to_centi(5));
}
void test_decikelvin_clamps(void) {
    TEST_ASSERT_EQUAL_UINT16(0, battery_fg_cdegc_to_decikelvin(-30000));   /* -300°C -> 0 */
    TEST_ASSERT_EQUAL_UINT16(65535, battery_fg_cdegc_to_decikelvin(700000)); /* 7000°C -> UINT16_MAX */
}
void test_full_charge_uah_negative_rated(void) {
    TEST_ASSERT_EQUAL_UINT32(0, battery_fg_full_charge_uah(-5, 7310)); /* negative rated -> 0 */
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_mv_to_uv);
    RUN_TEST(test_current_sign_flip_and_scale);
    RUN_TEST(test_temp_cdegc_to_decikelvin);
    RUN_TEST(test_soc_to_pct_round_clamp);
    RUN_TEST(test_mah_to_uah_clamp);
    RUN_TEST(test_full_charge_uah_with_soh);
    RUN_TEST(test_cycles_to_centi);
    RUN_TEST(test_decikelvin_clamps);
    RUN_TEST(test_full_charge_uah_negative_rated);
    return UNITY_END();
}
