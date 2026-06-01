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

void test_reset_persists_across_reboot(void)
{
    TEST_ASSERT_EQUAL(BATTERY_STATUS_OK, battery_soh_init(RATED));
    run_aged_excursion(4400);          /* fade it */
    battery_soh_reset();               /* back to 100% */

    int32_t after_reset = 0;
    battery_soh_get_learned_capacity_mah_x100(&after_reset);
    TEST_ASSERT_EQUAL_INT32(RATED, after_reset);

    /* Reboot — the reset must survive, not resurrect the faded value. */
    TEST_ASSERT_EQUAL(BATTERY_STATUS_OK, battery_soh_init(RATED));
    int32_t after_reboot = 0;
    battery_soh_get_learned_capacity_mah_x100(&after_reboot);
    TEST_ASSERT_EQUAL_INT32(RATED, after_reboot);
}

void test_first_boot_no_nvs_defaults_to_rated(void)
{
    mock_nvs_set_init_rc(BATTERY_STATUS_ERROR);  /* NVS unavailable */
    TEST_ASSERT_EQUAL(BATTERY_STATUS_OK, battery_soh_init(RATED));
    int32_t learned = 0;
    battery_soh_get_learned_capacity_mah_x100(&learned);
    TEST_ASSERT_EQUAL_INT32(RATED, learned);
}

void test_write_failure_still_learns_in_session(void)
{
    TEST_ASSERT_EQUAL(BATTERY_STATUS_OK, battery_soh_init(RATED));
    mock_nvs_set_write_rc(BATTERY_STATUS_IO);   /* writes fail */
    run_aged_excursion(4400);
    int32_t learned = 0;
    battery_soh_get_learned_capacity_mah_x100(&learned);
    TEST_ASSERT_TRUE(learned < RATED);          /* RAM learning unaffected */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_learned_restores_across_reboot);
    RUN_TEST(test_profile_change_discards_stored_learned);
    RUN_TEST(test_reset_persists_across_reboot);
    RUN_TEST(test_first_boot_no_nvs_defaults_to_rated);
    RUN_TEST(test_write_failure_still_learns_in_session);
    return UNITY_END();
}
