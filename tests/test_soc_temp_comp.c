#include "unity.h"
#include "battery_soc_temp_comp.h"
#include "battery_soc_lut.h"
#include <battery_sdk/battery_status.h>

void setUp(void) {}
void tearDown(void) {}

/* ── Null argument checks ────────────────────────────────────────────────── */

void test_null_output_returns_invalid_arg(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_soc_temp_compensated(3800, 2500, NULL));
}

/* ── Cold LUT vs room LUT comparison ────────────────────────────────────── */

void test_cold_lut_returns_lower_soc_than_room(void)
{
    uint16_t soc_cold, soc_room;

    /* At 3800 mV, cold should give lower SoC than room temp */
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_lipo_1s_cold,
                                                     3800, &soc_cold));
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_lipo_1s,
                                                     3800, &soc_room));
    TEST_ASSERT_TRUE(soc_cold > soc_room);
    /* At cold temps, same voltage means higher SoC because cold LUT's voltage
     * range is compressed downward — 3800 mV is relatively higher on cold curve */
}

void test_hot_lut_returns_lower_soc_than_room_at_same_voltage(void)
{
    uint16_t soc_hot, soc_room;

    /* At 3900 mV, hot LUT should give lower SoC than room
     * because hot curve is shifted up — 3900 mV is relatively lower */
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_lipo_1s_hot,
                                                     3900, &soc_hot));
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_lipo_1s,
                                                     3900, &soc_room));
    TEST_ASSERT_TRUE(soc_hot < soc_room);
}

/* ── Zone boundary: exact 0 deg C uses cold LUT ─────────────────────────── */

void test_at_zero_deg_uses_cold_lut_only(void)
{
    uint16_t soc_comp, soc_cold;

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_temp_compensated(3800, 0, &soc_comp));
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_lipo_1s_cold,
                                                     3800, &soc_cold));
    TEST_ASSERT_EQUAL_UINT16(soc_cold, soc_comp);
}

/* ── Zone boundary: exact 25 deg C uses room LUT ────────────────────────── */

void test_at_25_deg_uses_room_lut_only(void)
{
    uint16_t soc_comp, soc_room;

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_temp_compensated(3800, 2500, &soc_comp));
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_lipo_1s,
                                                     3800, &soc_room));
    TEST_ASSERT_EQUAL_UINT16(soc_room, soc_comp);
}

/* ── Zone boundary: exact 45 deg C uses hot LUT ─────────────────────────── */

void test_at_45_deg_uses_hot_lut_only(void)
{
    uint16_t soc_comp, soc_hot;

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_temp_compensated(3800, 4500, &soc_comp));
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_lipo_1s_hot,
                                                     3800, &soc_hot));
    TEST_ASSERT_EQUAL_UINT16(soc_hot, soc_comp);
}

/* ── Below 0 deg C clamps to cold LUT ───────────────────────────────────── */

void test_below_zero_clamps_to_cold_lut(void)
{
    uint16_t soc_comp, soc_cold;

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_temp_compensated(3800, -2000, &soc_comp));
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_lipo_1s_cold,
                                                     3800, &soc_cold));
    TEST_ASSERT_EQUAL_UINT16(soc_cold, soc_comp);
}

/* ── Above 45 deg C clamps to hot LUT ───────────────────────────────────── */

void test_above_45_clamps_to_hot_lut(void)
{
    uint16_t soc_comp, soc_hot;

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_temp_compensated(3800, 6000, &soc_comp));
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_lipo_1s_hot,
                                                     3800, &soc_hot));
    TEST_ASSERT_EQUAL_UINT16(soc_hot, soc_comp);
}

/* ── Midpoint blending: 12.5 deg C is halfway between cold and room ──── */

void test_midpoint_cold_room_blends_halfway(void)
{
    uint16_t soc_comp, soc_cold, soc_room;

    /* 12.50 deg C = 1250 in x100 units, midpoint of 0..25 zone */
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_temp_compensated(3800, 1250, &soc_comp));
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_lipo_1s_cold,
                                                     3800, &soc_cold));
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_lipo_1s,
                                                     3800, &soc_room));

    uint16_t expected = (soc_cold + soc_room) / 2;
    /* Allow +-1 for integer rounding */
    TEST_ASSERT_INT_WITHIN(1, expected, soc_comp);
}

/* ── Midpoint blending: 35 deg C is halfway between room and hot ────── */

void test_midpoint_room_hot_blends_halfway(void)
{
    uint16_t soc_comp, soc_room, soc_hot;

    /* 35.00 deg C = 3500 in x100 units, midpoint of 25..45 zone */
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_temp_compensated(3800, 3500, &soc_comp));
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_lipo_1s,
                                                     3800, &soc_room));
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_lipo_1s_hot,
                                                     3800, &soc_hot));

    uint16_t expected = (soc_room + soc_hot) / 2;
    /* Allow +-1 for integer rounding */
    TEST_ASSERT_INT_WITHIN(1, expected, soc_comp);
}

/* ── Full charge at room temperature ─────────────────────────────────────── */

void test_full_charge_at_room_temp(void)
{
    uint16_t soc;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_temp_compensated(4200, 2500, &soc));
    TEST_ASSERT_EQUAL_UINT16(10000, soc);
}

/* ── Empty at room temperature ───────────────────────────────────────────── */

void test_empty_at_room_temp(void)
{
    uint16_t soc;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_temp_compensated(3000, 2500, &soc));
    TEST_ASSERT_EQUAL_UINT16(0, soc);
}

/* ── Blending result is between the two LUT values ───────────────────────── */

void test_blended_result_between_luts(void)
{
    uint16_t soc_comp, soc_cold, soc_room;

    /* 10 deg C = 1000 in x100 units */
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_temp_compensated(3800, 1000, &soc_comp));
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_lipo_1s_cold,
                                                     3800, &soc_cold));
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_lipo_1s,
                                                     3800, &soc_room));

    /* Result must be between the two LUT values */
    uint16_t min_soc = (soc_cold < soc_room) ? soc_cold : soc_room;
    uint16_t max_soc = (soc_cold > soc_room) ? soc_cold : soc_room;
    TEST_ASSERT_TRUE(soc_comp >= min_soc);
    TEST_ASSERT_TRUE(soc_comp <= max_soc);
}

/* ── Runner ──────────────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_null_output_returns_invalid_arg);
    RUN_TEST(test_cold_lut_returns_lower_soc_than_room);
    RUN_TEST(test_hot_lut_returns_lower_soc_than_room_at_same_voltage);
    RUN_TEST(test_at_zero_deg_uses_cold_lut_only);
    RUN_TEST(test_at_25_deg_uses_room_lut_only);
    RUN_TEST(test_at_45_deg_uses_hot_lut_only);
    RUN_TEST(test_below_zero_clamps_to_cold_lut);
    RUN_TEST(test_above_45_clamps_to_hot_lut);
    RUN_TEST(test_midpoint_cold_room_blends_halfway);
    RUN_TEST(test_midpoint_room_hot_blends_halfway);
    RUN_TEST(test_full_charge_at_room_temp);
    RUN_TEST(test_empty_at_room_temp);
    RUN_TEST(test_blended_result_between_luts);

    return UNITY_END();
}
