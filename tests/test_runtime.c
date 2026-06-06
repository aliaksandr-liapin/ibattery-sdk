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
/* Default idle floor is 0.50 mA (CONFIG_BATTERY_RUNTIME_IDLE_THRESHOLD_MA_X100=50):
 * a smoothed current at or below it is noise, not a meaningful drain to project.
 * This bounds the largest emittable estimate to Q/floor, so the load-removal
 * transient can no longer spike to multi-year values before settling to n/a. */
void test_below_floor_not_available(void){
    for(int i=0;i<50;i++) battery_runtime_update(10);            /* 0.10 mA (< 0.50 floor) */
    uint32_t m=123;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_NOT_AVAILABLE, battery_runtime_to_empty_min(22000,&m));
}
void test_at_floor_not_available(void){
    for(int i=0;i<50;i++) battery_runtime_update(50);            /* exactly 0.50 mA; gate is <= */
    uint32_t m=123;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_NOT_AVAILABLE, battery_runtime_to_empty_min(22000,&m));
}
void test_just_above_floor_reports(void){
    for(int i=0;i<50;i++) battery_runtime_update(51);            /* 0.51 mA (> 0.50 floor) */
    uint32_t m=0;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_runtime_to_empty_min(6000,&m)); /* 60.00 mAh */
    TEST_ASSERT_EQUAL_UINT32(7058, m);                           /* 6000*60/51 */
}
void test_above_floor_value(void){
    for(int i=0;i<20;i++) battery_runtime_update(100);           /* 1.00 mA */
    uint32_t m=0;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_runtime_to_empty_min(6000,&m)); /* 60.00 mAh */
    TEST_ASSERT_EQUAL_UINT32(3600, m);                           /* 60*60/1.0 */
}
void test_estimate_never_collides_with_sentinel(void){
    /* With the floor in place the largest possible estimate is Q/floor, which
     * stays well below UINT32_MAX, so a real estimate can never be mistaken for
     * the wire-v5 NOT_AVAILABLE sentinel. */
    for(int i=0;i<50;i++) battery_runtime_update(51);            /* just above floor */
    uint32_t m=0;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_runtime_to_empty_min(INT32_MAX,&m));
    TEST_ASSERT_TRUE(m < UINT32_MAX - 1);
}
void test_null_out(void){
    for(int i=0;i<5;i++) battery_runtime_update(6000);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG, battery_runtime_to_empty_min(6000, NULL));
}
int main(void){ UNITY_BEGIN(); RUN_TEST(test_basic); RUN_TEST(test_idle_not_available); RUN_TEST(test_charging_not_available); RUN_TEST(test_remaining_zero); RUN_TEST(test_below_floor_not_available); RUN_TEST(test_at_floor_not_available); RUN_TEST(test_just_above_floor_reports); RUN_TEST(test_above_floor_value); RUN_TEST(test_estimate_never_collides_with_sentinel); RUN_TEST(test_null_out); return UNITY_END(); }
