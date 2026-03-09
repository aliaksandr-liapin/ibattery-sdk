#include "unity.h"
#include <battery_sdk/battery_power_manager.h>
#include <battery_sdk/battery_types.h>
#include <battery_sdk/battery_status.h>

/* Mock control functions — implemented in mocks/ */
extern void mock_voltage_set_rc(int rc);
extern void mock_voltage_set_mv(uint16_t mv);

void setUp(void)
{
    mock_voltage_set_rc(BATTERY_STATUS_OK);
    mock_voltage_set_mv(2950);     /* healthy battery */
    battery_power_manager_init();  /* reset state machine each test */
}

void tearDown(void) {}

/* ── Null check ──────────────────────────────────────────────────────────── */

void test_get_state_null_pointer(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_power_manager_get_state(NULL));
}

/* ── Happy path: healthy voltage = ACTIVE ────────────────────────────────── */

void test_healthy_voltage_returns_active(void)
{
    enum battery_power_state state;
    mock_voltage_set_mv(2950);

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_power_manager_get_state(&state));
    TEST_ASSERT_EQUAL_INT(BATTERY_POWER_STATE_ACTIVE, state);
}

/* ── Below critical threshold: CRITICAL ──────────────────────────────────── */

void test_below_critical_threshold_returns_critical(void)
{
    enum battery_power_state state;
    mock_voltage_set_mv(2050);  /* below 2100 mV enter threshold */

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_power_manager_get_state(&state));
    TEST_ASSERT_EQUAL_INT(BATTERY_POWER_STATE_CRITICAL, state);
}

/* ── Boundary tests ──────────────────────────────────────────────────────── */

void test_at_critical_enter_boundary(void)
{
    enum battery_power_state state;
    mock_voltage_set_mv(2100);  /* at boundary: < 2100 triggers, 2100 does NOT */

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_power_manager_get_state(&state));
    TEST_ASSERT_EQUAL_INT(BATTERY_POWER_STATE_ACTIVE, state);
}

void test_one_below_critical_enter(void)
{
    enum battery_power_state state;
    mock_voltage_set_mv(2099);  /* just below 2100 */

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_power_manager_get_state(&state));
    TEST_ASSERT_EQUAL_INT(BATTERY_POWER_STATE_CRITICAL, state);
}

/* ── Hysteresis tests ────────────────────────────────────────────────────── */

void test_hysteresis_stays_critical_in_deadband(void)
{
    enum battery_power_state state;

    /* Drop to CRITICAL */
    mock_voltage_set_mv(2050);
    battery_power_manager_get_state(&state);
    TEST_ASSERT_EQUAL_INT(BATTERY_POWER_STATE_CRITICAL, state);

    /* Rise to 2150 (between enter and exit thresholds): still CRITICAL */
    mock_voltage_set_mv(2150);
    battery_power_manager_get_state(&state);
    TEST_ASSERT_EQUAL_INT(BATTERY_POWER_STATE_CRITICAL, state);

    /* Rise to 2199: still CRITICAL */
    mock_voltage_set_mv(2199);
    battery_power_manager_get_state(&state);
    TEST_ASSERT_EQUAL_INT(BATTERY_POWER_STATE_CRITICAL, state);
}

void test_hysteresis_exits_critical_above_exit_threshold(void)
{
    enum battery_power_state state;

    /* Drop to CRITICAL */
    mock_voltage_set_mv(2050);
    battery_power_manager_get_state(&state);
    TEST_ASSERT_EQUAL_INT(BATTERY_POWER_STATE_CRITICAL, state);

    /* Rise above 2200: recovers to ACTIVE */
    mock_voltage_set_mv(2201);
    battery_power_manager_get_state(&state);
    TEST_ASSERT_EQUAL_INT(BATTERY_POWER_STATE_ACTIVE, state);
}

void test_hysteresis_at_exit_boundary(void)
{
    enum battery_power_state state;

    /* Drop to CRITICAL */
    mock_voltage_set_mv(2050);
    battery_power_manager_get_state(&state);
    TEST_ASSERT_EQUAL_INT(BATTERY_POWER_STATE_CRITICAL, state);

    /* At exactly 2200: > 2200 triggers exit, so 2200 does NOT */
    mock_voltage_set_mv(2200);
    battery_power_manager_get_state(&state);
    TEST_ASSERT_EQUAL_INT(BATTERY_POWER_STATE_CRITICAL, state);
}

/* ── Voltage read failure: graceful degradation ──────────────────────────── */

void test_voltage_error_returns_last_known_active(void)
{
    enum battery_power_state state;

    /* First call: healthy */
    mock_voltage_set_mv(2950);
    battery_power_manager_get_state(&state);
    TEST_ASSERT_EQUAL_INT(BATTERY_POWER_STATE_ACTIVE, state);

    /* Second call: voltage fails — should return last known (ACTIVE) */
    mock_voltage_set_rc(BATTERY_STATUS_IO);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_power_manager_get_state(&state));
    TEST_ASSERT_EQUAL_INT(BATTERY_POWER_STATE_ACTIVE, state);
}

void test_voltage_error_returns_last_known_critical(void)
{
    enum battery_power_state state;

    /* Drop to CRITICAL */
    mock_voltage_set_mv(2050);
    battery_power_manager_get_state(&state);
    TEST_ASSERT_EQUAL_INT(BATTERY_POWER_STATE_CRITICAL, state);

    /* Voltage fails — should stay CRITICAL */
    mock_voltage_set_rc(BATTERY_STATUS_IO);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_power_manager_get_state(&state));
    TEST_ASSERT_EQUAL_INT(BATTERY_POWER_STATE_CRITICAL, state);
}

/* ── Init ────────────────────────────────────────────────────────────────── */

void test_init_returns_ok(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_power_manager_init());
}

void test_init_sets_active(void)
{
    enum battery_power_state state;

    /* After init, first read with healthy voltage should be ACTIVE */
    mock_voltage_set_mv(2950);
    battery_power_manager_get_state(&state);
    TEST_ASSERT_EQUAL_INT(BATTERY_POWER_STATE_ACTIVE, state);
}

/* ── Runner ──────────────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_get_state_null_pointer);
    RUN_TEST(test_healthy_voltage_returns_active);
    RUN_TEST(test_below_critical_threshold_returns_critical);
    RUN_TEST(test_at_critical_enter_boundary);
    RUN_TEST(test_one_below_critical_enter);
    RUN_TEST(test_hysteresis_stays_critical_in_deadband);
    RUN_TEST(test_hysteresis_exits_critical_above_exit_threshold);
    RUN_TEST(test_hysteresis_at_exit_boundary);
    RUN_TEST(test_voltage_error_returns_last_known_active);
    RUN_TEST(test_voltage_error_returns_last_known_critical);
    RUN_TEST(test_init_returns_ok);
    RUN_TEST(test_init_sets_active);

    return UNITY_END();
}
