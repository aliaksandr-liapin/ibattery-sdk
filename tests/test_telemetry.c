#include "unity.h"
#include <battery_sdk/battery_telemetry.h>
#include <battery_sdk/battery_status.h>
#include <battery_sdk/battery_types.h>
#include <string.h>

/* Mock control functions — implemented in mocks/ */
extern void mock_sdk_set_uptime_rc(int rc);
extern void mock_sdk_set_uptime_ms(uint32_t ms);
extern void mock_voltage_set_rc(int rc);
extern void mock_voltage_set_mv(uint16_t mv);
extern void mock_temperature_set_rc(int rc);
extern void mock_temperature_set_c_x100(int32_t val);
extern void mock_soc_set_rc(int rc);
extern void mock_soc_set_pct_x100(uint16_t val);
extern void mock_power_set_rc(int rc);
extern void mock_power_set_state(enum battery_power_state s);

void setUp(void)
{
    /* Reset all mocks to happy-path defaults before each test */
    mock_sdk_set_uptime_rc(BATTERY_STATUS_OK);
    mock_sdk_set_uptime_ms(4000);
    mock_voltage_set_rc(BATTERY_STATUS_OK);
    mock_voltage_set_mv(2950);
    mock_temperature_set_rc(BATTERY_STATUS_OK);
    mock_temperature_set_c_x100(2500);
    mock_soc_set_rc(BATTERY_STATUS_OK);
    mock_soc_set_pct_x100(8500);
    mock_power_set_rc(BATTERY_STATUS_OK);
    mock_power_set_state(BATTERY_POWER_STATE_ACTIVE);
}

void tearDown(void) {}

/* ── Null check ───────────────────────────────────────────────────────────── */

void test_collect_null_packet(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_telemetry_collect(NULL));
}

/* ── Full happy-path packet ───────────────────────────────────────────────── */

void test_collect_full_packet(void)
{
    struct battery_telemetry_packet pkt;
    memset(&pkt, 0xFF, sizeof(pkt));  /* poison */

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_telemetry_collect(&pkt));

    TEST_ASSERT_EQUAL_UINT8(BATTERY_TELEMETRY_VERSION, pkt.telemetry_version);
    TEST_ASSERT_EQUAL_UINT32(4000, pkt.timestamp_ms);
    TEST_ASSERT_EQUAL_INT32(2950, pkt.voltage_mv);
    TEST_ASSERT_EQUAL_INT32(2500, pkt.temperature_c_x100);
    TEST_ASSERT_EQUAL_UINT16(8500, pkt.soc_pct_x100);
    TEST_ASSERT_EQUAL_UINT8(BATTERY_POWER_STATE_ACTIVE, pkt.power_state);
    TEST_ASSERT_EQUAL_UINT32(0, pkt.status_flags);
}

/* ── Voltage failure sets flag, still succeeds ────────────────────────────── */

void test_collect_voltage_error_sets_flag(void)
{
    struct battery_telemetry_packet pkt;
    mock_voltage_set_rc(BATTERY_STATUS_IO);

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_telemetry_collect(&pkt));

    TEST_ASSERT_BITS(BATTERY_TELEMETRY_FLAG_VOLTAGE_ERR,
                     BATTERY_TELEMETRY_FLAG_VOLTAGE_ERR,
                     pkt.status_flags);
    /* Voltage field should be zero (memset) */
    TEST_ASSERT_EQUAL_INT32(0, pkt.voltage_mv);
}

/* ── Temperature failure sets flag ────────────────────────────────────────── */

void test_collect_temp_error_sets_flag(void)
{
    struct battery_telemetry_packet pkt;
    mock_temperature_set_rc(BATTERY_STATUS_IO);

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_telemetry_collect(&pkt));

    TEST_ASSERT_BITS(BATTERY_TELEMETRY_FLAG_TEMP_ERR,
                     BATTERY_TELEMETRY_FLAG_TEMP_ERR,
                     pkt.status_flags);
}

/* ── SoC failure sets flag ────────────────────────────────────────────────── */

void test_collect_soc_error_sets_flag(void)
{
    struct battery_telemetry_packet pkt;
    mock_soc_set_rc(BATTERY_STATUS_ERROR);

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_telemetry_collect(&pkt));

    TEST_ASSERT_BITS(BATTERY_TELEMETRY_FLAG_SOC_ERR,
                     BATTERY_TELEMETRY_FLAG_SOC_ERR,
                     pkt.status_flags);
}

/* ── Power state failure sets flag ────────────────────────────────────────── */

