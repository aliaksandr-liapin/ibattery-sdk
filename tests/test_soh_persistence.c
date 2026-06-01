/* SoH NVS persistence — learned capacity survives a reboot. */
#include "unity.h"
#include <battery_sdk/battery_soh.h>
#include <battery_sdk/battery_status.h>
#include <stdint.h>
#include <stdbool.h>

/* mock_nvs controls */
extern void mock_nvs_reset(void);
extern void mock_nvs_set_init_rc(int rc);
extern void mock_nvs_set_write_rc(int rc);
extern void mock_nvs_set_stored_value_key(uint16_t key, uint32_t v);
extern bool mock_nvs_get_value_key(uint16_t key, uint32_t *out);

/* Keys (mirror battery_hal_nvs.h) */
#define KEY_SOH_LEARNED 3
#define KEY_SOH_RATED   4

#define RATED 22000  /* 220.00 mAh x100 */

void setUp(void)    { mock_nvs_reset(); }
void tearDown(void) {}

/* Drive one valid full->empty excursion that lands learned below rated. */
static void run_aged_excursion(int32_t q_before_empty_x100)
{
    battery_soh_note_full_anchor();
    battery_soh_observe_empty_anchor(q_before_empty_x100);
}

void test_learned_restores_across_reboot(void)
{
    /* Boot 1: learn a faded capacity. */
    TEST_ASSERT_EQUAL(BATTERY_STATUS_OK, battery_soh_init(RATED));
    run_aged_excursion(4400);  /* measured = 22000-4400 = 17600 (80%) */

    int32_t learned1 = 0;
    battery_soh_get_learned_capacity_mah_x100(&learned1);
    TEST_ASSERT_TRUE(learned1 < RATED);   /* it faded */

    /* Reboot: re-init WITHOUT clearing the mock's flash. */
    TEST_ASSERT_EQUAL(BATTERY_STATUS_OK, battery_soh_init(RATED));

    int32_t learned2 = 0;
    battery_soh_get_learned_capacity_mah_x100(&learned2);
    TEST_ASSERT_EQUAL_INT32(learned1, learned2);  /* restored, not reset to RATED */
}

void test_profile_change_discards_stored_learned(void)
{
    /* Flash holds a learned value from a DIFFERENT rated capacity. */
    mock_nvs_set_stored_value_key(KEY_SOH_RATED, 100000);   /* 1000 mAh pack */
    mock_nvs_set_stored_value_key(KEY_SOH_LEARNED, 80000);  /* 80% of that  */

    /* Boot with the CR2032 profile (RATED = 22000). */
    TEST_ASSERT_EQUAL(BATTERY_STATUS_OK, battery_soh_init(RATED));

    int32_t learned = 0;
    battery_soh_get_learned_capacity_mah_x100(&learned);
    TEST_ASSERT_EQUAL_INT32(RATED, learned);  /* stale value rejected */

    /* And the current rated should now be stamped to flash. */
    uint32_t stamped = 0;
    TEST_ASSERT_TRUE(mock_nvs_get_value_key(KEY_SOH_RATED, &stamped));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)RATED, stamped);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_learned_restores_across_reboot);
    RUN_TEST(test_profile_change_discards_stored_learned);
    return UNITY_END();
}
