#include "unity.h"
#include <battery_sdk/battery_temperature.h>
#include <battery_sdk/battery_status.h>
#include <stdint.h>

/* Mock control functions — implemented in mocks/ */
extern void mock_hal_set_temp_init_rc(int rc);
extern void mock_hal_set_temp_read_rc(int rc);
extern void mock_hal_set_temp_c_x100(int32_t val);

void setUp(void)
{
    mock_hal_set_temp_init_rc(BATTERY_STATUS_OK);
    mock_hal_set_temp_read_rc(BATTERY_STATUS_OK);
    mock_hal_set_temp_c_x100(2500);

    /* Re-init the module before each test */
    battery_temperature_init();
}

void tearDown(void) {}

/* ── Null check ──────────────────────────────────────────────────────────── */

void test_get_null_pointer(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_temperature_get_c_x100(NULL));
}

/* ── Happy path ──────────────────────────────────────────────────────────── */

void test_get_returns_hal_value(void)
{
    int32_t temp;
    mock_hal_set_temp_c_x100(2350);  /* 23.50 C */

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_temperature_get_c_x100(&temp));
    TEST_ASSERT_EQUAL_INT32(2350, temp);
}

void test_get_negative_temperature(void)
{
    int32_t temp;
    mock_hal_set_temp_c_x100(-1050);  /* -10.50 C */

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_temperature_get_c_x100(&temp));
    TEST_ASSERT_EQUAL_INT32(-1050, temp);
}

void test_get_zero_temperature(void)
{
    int32_t temp;
    mock_hal_set_temp_c_x100(0);  /* 0.00 C */

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_temperature_get_c_x100(&temp));
    TEST_ASSERT_EQUAL_INT32(0, temp);
}

/* ── HAL failure propagation ─────────────────────────────────────────────── */

void test_get_hal_io_error(void)
{
    int32_t temp;
    mock_hal_set_temp_read_rc(BATTERY_STATUS_IO);

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_IO,
                          battery_temperature_get_c_x100(&temp));
}

/* ── Init ────────────────────────────────────────────────────────────────── */

void test_init_hal_failure(void)
{
    mock_hal_set_temp_init_rc(BATTERY_STATUS_IO);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_IO,
                          battery_temperature_init());
}

void test_init_success(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_temperature_init());
}

/* ── Runner ──────────────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_get_null_pointer);
    RUN_TEST(test_get_returns_hal_value);
    RUN_TEST(test_get_negative_temperature);
    RUN_TEST(test_get_zero_temperature);
    RUN_TEST(test_get_hal_io_error);
    RUN_TEST(test_init_hal_failure);
    RUN_TEST(test_init_success);

    return UNITY_END();
}