void test_collect_power_error_sets_flag(void)
{
    struct battery_telemetry_packet pkt;
    mock_power_set_rc(BATTERY_STATUS_ERROR);

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_telemetry_collect(&pkt));

    TEST_ASSERT_BITS(BATTERY_TELEMETRY_FLAG_POWER_STATE_ERR,
                     BATTERY_TELEMETRY_FLAG_POWER_STATE_ERR,
                     pkt.status_flags);
}

/* ── Timestamp failure sets flag ──────────────────────────────────────────── */

void test_collect_timestamp_error_sets_flag(void)
{
    struct battery_telemetry_packet pkt;
    mock_sdk_set_uptime_rc(BATTERY_STATUS_IO);

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_telemetry_collect(&pkt));

    TEST_ASSERT_BITS(BATTERY_TELEMETRY_FLAG_TIMESTAMP_ERR,
                     BATTERY_TELEMETRY_FLAG_TIMESTAMP_ERR,
                     pkt.status_flags);
    TEST_ASSERT_EQUAL_UINT32(0, pkt.timestamp_ms);
}

/* ── Multiple failures accumulate flags ───────────────────────────────────── */

void test_collect_multiple_failures(void)
{
    struct battery_telemetry_packet pkt;
    mock_voltage_set_rc(BATTERY_STATUS_IO);
    mock_temperature_set_rc(BATTERY_STATUS_IO);
    mock_soc_set_rc(BATTERY_STATUS_ERROR);

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_telemetry_collect(&pkt));

    uint32_t expected = BATTERY_TELEMETRY_FLAG_VOLTAGE_ERR |
                        BATTERY_TELEMETRY_FLAG_TEMP_ERR |
                        BATTERY_TELEMETRY_FLAG_SOC_ERR;

    TEST_ASSERT_EQUAL_UINT32(expected, pkt.status_flags);

    /* Good fields still populated */
    TEST_ASSERT_EQUAL_UINT32(4000, pkt.timestamp_ms);
    TEST_ASSERT_EQUAL_UINT8(BATTERY_POWER_STATE_ACTIVE, pkt.power_state);
}

/* ── New power states ────────────────────────────────────────────────────── */

void test_collect_charging_state(void)
{
    struct battery_telemetry_packet pkt;
    mock_power_set_state(BATTERY_POWER_STATE_CHARGING);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_telemetry_collect(&pkt));
    TEST_ASSERT_EQUAL_UINT8(BATTERY_POWER_STATE_CHARGING, pkt.power_state);
}

void test_collect_discharging_state(void)
{
    struct battery_telemetry_packet pkt;
    mock_power_set_state(BATTERY_POWER_STATE_DISCHARGING);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_telemetry_collect(&pkt));
    TEST_ASSERT_EQUAL_UINT8(BATTERY_POWER_STATE_DISCHARGING, pkt.power_state);
}

void test_collect_charged_state(void)
{
    struct battery_telemetry_packet pkt;
    mock_power_set_state(BATTERY_POWER_STATE_CHARGED);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_telemetry_collect(&pkt));
    TEST_ASSERT_EQUAL_UINT8(BATTERY_POWER_STATE_CHARGED, pkt.power_state);
}

void test_collect_idle_state(void)
{
    struct battery_telemetry_packet pkt;
    mock_power_set_state(BATTERY_POWER_STATE_IDLE);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_telemetry_collect(&pkt));
    TEST_ASSERT_EQUAL_UINT8(BATTERY_POWER_STATE_IDLE, pkt.power_state);
}

void test_collect_sleep_state(void)
{
    struct battery_telemetry_packet pkt;
    mock_power_set_state(BATTERY_POWER_STATE_SLEEP);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_telemetry_collect(&pkt));
    TEST_ASSERT_EQUAL_UINT8(BATTERY_POWER_STATE_SLEEP, pkt.power_state);
}

/* ── Init sets flag ───────────────────────────────────────────────────────── */

void test_init_returns_ok(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_telemetry_init());
}

/* ── Runner ───────────────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_collect_null_packet);
    RUN_TEST(test_collect_full_packet);
    RUN_TEST(test_collect_voltage_error_sets_flag);
    RUN_TEST(test_collect_temp_error_sets_flag);
    RUN_TEST(test_collect_soc_error_sets_flag);
    RUN_TEST(test_collect_power_error_sets_flag);
    RUN_TEST(test_collect_timestamp_error_sets_flag);
    RUN_TEST(test_collect_multiple_failures);
    RUN_TEST(test_collect_charging_state);
    RUN_TEST(test_collect_discharging_state);
    RUN_TEST(test_collect_charged_state);
    RUN_TEST(test_collect_idle_state);
    RUN_TEST(test_collect_sleep_state);
    RUN_TEST(test_init_returns_ok);

    return UNITY_END();
}
