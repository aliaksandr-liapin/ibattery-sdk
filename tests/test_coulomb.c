/*
 * Unit tests for battery_coulomb (coulomb counter).
 *
 * Tests trapezoidal integration, NVS persistence, and reset.
 */

#include "unity.h"
#include <battery_sdk/battery_status.h>
#include <battery_sdk/battery_coulomb.h>

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
    battery_coulomb_init();
}

void tearDown(void) {}

/* ── Init tests ──────────────────────────────────────────────────── */

void test_init_returns_ok(void)
{
    mock_nvs_reset();
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_coulomb_init());
}

void test_init_starts_at_zero(void)
{
    int32_t mah = -1;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_coulomb_get_mah_x100(&mah));
    TEST_ASSERT_EQUAL_INT32(0, mah);
}

void test_init_loads_from_nvs(void)
{
    /* Store 500 in NVS = 5.00 mAh in x100 units */
    mock_nvs_reset();
    mock_nvs_set_stored_value(500);
    battery_coulomb_init();

    int32_t mah;
    battery_coulomb_get_mah_x100(&mah);
    TEST_ASSERT_EQUAL_INT32(500, mah);
}

/* ── Null pointer ────────────────────────────────────────────────── */

void test_get_null_returns_invalid_arg(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_coulomb_get_mah_x100(NULL));
}

/* ── Integration tests ───────────────────────────────────────────── */

void test_constant_discharge_1_hour(void)
{
    /*
     * 100.00 mA (= 10000 x100) for 3600 seconds = 100.00 mAh (= 10000 x100)
     * Feed 1800 samples at 2s intervals.
     * First sample consumed to initialize prev_current.
     */
    int32_t current_x100 = 10000; /* 100.00 mA */
    uint32_t dt_ms = 2000;        /* 2 seconds */

    /* First call: sets prev, no integration */
    battery_coulomb_update(current_x100, dt_ms);

    /* Remaining 1799 intervals of 2s = 3598s.
     * But we need full 3600s of integration.
     * Total integration time = 1800 intervals * 2s = 3600s
     * First call sets prev (no integration).
     * So we need 1800 more calls to get 1800 * 2s = 3600s of integration.
     */
    for (int i = 0; i < 1800; i++) {
        battery_coulomb_update(current_x100, dt_ms);
    }

    int32_t mah;
    battery_coulomb_get_mah_x100(&mah);
    /* 100 mA * 1 hour = 100 mAh = 10000 in x100 */
    TEST_ASSERT_INT32_WITHIN(50, 10000, mah);
}

void test_constant_charge_negative_current(void)
{
    /*
     * -50.00 mA (= -5000 x100) for 3600 seconds = -50.00 mAh (= -5000 x100)
     * Feed 1800 intervals at 2s.
     */
    int32_t current_x100 = -5000; /* -50.00 mA */
    uint32_t dt_ms = 2000;

    battery_coulomb_update(current_x100, dt_ms); /* init prev */

    for (int i = 0; i < 1800; i++) {
        battery_coulomb_update(current_x100, dt_ms);
    }

    int32_t mah;
    battery_coulomb_get_mah_x100(&mah);
    TEST_ASSERT_INT32_WITHIN(50, -5000, mah);
}

void test_zero_current_no_accumulation(void)
{
    battery_coulomb_update(0, 2000); /* init prev */

    for (int i = 0; i < 100; i++) {
        battery_coulomb_update(0, 2000);
    }

    int32_t mah;
    battery_coulomb_get_mah_x100(&mah);
    TEST_ASSERT_EQUAL_INT32(0, mah);
}

void test_zero_dt_no_accumulation(void)
{
    battery_coulomb_update(10000, 0); /* init prev with dt=0 */
    battery_coulomb_update(10000, 0); /* dt=0, no integration */
    battery_coulomb_update(10000, 0);

    int32_t mah;
    battery_coulomb_get_mah_x100(&mah);
    TEST_ASSERT_EQUAL_INT32(0, mah);
}

void test_trapezoidal_ramp(void)
{
    /*
     * Ramp from 0 mA to 100 mA over 1 hour.
     * Expected: ~50 mAh (average of ramp) = 5000 in x100.
     *
     * Use 1800 steps of 2s. Current ramps linearly from 0 to 10000 x100.
     */
    uint32_t dt_ms = 2000;
    int steps = 1800;

    /* First call at 0 mA — sets prev, no integration */
    battery_coulomb_update(0, dt_ms);

    for (int i = 1; i <= steps; i++) {
        /* Linear ramp: i/steps * 10000 */
        int32_t current_x100 = (int32_t)((int64_t)i * 10000 / steps);
        battery_coulomb_update(current_x100, dt_ms);
    }

    int32_t mah;
    battery_coulomb_get_mah_x100(&mah);
    /* Average current = 50 mA, time = 3600s = 1hr → 50 mAh = 5000 x100 */
    TEST_ASSERT_INT32_WITHIN(50, 5000, mah);
}

/* ── Reset tests ─────────────────────────────────────────────────── */

void test_reset_sets_value(void)
{
    battery_coulomb_reset(5000); /* 50.00 mAh */

    int32_t mah;
    battery_coulomb_get_mah_x100(&mah);
    TEST_ASSERT_EQUAL_INT32(5000, mah);
}

void test_reset_persists_to_nvs(void)
{
    battery_coulomb_reset(5000); /* 50.00 mAh */

    /* NVS stores x100 value directly */
    TEST_ASSERT_EQUAL_UINT32(5000, mock_nvs_get_last_written());
}

/* ── Test runner ─────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    /* Init */
    RUN_TEST(test_init_returns_ok);
    RUN_TEST(test_init_starts_at_zero);
    RUN_TEST(test_init_loads_from_nvs);
    RUN_TEST(test_get_null_returns_invalid_arg);

    /* Integration */
    RUN_TEST(test_constant_discharge_1_hour);
    RUN_TEST(test_constant_charge_negative_current);
    RUN_TEST(test_zero_current_no_accumulation);
    RUN_TEST(test_zero_dt_no_accumulation);
    RUN_TEST(test_trapezoidal_ramp);

    /* Reset */
    RUN_TEST(test_reset_sets_value);
    RUN_TEST(test_reset_persists_to_nvs);

    return UNITY_END();
}
