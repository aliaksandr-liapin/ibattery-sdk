#include "unity.h"
#include "battery_ntc_lut.h"
#include <battery_sdk/battery_status.h>

void setUp(void) {}
void tearDown(void) {}

/* ══ battery_ntc_resistance_from_mv() tests ══════════════════════════════════ */

/* ── Null / invalid argument checks ───────────────────────────────────────── */

void test_resistance_null_output(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_ntc_resistance_from_mv(10000, 3300, 1650,
                                                        NULL));
}

void test_resistance_zero_vdd(void)
{
    uint32_t r;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_ntc_resistance_from_mv(10000, 0, 1650, &r));
}

/* ── Edge cases ───────────────────────────────────────────────────────────── */

void test_resistance_adc_equals_vdd_open_circuit(void)
{
    /* ADC >= VDD means NTC is disconnected / open circuit */
    uint32_t r;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_IO,
                          battery_ntc_resistance_from_mv(10000, 3300, 3300,
                                                        &r));
}

void test_resistance_adc_above_vdd_open_circuit(void)
{
    uint32_t r;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_IO,
                          battery_ntc_resistance_from_mv(10000, 3300, 3500,
                                                        &r));
}

void test_resistance_adc_zero_shorted(void)
{
    /* ADC = 0 means NTC is shorted to GND → resistance = 0 */
    uint32_t r;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_ntc_resistance_from_mv(10000, 3300, 0, &r));
    TEST_ASSERT_EQUAL_UINT32(0, r);
}

/* ── Normal operation ─────────────────────────────────────────────────────── */

void test_resistance_half_vdd_equals_pullup(void)
{
    /*
     * When ADC = VDD/2, NTC = pullup (voltage divider balanced).
     * R = 10000 * 1650 / (3300 - 1650) = 10000 * 1650 / 1650 = 10000
     */
    uint32_t r;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_ntc_resistance_from_mv(10000, 3300, 1650,
                                                        &r));
    TEST_ASSERT_EQUAL_UINT32(10000, r);
}

void test_resistance_low_voltage_high_resistance(void)
{
    /*
     * Low ADC voltage → NTC is cold → high resistance.
     * R = 10000 * 330 / (3300 - 330) = 10000 * 330 / 2970 = 1111
     */
    uint32_t r;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_ntc_resistance_from_mv(10000, 3300, 330,
                                                        &r));
    TEST_ASSERT_EQUAL_UINT32(1111, r);
}

void test_resistance_high_voltage_low_resistance(void)
{
    /*
     * High ADC voltage → NTC is hot → low resistance.
     * R = 10000 * 3000 / (3300 - 3000) = 10000 * 3000 / 300 = 100000
     */
    uint32_t r;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_ntc_resistance_from_mv(10000, 3300, 3000,
                                                        &r));
    TEST_ASSERT_EQUAL_UINT32(100000, r);
}

/* ══ battery_ntc_lut_interpolate() tests ═════════════════════════════════════ */

/* ── Null / invalid argument checks ───────────────────────────────────────── */

void test_interpolate_null_lut(void)
{
    int32_t temp;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_ntc_lut_interpolate(NULL, 10000, &temp));
}

void test_interpolate_null_output(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_ntc_lut_interpolate(&battery_ntc_lut_10k_3950,
                                                     10000, NULL));
}

void test_interpolate_empty_lut(void)
{
    int32_t temp;
    battery_ntc_lut_t empty = { .entries = NULL, .count = 0 };
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_ntc_lut_interpolate(&empty, 10000, &temp));
}

/* ── Exact table points ───────────────────────────────────────────────────── */

void test_interpolate_exact_25c_reference(void)
{
    /* 10000 Ω = 25.00 °C (reference point) */
    int32_t temp;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_ntc_lut_interpolate(&battery_ntc_lut_10k_3950,
                                                     10000, &temp));
    TEST_ASSERT_EQUAL_INT32(2500, temp);
}

void test_interpolate_exact_0c(void)
{
    /* 33640 Ω = 0.00 °C */
    int32_t temp;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_ntc_lut_interpolate(&battery_ntc_lut_10k_3950,
                                                     33640, &temp));
    TEST_ASSERT_EQUAL_INT32(0, temp);
}

void test_interpolate_exact_negative_40c(void)
{
    /* 401600 Ω = -40.00 °C (coldest entry) */
    int32_t temp;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_ntc_lut_interpolate(&battery_ntc_lut_10k_3950,
                                                     401600, &temp));
    TEST_ASSERT_EQUAL_INT32(-4000, temp);
}

void test_interpolate_exact_125c(void)
{
    /* 359 Ω = 125.00 °C (hottest entry) */
    int32_t temp;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_ntc_lut_interpolate(&battery_ntc_lut_10k_3950,
                                                     359, &temp));
    TEST_ASSERT_EQUAL_INT32(12500, temp);
}

/* ── Clamping ─────────────────────────────────────────────────────────────── */

