#include "unity.h"
#include <battery_sdk/battery_hal_current.h>
#include <battery_sdk/battery_status.h>
#include <stddef.h>

void setUp(void) {}
void tearDown(void) {}

void test_stub_init_returns_unsupported(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_UNSUPPORTED,
                          battery_hal_current_init());
}

void test_stub_read_returns_unsupported(void)
{
    int32_t current;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_UNSUPPORTED,
                          battery_hal_current_read_ma_x100(&current));
}

void test_stub_read_null_returns_unsupported(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_UNSUPPORTED,
                          battery_hal_current_read_ma_x100(NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_stub_init_returns_unsupported);
    RUN_TEST(test_stub_read_returns_unsupported);
    RUN_TEST(test_stub_read_null_returns_unsupported);
    return UNITY_END();
}
