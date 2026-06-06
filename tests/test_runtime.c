#include "unity.h"
#include "battery_sdk/battery_runtime.h"
#include "battery_sdk/battery_status.h"
void setUp(void){ battery_runtime_reset(); }
void tearDown(void){}
void test_basic(void){
    for(int i=0;i<20;i++) battery_runtime_update(6000);          /* 60.00 mA */
    uint32_t m=0;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_runtime_to_empty_min(6000,&m)); /* 60.00 mAh */
    TEST_ASSERT_EQUAL_UINT32(60, m);                              /* 60*60/60 = 60 min */
}
void test_idle_not_available(void){
    for(int i=0;i<20;i++) battery_runtime_update(0);
    uint32_t m=123;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_NOT_AVAILABLE, battery_runtime_to_empty_min(6000,&m));
}
void test_charging_not_available(void){
    for(int i=0;i<20;i++) battery_runtime_update(-5000);
    uint32_t m=123;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_NOT_AVAILABLE, battery_runtime_to_empty_min(6000,&m));
}
void test_remaining_zero(void){
    for(int i=0;i<20;i++) battery_runtime_update(6000);
    uint32_t m=123;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_runtime_to_empty_min(0,&m));
    TEST_ASSERT_EQUAL_UINT32(0,m);
}
void test_low_drain_no_overflow(void){
    for(int i=0;i<50;i++) battery_runtime_update(10);            /* 0.10 mA */
    uint32_t m=0;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_runtime_to_empty_min(22000,&m)); /* 220.00 mAh */
    TEST_ASSERT_EQUAL_UINT32(132000,m);                          /* 220*60/0.1 */
}
void test_null_out(void){
    for(int i=0;i<5;i++) battery_runtime_update(6000);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG, battery_runtime_to_empty_min(6000, NULL));
}
void test_clamp_to_uint32_max_minus_one(void){
    for(int i=0;i<50;i++) battery_runtime_update(1);              /* 0.01 mA */
    uint32_t m=0;
    /* numerator = INT32_MAX*60 ~= 1.3e11 / 1 (ema) overflows uint32_t -> clamps */
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_runtime_to_empty_min(INT32_MAX,&m));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX - 1, m);
}
int main(void){ UNITY_BEGIN(); RUN_TEST(test_basic); RUN_TEST(test_idle_not_available); RUN_TEST(test_charging_not_available); RUN_TEST(test_remaining_zero); RUN_TEST(test_low_drain_no_overflow); RUN_TEST(test_null_out); RUN_TEST(test_clamp_to_uint32_max_minus_one); return UNITY_END(); }
