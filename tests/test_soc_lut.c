#include "unity.h"
#include "battery_soc_lut.h"
#include <battery_sdk/battery_status.h>

void setUp(void) {}
void tearDown(void) {}

/* ── Null / invalid argument checks ───────────────────────────────────────── */

void test_null_lut(void)
{
    uint16_t soc;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_soc_lut_interpolate(NULL, 2800, &soc));
}

void test_null_output(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_soc_lut_interpolate(&battery_soc_lut_cr2032,
                                                     2800, NULL));
}

void test_empty_lut(void)
{
    uint16_t soc;
    battery_soc_lut_t empty = { .entries = NULL, .count = 0 };
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_soc_lut_interpolate(&empty, 2800, &soc));
}

/* ── Exact table points ───────────────────────────────────────────────────── */

void test_exact_3000mv_is_100pct(void)
{
    uint16_t soc;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_cr2032,
                                                     3000, &soc));
    TEST_ASSERT_EQUAL_UINT16(10000, soc);  /* 100.00% */
}

void test_exact_2000mv_is_0pct(void)
{
    uint16_t soc;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_cr2032,
                                                     2000, &soc));
    TEST_ASSERT_EQUAL_UINT16(0, soc);
}

void test_exact_2700mv_is_50pct(void)
{
    uint16_t soc;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_cr2032,
                                                     2700, &soc));
    TEST_ASSERT_EQUAL_UINT16(5000, soc);
}

/* ── Clamping ─────────────────────────────────────────────────────────────── */

void test_above_max_clamps_to_100pct(void)
{
    uint16_t soc;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_cr2032,
                                                     3300, &soc));
    TEST_ASSERT_EQUAL_UINT16(10000, soc);
}

void test_below_min_clamps_to_0pct(void)
{
    uint16_t soc;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_cr2032,
                                                     1800, &soc));
    TEST_ASSERT_EQUAL_UINT16(0, soc);
}

/* ── Linear interpolation between entries ─────────────────────────────────── */

void test_interpolation_midpoint_2950mv(void)
{
    /* Between 3000 (100%) and 2900 (90%): midpoint = 95.00% = 9500 */
    uint16_t soc;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_cr2032,
                                                     2950, &soc));
    TEST_ASSERT_EQUAL_UINT16(9500, soc);
}

void test_interpolation_2850mv(void)
{
    /* Between 2900 (90%) and 2800 (70%):
     * offset = 2850 - 2800 = 50
     * span = 100
     * soc_span = 9000 - 7000 = 2000
     * result = 7000 + (50 * 2000) / 100 = 7000 + 1000 = 8000 = 80.00%
     */
    uint16_t soc;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_cr2032,
                                                     2850, &soc));
    TEST_ASSERT_EQUAL_UINT16(8000, soc);
}

void test_interpolation_2300mv(void)
{
    /* Between 2400 (10%) and 2200 (5%):
     * offset = 2300 - 2200 = 100
     * span = 200
     * soc_span = 1000 - 500 = 500
     * result = 500 + (100 * 500) / 200 = 500 + 250 = 750 = 7.50%
     */
    uint16_t soc;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_cr2032,
                                                     2300, &soc));
    TEST_ASSERT_EQUAL_UINT16(750, soc);
}

/* ══ LiPo 1S LUT tests ═══════════════════════════════════════════════════════ */

/* ── Exact table points ───────────────────────────────────────────────────── */

void test_lipo_4200mv_is_100pct(void)
{
    uint16_t soc;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_lipo_1s,
                                                     4200, &soc));
    TEST_ASSERT_EQUAL_UINT16(10000, soc);
}

void test_lipo_3000mv_is_0pct(void)
{
    uint16_t soc;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_lipo_1s,
                                                     3000, &soc));
    TEST_ASSERT_EQUAL_UINT16(0, soc);
}

void test_lipo_3870mv_is_55pct(void)
{
    uint16_t soc;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_lipo_1s,
                                                     3870, &soc));
    TEST_ASSERT_EQUAL_UINT16(5500, soc);
}

/* ── Clamping ─────────────────────────────────────────────────────────────── */

void test_lipo_above_max_clamps(void)
{
    uint16_t soc;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_lipo_1s,
                                                     4500, &soc));
    TEST_ASSERT_EQUAL_UINT16(10000, soc);
}

void test_lipo_below_min_clamps(void)
{
    uint16_t soc;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_lipo_1s,
                                                     2500, &soc));
    TEST_ASSERT_EQUAL_UINT16(0, soc);
}

/* ── Interpolation in plateau region ──────────────────────────────────────── */

void test_lipo_interpolation_3950mv(void)
{
    /* Between 3980 (75%) and 3920 (65%):
     * offset = 3950 - 3920 = 30
     * span = 60
     * soc_span = 7500 - 6500 = 1000
     * result = 6500 + (30 * 1000) / 60 = 6500 + 500 = 7000 = 70.00%
     */
    uint16_t soc;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_lipo_1s,
                                                     3950, &soc));
    TEST_ASSERT_EQUAL_UINT16(7000, soc);
}

/* ── Interpolation in knee region ─────────────────────────────────────────── */

void test_lipo_interpolation_3745mv(void)
{
    /* Between 3790 (30%) and 3700 (10%):
     * offset = 3745 - 3700 = 45
     * span = 90
     * soc_span = 3000 - 1000 = 2000
     * result = 1000 + (45 * 2000) / 90 = 1000 + 1000 = 2000 = 20.00%
     */
    uint16_t soc;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_lipo_1s,
                                                     3745, &soc));
    TEST_ASSERT_EQUAL_UINT16(2000, soc);
}

/* ── Interpolation in steep cliff region ──────────────────────────────────── */

void test_lipo_interpolation_3600mv(void)
{
    /* Between 3700 (10%) and 3500 (3%):
     * offset = 3600 - 3500 = 100
     * span = 200
     * soc_span = 1000 - 300 = 700
     * result = 300 + (100 * 700) / 200 = 300 + 350 = 650 = 6.50%
     */
    uint16_t soc;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_lut_interpolate(&battery_soc_lut_lipo_1s,
                                                     3600, &soc));
    TEST_ASSERT_EQUAL_UINT16(650, soc);
}

/* ── Runner ───────────────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    /* CR2032 tests */
    RUN_TEST(test_null_lut);
    RUN_TEST(test_null_output);
    RUN_TEST(test_empty_lut);
    RUN_TEST(test_exact_3000mv_is_100pct);
    RUN_TEST(test_exact_2000mv_is_0pct);
    RUN_TEST(test_exact_2700mv_is_50pct);
    RUN_TEST(test_above_max_clamps_to_100pct);
    RUN_TEST(test_below_min_clamps_to_0pct);
    RUN_TEST(test_interpolation_midpoint_2950mv);
    RUN_TEST(test_interpolation_2850mv);
    RUN_TEST(test_interpolation_2300mv);

    /* LiPo 1S tests */
    RUN_TEST(test_lipo_4200mv_is_100pct);
    RUN_TEST(test_lipo_3000mv_is_0pct);
    RUN_TEST(test_lipo_3870mv_is_55pct);
    RUN_TEST(test_lipo_above_max_clamps);
    RUN_TEST(test_lipo_below_min_clamps);
    RUN_TEST(test_lipo_interpolation_3950mv);
    RUN_TEST(test_lipo_interpolation_3745mv);
    RUN_TEST(test_lipo_interpolation_3600mv);

    return UNITY_END();
}
