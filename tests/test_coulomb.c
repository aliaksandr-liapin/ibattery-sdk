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
     * Q-as-remaining semantics: 100 mA discharge for 1 hour starting from
     * a 220 mAh full baseline should leave 120 mAh remaining (= 12000 x100).
     */
    int32_t current_x100 = 10000; /* 100.00 mA discharge */
    uint32_t dt_ms = 2000;        /* 2 seconds */

    battery_coulomb_reset(22000); /* 220.00 mAh full CR2032 baseline */

    /* First call: sets prev, no integration */
    battery_coulomb_update(current_x100, dt_ms);

    /* 1800 intervals of 2s = 3600s of integration. */
    for (int i = 0; i < 1800; i++) {
        battery_coulomb_update(current_x100, dt_ms);
    }

    int32_t mah;
    battery_coulomb_get_mah_x100(&mah);
    /* 22000 - (100 mA * 1 h) = 22000 - 10000 = 12000 in x100 */
    TEST_ASSERT_INT32_WITHIN(50, 12000, mah);
}

void test_constant_charge_negative_current(void)
{
    /*
     * Q-as-remaining: -50 mA (negative = charge) for 1 hour starting from
     * a 220 mAh baseline should leave 270 mAh remaining (= 27000 x100).
     */
    int32_t current_x100 = -5000; /* -50.00 mA charge */
    uint32_t dt_ms = 2000;

    battery_coulomb_reset(22000); /* 220.00 mAh baseline */

    battery_coulomb_update(current_x100, dt_ms); /* init prev */

    for (int i = 0; i < 1800; i++) {
        battery_coulomb_update(current_x100, dt_ms);
    }

    int32_t mah;
    battery_coulomb_get_mah_x100(&mah);
    /* 22000 + (50 mA * 1 h) = 22000 + 5000 = 27000 in x100 */
    TEST_ASSERT_INT32_WITHIN(50, 27000, mah);
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
     * Q-as-remaining: ramp discharge from 0 to 100 mA over 1 hour.
     * Average current = 50 mA over 1 h = 50 mAh consumed. From a 220 mAh
     * baseline, Q ends at 170 mAh = 17000 x100.
     */
    uint32_t dt_ms = 2000;
    int steps = 1800;

    battery_coulomb_reset(22000); /* 220.00 mAh baseline */

    /* First call at 0 mA — sets prev, no integration */
    battery_coulomb_update(0, dt_ms);

    for (int i = 1; i <= steps; i++) {
        /* Linear ramp: i/steps * 10000 */
        int32_t current_x100 = (int32_t)((int64_t)i * 10000 / steps);
        battery_coulomb_update(current_x100, dt_ms);
    }

    int32_t mah;
    battery_coulomb_get_mah_x100(&mah);
    /* 22000 - 5000 (avg 50 mA * 1 h) = 17000 in x100 */
    TEST_ASSERT_INT32_WITHIN(50, 17000, mah);
}

/* ── Q-as-remaining semantics (issue #1, v0.8.4) ──────────────────── */

void test_positive_current_decreases_q_as_remaining(void)
{
    /*
     * Explicit guard: with Q-as-remaining semantics, positive (discharge)
     * current MUST strictly decrease Q. This is the assertion that flipped
     * vs v0.8.3, and is the core invariant the SoC estimator relies on.
     */
    int32_t mah_before;
    int32_t mah_after;

    battery_coulomb_reset(22000);
    battery_coulomb_get_mah_x100(&mah_before);

    battery_coulomb_update(280, 0);     /* init prev at 2.80 mA */
    for (int i = 0; i < 100; i++) {
        battery_coulomb_update(280, 2000);  /* 200 s @ 2.80 mA */
    }

    battery_coulomb_get_mah_x100(&mah_after);
    TEST_ASSERT_LESS_THAN_INT32(mah_before, mah_after);
}

void test_negative_current_increases_q_as_remaining(void)
{
    /*
     * Negative current (charge) MUST strictly increase Q-as-remaining.
     */
    int32_t mah_before;
    int32_t mah_after;

    battery_coulomb_reset(22000);
    battery_coulomb_get_mah_x100(&mah_before);

    battery_coulomb_update(-280, 0);
    for (int i = 0; i < 100; i++) {
        battery_coulomb_update(-280, 2000);
    }

    battery_coulomb_get_mah_x100(&mah_after);
    TEST_ASSERT_GREATER_THAN_INT32(mah_before, mah_after);
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

    /* Q-as-remaining semantics */
    RUN_TEST(test_positive_current_decreases_q_as_remaining);
    RUN_TEST(test_negative_current_increases_q_as_remaining);

    /* Reset */
    RUN_TEST(test_reset_sets_value);
    RUN_TEST(test_reset_persists_to_nvs);

    return UNITY_END();
}
