#include "unity.h"
#include <battery_sdk/battery_power_manager.h>
#include <battery_sdk/battery_types.h>
#include <battery_sdk/battery_status.h>
#include <stdbool.h>

/* Mock control functions — implemented in mocks/ */
extern void mock_voltage_set_rc(int rc);
extern void mock_voltage_set_mv(uint16_t mv);
extern void mock_hal_set_uptime_rc(int rc);
extern void mock_hal_set_uptime_ms(uint32_t ms);

#if defined(CONFIG_BATTERY_CHARGER_TP4056)
extern void mock_charger_set_rc(int rc);
extern void mock_charger_set_charging(bool v);
extern void mock_charger_set_charged(bool v);
/* When charger driver is present, the "normal on-battery" state is DISCHARGING */
#define EXPECTED_DEFAULT_STATE  BATTERY_POWER_STATE_DISCHARGING
#else
#define EXPECTED_DEFAULT_STATE  BATTERY_POWER_STATE_ACTIVE
#endif

void setUp(void)
{
    mock_voltage_set_rc(BATTERY_STATUS_OK);
    mock_voltage_set_mv(2950);     /* healthy battery */
    mock_hal_set_uptime_rc(BATTERY_STATUS_OK);
    mock_hal_set_uptime_ms(1000);  /* boot time */

#if defined(CONFIG_BATTERY_CHARGER_TP4056)
    mock_charger_set_rc(BATTERY_STATUS_OK);
    mock_charger_set_charging(false);
    mock_charger_set_charged(false);
#endif

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
    TEST_ASSERT_EQUAL_INT(EXPECTED_DEFAULT_STATE, state);
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
    TEST_ASSERT_EQUAL_INT(EXPECTED_DEFAULT_STATE, state);
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

    /* Rise above 2200: recovers to default on-battery state */
    mock_voltage_set_mv(2201);
    battery_power_manager_get_state(&state);
    TEST_ASSERT_EQUAL_INT(EXPECTED_DEFAULT_STATE, state);
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
    TEST_ASSERT_EQUAL_INT(EXPECTED_DEFAULT_STATE, state);

    /* Second call: voltage fails — should return last known */
    mock_voltage_set_rc(BATTERY_STATUS_IO);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_power_manager_get_state(&state));
    TEST_ASSERT_EQUAL_INT(EXPECTED_DEFAULT_STATE, state);
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

    /* After init, first read with healthy voltage should be default state */
    mock_voltage_set_mv(2950);
    battery_power_manager_get_state(&state);
    TEST_ASSERT_EQUAL_INT(EXPECTED_DEFAULT_STATE, state);
}

/* ── Inactivity: IDLE after 30 s ─────────────────────────────────────────── */

void test_idle_after_inactivity(void)
{
    enum battery_power_state state;

    /* Advance uptime by 31 seconds past last activity */
    mock_hal_set_uptime_ms(1000 + 31000);
    battery_power_manager_get_state(&state);
    TEST_ASSERT_EQUAL_INT(BATTERY_POWER_STATE_IDLE, state);
}

/* ── Inactivity: SLEEP after 120 s ──────────────────────────────────────── */

void test_sleep_after_deep_inactivity(void)
{
    enum battery_power_state state;

    /* Advance uptime by 121 seconds past last activity */
    mock_hal_set_uptime_ms(1000 + 121000);
    battery_power_manager_get_state(&state);
    TEST_ASSERT_EQUAL_INT(BATTERY_POWER_STATE_SLEEP, state);
}

/* ── report_activity resets to ACTIVE ────────────────────────────────────── */

void test_activity_resets_to_active(void)
{
    enum battery_power_state state;

    /* Advance to IDLE */
    mock_hal_set_uptime_ms(1000 + 31000);
    battery_power_manager_get_state(&state);
    TEST_ASSERT_EQUAL_INT(BATTERY_POWER_STATE_IDLE, state);

    /* Report activity at current uptime, then query again */
    battery_power_manager_report_activity();
    battery_power_manager_get_state(&state);
    TEST_ASSERT_EQUAL_INT(EXPECTED_DEFAULT_STATE, state);
}

/* ── CRITICAL overrides IDLE ─────────────────────────────────────────────── */

