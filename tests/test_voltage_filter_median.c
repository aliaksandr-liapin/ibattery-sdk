#include "unity.h"
#include "battery_voltage_filter.h"
#include <battery_sdk/battery_status.h>

static battery_voltage_filter_t filter;

void setUp(void)
{
    /* Default to window_size = 5 (odd, simple median). */
    battery_voltage_filter_init(&filter, 5);
}

void tearDown(void) {}

/* ── Null-pointer checks ──────────────────────────────────────────────────── */

void test_init_null_filter(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_voltage_filter_init(NULL, 5));
}

void test_update_null_filter(void)
{
    uint16_t out;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_voltage_filter_update(NULL, 3000, &out));
}

void test_update_null_output(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_voltage_filter_update(&filter, 3000, NULL));
}

/* ── Single sample ────────────────────────────────────────────────────────── */

void test_single_sample_returns_itself(void)
{
    uint16_t out = 0;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_voltage_filter_update(&filter, 3000, &out));
    TEST_ASSERT_EQUAL_UINT16(3000, out);
}

/* ── Three samples → middle ───────────────────────────────────────────────── */

void test_three_samples_return_middle(void)
{
    uint16_t out = 0;
    battery_voltage_filter_update(&filter, 3000, &out);
    battery_voltage_filter_update(&filter, 3200, &out);
    battery_voltage_filter_update(&filter, 3100, &out);
    /* sorted: 3000, 3100, 3200 → median 3100 */
    TEST_ASSERT_EQUAL_UINT16(3100, out);
}

/* ── Outlier rejection ────────────────────────────────────────────────────── */

void test_single_outlier_rejected(void)
{
    uint16_t out = 0;
    /* Steady 3700, one sag to 3200, back to 3700 */
    battery_voltage_filter_update(&filter, 3700, &out);
    battery_voltage_filter_update(&filter, 3700, &out);
    battery_voltage_filter_update(&filter, 3200, &out);
    battery_voltage_filter_update(&filter, 3700, &out);
    battery_voltage_filter_update(&filter, 3700, &out);
    /* sorted: 3200, 3700, 3700, 3700, 3700 → median 3700 */
    TEST_ASSERT_EQUAL_UINT16(3700, out);
}

void test_majority_outliers_overcome_signal(void)
{
    uint16_t out = 0;
    /* 3 sags out of 5 — median follows sag */
    battery_voltage_filter_update(&filter, 3700, &out);
    battery_voltage_filter_update(&filter, 3200, &out);
    battery_voltage_filter_update(&filter, 3200, &out);
    battery_voltage_filter_update(&filter, 3700, &out);
    battery_voltage_filter_update(&filter, 3200, &out);
    /* sorted: 3200, 3200, 3200, 3700, 3700 → median 3200 */
    TEST_ASSERT_EQUAL_UINT16(3200, out);
}

/* ── Even window: mean of two middle elements ─────────────────────────────── */

void test_even_window_mean_of_middles(void)
{
    battery_voltage_filter_t f;
    uint16_t out = 0;
    battery_voltage_filter_init(&f, 4);

    battery_voltage_filter_update(&f, 3000, &out);
    battery_voltage_filter_update(&f, 3100, &out);
    battery_voltage_filter_update(&f, 3200, &out);
    battery_voltage_filter_update(&f, 3300, &out);
    /* sorted: 3000, 3100, 3200, 3300 → mean(3100, 3200) = 3150 */
    TEST_ASSERT_EQUAL_UINT16(3150, out);
}

/* ── Already-sorted ───────────────────────────────────────────────────────── */

void test_already_sorted_input(void)
{
    uint16_t out = 0;
    battery_voltage_filter_update(&filter, 3000, &out);
    battery_voltage_filter_update(&filter, 3100, &out);
    battery_voltage_filter_update(&filter, 3200, &out);
    battery_voltage_filter_update(&filter, 3300, &out);
    battery_voltage_filter_update(&filter, 3400, &out);
    /* median 3200 */
    TEST_ASSERT_EQUAL_UINT16(3200, out);
}

/* ── Reverse-sorted ───────────────────────────────────────────────────────── */

void test_reverse_sorted_input(void)
{
    uint16_t out = 0;
    battery_voltage_filter_update(&filter, 3400, &out);
    battery_voltage_filter_update(&filter, 3300, &out);
    battery_voltage_filter_update(&filter, 3200, &out);
    battery_voltage_filter_update(&filter, 3100, &out);
    battery_voltage_filter_update(&filter, 3000, &out);
    /* median 3200 */
    TEST_ASSERT_EQUAL_UINT16(3200, out);
}

/* ── Circular wrap after window full ──────────────────────────────────────── */

void test_circular_wrap_after_full(void)
{
    battery_voltage_filter_t f;
    uint16_t out = 0;
    battery_voltage_filter_init(&f, 3);

    battery_voltage_filter_update(&f, 1000, &out);
    battery_voltage_filter_update(&f, 2000, &out);
    battery_voltage_filter_update(&f, 3000, &out);
    /* sorted: 1000, 2000, 3000 → median 2000 */
    TEST_ASSERT_EQUAL_UINT16(2000, out);

    /* Push 4000, oldest (1000) drops: buffer holds [2000, 3000, 4000] */
    battery_voltage_filter_update(&f, 4000, &out);
    /* sorted: 2000, 3000, 4000 → median 3000 */
    TEST_ASSERT_EQUAL_UINT16(3000, out);

    /* Push 5000: buffer holds [3000, 4000, 5000] */
    battery_voltage_filter_update(&f, 5000, &out);
    TEST_ASSERT_EQUAL_UINT16(4000, out);
}

/* ── Reset ────────────────────────────────────────────────────────────────── */

void test_reset_clears_state(void)
{
    uint16_t out = 0;
    battery_voltage_filter_update(&filter, 3000, &out);
    battery_voltage_filter_update(&filter, 3100, &out);
    battery_voltage_filter_update(&filter, 3200, &out);

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_voltage_filter_reset(&filter));

    /* After reset, first sample should be itself */
    battery_voltage_filter_update(&filter, 2500, &out);
    TEST_ASSERT_EQUAL_UINT16(2500, out);
}

/* ── Runner ───────────────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_null_filter);
    RUN_TEST(test_update_null_filter);
    RUN_TEST(test_update_null_output);
    RUN_TEST(test_single_sample_returns_itself);
    RUN_TEST(test_three_samples_return_middle);
    RUN_TEST(test_single_outlier_rejected);
    RUN_TEST(test_majority_outliers_overcome_signal);
    RUN_TEST(test_even_window_mean_of_middles);
    RUN_TEST(test_already_sorted_input);
    RUN_TEST(test_reverse_sorted_input);
    RUN_TEST(test_circular_wrap_after_full);
    RUN_TEST(test_reset_clears_state);

    return UNITY_END();
}
