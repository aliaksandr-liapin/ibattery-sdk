/*
 * Unit tests for battery_cycle_counter.
 *
 * Tests CHARGING → CHARGED transition detection and NVS persistence.
 */

#include "unity.h"
#include <battery_sdk/battery_status.h>
#include <battery_sdk/battery_types.h>
#include <battery_sdk/battery_cycle_counter.h>

/* ── Mock control declarations ───────────────────────────────────── */

extern void mock_nvs_set_init_rc(int rc);
extern void mock_nvs_set_read_rc(int rc);
extern void mock_nvs_set_write_rc(int rc);
extern void mock_nvs_set_stored_value(uint32_t v);
extern void mock_nvs_reset(void);
extern uint32_t mock_nvs_get_last_written(void);

void setUp(void)
{
    mock_nvs_reset();
}

void tearDown(void) {}

/* ── Init tests ──────────────────────────────────────────────────── */

void test_init_no_stored_value(void)
{
    /* NVS has no stored cycle count — starts at 0 */
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_cycle_counter_init());

    uint32_t count = 99;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_cycle_counter_get(&count));
    TEST_ASSERT_EQUAL_UINT32(0, count);
}

void test_init_loads_stored_value(void)
{
    mock_nvs_set_stored_value(42);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_cycle_counter_init());

    uint32_t count;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_cycle_counter_get(&count));
    TEST_ASSERT_EQUAL_UINT32(42, count);
}

void test_init_nvs_failure_starts_at_zero(void)
{
    mock_nvs_set_init_rc(BATTERY_STATUS_IO);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_cycle_counter_init());

    uint32_t count;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_cycle_counter_get(&count));
    TEST_ASSERT_EQUAL_UINT32(0, count);
}

/* ── Get NULL arg ────────────────────────────────────────────────── */

void test_get_null(void)
{
    battery_cycle_counter_init();
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_cycle_counter_get(NULL));
}

/* ── Transition detection ────────────────────────────────────────── */

void test_charging_to_charged_increments(void)
{
    battery_cycle_counter_init();

    battery_cycle_counter_update(BATTERY_POWER_STATE_CHARGING);
    battery_cycle_counter_update(BATTERY_POWER_STATE_CHARGED);

    uint32_t count;
    battery_cycle_counter_get(&count);
    TEST_ASSERT_EQUAL_UINT32(1, count);
}

void test_multiple_cycles(void)
{
    battery_cycle_counter_init();

    for (int i = 0; i < 5; i++) {
        battery_cycle_counter_update(BATTERY_POWER_STATE_DISCHARGING);
        battery_cycle_counter_update(BATTERY_POWER_STATE_CHARGING);
        battery_cycle_counter_update(BATTERY_POWER_STATE_CHARGED);
    }

    uint32_t count;
    battery_cycle_counter_get(&count);
    TEST_ASSERT_EQUAL_UINT32(5, count);
}

void test_no_increment_without_charging_first(void)
{
    battery_cycle_counter_init();

    /* Go straight to CHARGED without CHARGING */
    battery_cycle_counter_update(BATTERY_POWER_STATE_ACTIVE);
    battery_cycle_counter_update(BATTERY_POWER_STATE_CHARGED);

    uint32_t count;
    battery_cycle_counter_get(&count);
    TEST_ASSERT_EQUAL_UINT32(0, count);
}

void test_no_increment_charging_to_discharging(void)
{
    battery_cycle_counter_init();

    /* Charger disconnected mid-charge */
    battery_cycle_counter_update(BATTERY_POWER_STATE_CHARGING);
    battery_cycle_counter_update(BATTERY_POWER_STATE_DISCHARGING);

    uint32_t count;
    battery_cycle_counter_get(&count);
    TEST_ASSERT_EQUAL_UINT32(0, count);
}

void test_no_double_count_staying_charged(void)
{
    battery_cycle_counter_init();

    battery_cycle_counter_update(BATTERY_POWER_STATE_CHARGING);
    battery_cycle_counter_update(BATTERY_POWER_STATE_CHARGED);
    battery_cycle_counter_update(BATTERY_POWER_STATE_CHARGED);  /* stays CHARGED */

    uint32_t count;
    battery_cycle_counter_get(&count);
    TEST_ASSERT_EQUAL_UINT32(1, count);
}

/* ── NVS persistence ─────────────────────────────────────────────── */

void test_persists_on_increment(void)
{
    battery_cycle_counter_init();

    battery_cycle_counter_update(BATTERY_POWER_STATE_CHARGING);
    battery_cycle_counter_update(BATTERY_POWER_STATE_CHARGED);

    TEST_ASSERT_EQUAL_UINT32(1, mock_nvs_get_last_written());
}

void test_persists_cumulative_with_stored(void)
{
    mock_nvs_set_stored_value(10);
    battery_cycle_counter_init();

    battery_cycle_counter_update(BATTERY_POWER_STATE_CHARGING);
    battery_cycle_counter_update(BATTERY_POWER_STATE_CHARGED);

    uint32_t count;
    battery_cycle_counter_get(&count);
    TEST_ASSERT_EQUAL_UINT32(11, count);
    TEST_ASSERT_EQUAL_UINT32(11, mock_nvs_get_last_written());
}

void test_write_failure_still_counts_in_memory(void)
{
    battery_cycle_counter_init();
    mock_nvs_set_write_rc(BATTERY_STATUS_IO);

    battery_cycle_counter_update(BATTERY_POWER_STATE_CHARGING);
    battery_cycle_counter_update(BATTERY_POWER_STATE_CHARGED);

    uint32_t count;
    battery_cycle_counter_get(&count);
    TEST_ASSERT_EQUAL_UINT32(1, count);  /* counted in memory despite write fail */
}

/* ── Test runner ─────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    /* Init */
    RUN_TEST(test_init_no_stored_value);
    RUN_TEST(test_init_loads_stored_value);
    RUN_TEST(test_init_nvs_failure_starts_at_zero);
    RUN_TEST(test_get_null);

    /* Transitions */
    RUN_TEST(test_charging_to_charged_increments);
    RUN_TEST(test_multiple_cycles);
    RUN_TEST(test_no_increment_without_charging_first);
    RUN_TEST(test_no_increment_charging_to_discharging);
    RUN_TEST(test_no_double_count_staying_charged);

    /* Persistence */
    RUN_TEST(test_persists_on_increment);
    RUN_TEST(test_persists_cumulative_with_stored);
    RUN_TEST(test_write_failure_still_counts_in_memory);

    return UNITY_END();
}
