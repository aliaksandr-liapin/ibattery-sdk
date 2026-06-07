#include "unity.h"
#include "battery_sdk/battery_swap.h"
#include "battery_sdk/battery_soh.h"
#include "battery_sdk/battery_status.h"

void     mock_nvs_reset(void);
void     mock_nvs_set_stored_value_key(uint16_t key, uint32_t v);
bool     mock_nvs_get_value_key(uint16_t key, uint32_t *out);
void     mock_nvs_set_write_rc(int rc);

#define KEY_LAST_SOC 5

void setUp(void)   { mock_nvs_reset(); battery_soh_init(22000); }
void tearDown(void){}

void test_first_boot_no_swap_stamps_baseline(void){
    battery_swap_init();
    battery_swap_update(10000);
    TEST_ASSERT_FALSE(battery_swap_detected());
    uint32_t v=0; TEST_ASSERT_TRUE(mock_nvs_get_value_key(KEY_LAST_SOC,&v));
    TEST_ASSERT_EQUAL_UINT32(10000, v);
}
void test_boot_jump_detects_swap_and_resets_soh(void){
    battery_soh_note_full_anchor();
    battery_soh_observe_empty_anchor(5000);
    uint16_t faded=0; battery_soh_get_pct_x100(&faded);
    TEST_ASSERT_TRUE(faded < 10000);
    mock_nvs_set_stored_value_key(KEY_LAST_SOC, 500);
    battery_swap_init();
    battery_swap_update(10000);
    TEST_ASSERT_TRUE(battery_swap_detected());
    uint16_t soh=0; battery_soh_get_pct_x100(&soh);
    TEST_ASSERT_EQUAL_UINT16(10000, soh);
    uint32_t v=0; mock_nvs_get_value_key(KEY_LAST_SOC,&v);
    TEST_ASSERT_EQUAL_UINT32(10000, v);
}
void test_boot_small_rise_no_swap(void){
    mock_nvs_set_stored_value_key(KEY_LAST_SOC, 8000);
    battery_swap_init();
    battery_swap_update(9000);
    TEST_ASSERT_FALSE(battery_swap_detected());
}
void test_boot_lower_soc_no_swap(void){
    mock_nvs_set_stored_value_key(KEY_LAST_SOC, 8000);
    battery_swap_init();
    battery_swap_update(7000);
    TEST_ASSERT_FALSE(battery_swap_detected());
}
void test_persist_throttled_on_drop(void){
    battery_swap_init();
    battery_swap_update(10000);
    battery_swap_update(9800);
    uint32_t v=0; mock_nvs_get_value_key(KEY_LAST_SOC,&v);
    TEST_ASSERT_EQUAL_UINT32(10000, v);
    battery_swap_update(9400);
    mock_nvs_get_value_key(KEY_LAST_SOC,&v);
    TEST_ASSERT_EQUAL_UINT32(9400, v);
}
/* The swap flag is per-session: a fresh init (next power cycle) clears it. */
void test_swap_flag_clears_on_reinit(void){
    mock_nvs_set_stored_value_key(KEY_LAST_SOC, 500);
    battery_swap_init();
    battery_swap_update(10000);              /* swap detected */
    TEST_ASSERT_TRUE(battery_swap_detected());
    battery_swap_init();                     /* new power session */
    TEST_ASSERT_FALSE(battery_swap_detected());
}

/* NVS write is best-effort: a forced write failure must not crash, must not
 * flip the swap flag, and RAM stays authoritative (detection still works). */
void test_persist_write_failure_is_best_effort(void){
    mock_nvs_set_write_rc(BATTERY_STATUS_ERROR);   /* every write fails */
    battery_swap_init();                            /* no stored baseline */
    battery_swap_update(10000);                     /* boot stamp; write fails silently */
    TEST_ASSERT_FALSE(battery_swap_detected());     /* no spurious swap, no crash */
    battery_swap_update(9400);                      /* >=5% drop; write fails again, no crash */
    TEST_ASSERT_FALSE(battery_swap_detected());
    mock_nvs_set_write_rc(BATTERY_STATUS_OK);       /* restore so later tests aren't affected */
}
int main(void){
    UNITY_BEGIN();
    RUN_TEST(test_first_boot_no_swap_stamps_baseline);
    RUN_TEST(test_boot_jump_detects_swap_and_resets_soh);
    RUN_TEST(test_boot_small_rise_no_swap);
    RUN_TEST(test_boot_lower_soc_no_swap);
    RUN_TEST(test_persist_throttled_on_drop);
    RUN_TEST(test_swap_flag_clears_on_reinit);
    RUN_TEST(test_persist_write_failure_is_best_effort);
    return UNITY_END();
}
