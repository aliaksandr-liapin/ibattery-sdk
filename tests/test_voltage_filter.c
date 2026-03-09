#include "unity.h"
#include "battery_voltage_filter.h"
#include <battery_sdk/battery_status.h>

static battery_voltage_filter_t filter;

void setUp(void)
{
    /* Fresh filter before each test */
    battery_voltage_filter_init(&filter, BATTERY_VOLTAGE_FILTER_DEFAULT_WINDOW_SIZE);
}

void tearDown(void) {}

/* ── Null-pointer checks ──────────────────────────────────────────────────── */

void test_init_null_filter(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_voltage_filter_init(NULL, 4));
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

void test_get_null_filter(void)
{
    uint16_t out;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_voltage_filter_get(NULL, &out));
}

/* ── Single sample ────────────────────────────────────────────────────────── */

void test_single_sample_returns_itself(void)
{
    uint16_t out = 0;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_voltage_filter_update(&filter, 3000, &out));
    TEST_ASSERT_EQUAL_UINT16(3000, out);
}

/* ── Averaging ────────────────────────────────────────────────────────────── */

void test_two_samples_averaged(void)
{
    uint16_t out = 0;
    battery_voltage_filter_update(&filter, 3000, &out);
    battery_voltage_filter_update(&filter, 3100, &out);
    /* average of (3000 + 3100) / 2 = 3050 */
    TEST_ASSERT_EQUAL_UINT16(3050, out);
}

/* ── Window rollover ──────────────────────────────────────────────────────── */

void test_window_rollover_discards_oldest(void)
{
    /* Use a small window size for easy verification */
    battery_voltage_filter_t small;
    uint16_t out = 0;

    battery_voltage_filter_init(&small, 3);

    /* Fill window: [1000, 2000, 3000] avg = 2000 */
    battery_voltage_filter_update(&small, 1000, &out);
    battery_voltage_filter_update(&small, 2000, &out);
    battery_voltage_filter_update(&small, 3000, &out);
    TEST_ASSERT_EQUAL_UINT16(2000, out);

    /* Push 4000, oldest (1000) drops: [2000, 3000, 4000] avg = 3000 */
    battery_voltage_filter_update(&small, 4000, &out);
    TEST_ASSERT_EQUAL_UINT16(3000, out);
}

/* ── Reset ────────────────────────────────────────────────────────────────── */

void test_reset_clears_state(void)
{
    uint16_t out = 0;

    battery_voltage_filter_update(&filter, 3000, &out);
    battery_voltage_filter_update(&filter, 3100, &out);

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_voltage_filter_reset(&filter));

    /* After reset, first sample should equal itself */
    battery_voltage_filter_update(&filter, 2500, &out);
    TEST_ASSERT_EQUAL_UINT16(2500, out);
}

void test_reset_null_filter(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_voltage_filter_reset(NULL));
}

/* ── Window size sanitization ─────────────────────────────────────────────── */

void test_window_size_zero_uses_default(void)
{
    battery_voltage_filter_t f;
    battery_voltage_filter_init(&f, 0);
    TEST_ASSERT_EQUAL_size_t(BATTERY_VOLTAGE_FILTER_DEFAULT_WINDOW_SIZE,
                             f.window_size);
}

void test_window_size_exceeds_max_uses_default(void)
{
    battery_voltage_filter_t f;
    battery_voltage_filter_init(&f, BATTERY_VOLTAGE_FILTER_MAX_WINDOW_SIZE + 1);
    TEST_ASSERT_EQUAL_size_t(BATTERY_VOLTAGE_FILTER_DEFAULT_WINDOW_SIZE,
                             f.window_size);
}

/* ── get without update ───────────────────────────────────────────────────── */

void test_get_with_no_samples_returns_zero(void)
{
    uint16_t out = 9999;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_voltage_filter_get(&filter, &out));
    TEST_ASSERT_EQUAL_UINT16(0, out);
}

/* ── Runner ───────────────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_null_filter);
    RUN_TEST(test_update_null_filter);
    RUN_TEST(test_update_null_output);
    RUN_TEST(test_get_null_filter);
    RUN_TEST(test_single_sample_returns_itself);
    RUN_TEST(test_two_samples_averaged);
    RUN_TEST(test_window_rollover_discards_oldest);
    RUN_TEST(test_reset_clears_state);
    RUN_TEST(test_reset_null_filter);
    RUN_TEST(test_window_size_zero_uses_default);
    RUN_TEST(test_window_size_exceeds_max_uses_default);
    RUN_TEST(test_get_with_no_samples_returns_zero);

    return UNITY_END();
}