void test_critical_overrides_idle(void)
{
    enum battery_power_state state;

    /* Low voltage + inactivity timeout — CRITICAL wins */
    mock_voltage_set_mv(2050);
    mock_hal_set_uptime_ms(1000 + 31000);
    battery_power_manager_get_state(&state);
    TEST_ASSERT_EQUAL_INT(BATTERY_POWER_STATE_CRITICAL, state);
}

/* ── Uptime failure: stays ACTIVE (skip inactivity logic) ────────────────── */

void test_uptime_error_stays_active(void)
{
    enum battery_power_state state;

    /* If uptime read fails, should still return ACTIVE (not IDLE/SLEEP) */
    mock_hal_set_uptime_rc(BATTERY_STATUS_IO);
    /* Re-init so init can set g_last_activity_ms to 0 (uptime fails) */
    battery_power_manager_init();

    mock_voltage_set_mv(2950);
    battery_power_manager_get_state(&state);
    TEST_ASSERT_EQUAL_INT(EXPECTED_DEFAULT_STATE, state);
}

/* ── Charger tests (only when CONFIG_BATTERY_CHARGER_TP4056 is defined) ── */

#if defined(CONFIG_BATTERY_CHARGER_TP4056)

void test_charging_state(void)
{
    enum battery_power_state state;

    mock_charger_set_charging(true);
    battery_power_manager_get_state(&state);
    TEST_ASSERT_EQUAL_INT(BATTERY_POWER_STATE_CHARGING, state);
}

void test_charged_state(void)
{
    enum battery_power_state state;

    mock_charger_set_charged(true);
    battery_power_manager_get_state(&state);
    TEST_ASSERT_EQUAL_INT(BATTERY_POWER_STATE_CHARGED, state);
}

void test_discharging_state(void)
{
    enum battery_power_state state;

    /* Neither charging nor charged = DISCHARGING (when charger driver present) */
    mock_charger_set_charging(false);
    mock_charger_set_charged(false);
    battery_power_manager_get_state(&state);
    TEST_ASSERT_EQUAL_INT(BATTERY_POWER_STATE_DISCHARGING, state);
}

void test_critical_to_charging(void)
{
    enum battery_power_state state;

    /* Enter CRITICAL via low voltage */
    mock_voltage_set_mv(2050);
    mock_charger_set_charging(false);
    battery_power_manager_get_state(&state);
    TEST_ASSERT_EQUAL_INT(BATTERY_POWER_STATE_CRITICAL, state);

    /* Connect charger — should recover from CRITICAL to CHARGING */
    mock_charger_set_charging(true);
    battery_power_manager_get_state(&state);
    TEST_ASSERT_EQUAL_INT(BATTERY_POWER_STATE_CHARGING, state);
}

void test_charger_error_falls_back(void)
{
    enum battery_power_state state;

    /* Charger read fails — should fall through to DISCHARGING (charger default) */
    mock_charger_set_rc(BATTERY_STATUS_IO);
    battery_power_manager_get_state(&state);
    TEST_ASSERT_EQUAL_INT(BATTERY_POWER_STATE_DISCHARGING, state);
}

void test_charging_overrides_idle(void)
{
    enum battery_power_state state;

    /* Even with inactivity timeout elapsed, CHARGING takes priority */
    mock_hal_set_uptime_ms(1000 + 31000);
    mock_charger_set_charging(true);
    battery_power_manager_get_state(&state);
    TEST_ASSERT_EQUAL_INT(BATTERY_POWER_STATE_CHARGING, state);
}

#endif /* CONFIG_BATTERY_CHARGER_TP4056 */

/* ── Runner ──────────────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    /* Original tests */
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

    /* Inactivity tests */
    RUN_TEST(test_idle_after_inactivity);
    RUN_TEST(test_sleep_after_deep_inactivity);
    RUN_TEST(test_activity_resets_to_active);
    RUN_TEST(test_critical_overrides_idle);
    RUN_TEST(test_uptime_error_stays_active);

    /* Charger tests */
#if defined(CONFIG_BATTERY_CHARGER_TP4056)
    RUN_TEST(test_charging_state);
    RUN_TEST(test_charged_state);
    RUN_TEST(test_discharging_state);
    RUN_TEST(test_critical_to_charging);
    RUN_TEST(test_charger_error_falls_back);
    RUN_TEST(test_charging_overrides_idle);
#endif

    return UNITY_END();
}