void test_interpolate_above_max_resistance_clamps_cold(void)
{
    /* Very high resistance (extremely cold) → clamp to -40 °C */
    int32_t temp;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_ntc_lut_interpolate(&battery_ntc_lut_10k_3950,
                                                     999999, &temp));
    TEST_ASSERT_EQUAL_INT32(-4000, temp);
}

void test_interpolate_below_min_resistance_clamps_hot(void)
{
    /* Very low resistance (extremely hot) → clamp to 125 °C */
    int32_t temp;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_ntc_lut_interpolate(&battery_ntc_lut_10k_3950,
                                                     100, &temp));
    TEST_ASSERT_EQUAL_INT32(12500, temp);
}

/* ── Interpolation between entries ────────────────────────────────────────── */

void test_interpolate_midpoint_20c_25c(void)
{
    /*
     * Between 12520 Ω (20°C) and 10000 Ω (25°C):
     * Midpoint resistance = 11260 Ω
     * dr_total = 12520 - 10000 = 2520
     * dr_from_upper = 12520 - 11260 = 1260
     * dt = 2500 - 2000 = 500
     * result = 2000 + (1260 * 500) / 2520 = 2000 + 250 = 2250 = 22.50 °C
     */
    int32_t temp;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_ntc_lut_interpolate(&battery_ntc_lut_10k_3950,
                                                     11260, &temp));
    TEST_ASSERT_EQUAL_INT32(2250, temp);
}

void test_interpolate_negative_range(void)
{
    /*
     * Between 200500 Ω (-30°C) and 105300 Ω (-20°C):
     * Resistance = 152900 Ω (midpoint)
     * dr_total = 200500 - 105300 = 95200
     * dr_from_upper = 200500 - 152900 = 47600
     * dt = -2000 - (-3000) = 1000
     * result = -3000 + (47600 * 1000) / 95200 = -3000 + 500 = -2500 = -25.00 °C
     */
    int32_t temp;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_ntc_lut_interpolate(&battery_ntc_lut_10k_3950,
                                                     152900, &temp));
    TEST_ASSERT_EQUAL_INT32(-2500, temp);
}

void test_interpolate_across_zero(void)
{
    /*
     * Between 33640 Ω (0°C) and 20200 Ω (10°C):
     * Resistance = 26920 Ω (midpoint)
     * dr_total = 33640 - 20200 = 13440
     * dr_from_upper = 33640 - 26920 = 6720
     * dt = 1000 - 0 = 1000
     * result = 0 + (6720 * 1000) / 13440 = 500 = 5.00 °C
     */
    int32_t temp;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_ntc_lut_interpolate(&battery_ntc_lut_10k_3950,
                                                     26920, &temp));
    TEST_ASSERT_EQUAL_INT32(500, temp);
}

void test_interpolate_hot_range(void)
{
    /*
     * Between 1270 Ω (80°C) and 697 Ω (100°C):
     * Resistance = 983 Ω (approximate midpoint)
     * dr_total = 1270 - 697 = 573
     * dr_from_upper = 1270 - 983 = 287
     * dt = 10000 - 8000 = 2000
     * result = 8000 + (287 * 2000) / 573 = 8000 + 1001 = 9001 = 90.01 °C
     */
    int32_t temp;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_ntc_lut_interpolate(&battery_ntc_lut_10k_3950,
                                                     983, &temp));
    TEST_ASSERT_EQUAL_INT32(9001, temp);
}

/* ── Runner ───────────────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    /* Resistance conversion tests */
    RUN_TEST(test_resistance_null_output);
    RUN_TEST(test_resistance_zero_vdd);
    RUN_TEST(test_resistance_adc_equals_vdd_open_circuit);
    RUN_TEST(test_resistance_adc_above_vdd_open_circuit);
    RUN_TEST(test_resistance_adc_zero_shorted);
    RUN_TEST(test_resistance_half_vdd_equals_pullup);
    RUN_TEST(test_resistance_low_voltage_high_resistance);
    RUN_TEST(test_resistance_high_voltage_low_resistance);

    /* LUT interpolation tests */
    RUN_TEST(test_interpolate_null_lut);
    RUN_TEST(test_interpolate_null_output);
    RUN_TEST(test_interpolate_empty_lut);
    RUN_TEST(test_interpolate_exact_25c_reference);
    RUN_TEST(test_interpolate_exact_0c);
    RUN_TEST(test_interpolate_exact_negative_40c);
    RUN_TEST(test_interpolate_exact_125c);
    RUN_TEST(test_interpolate_above_max_resistance_clamps_cold);
    RUN_TEST(test_interpolate_below_min_resistance_clamps_hot);
    RUN_TEST(test_interpolate_midpoint_20c_25c);
    RUN_TEST(test_interpolate_negative_range);
    RUN_TEST(test_interpolate_across_zero);
    RUN_TEST(test_interpolate_hot_range);

    return UNITY_END();
}
