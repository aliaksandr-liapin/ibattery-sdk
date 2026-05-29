/*
 * Test: SoC estimator with coulomb counting path on CR2032 chemistry.
 *
 * Compiled with CONFIG_BATTERY_SOC_COULOMB=1 and (implicit CR2032 — no
 * CONFIG_BATTERY_CHEMISTRY_LIPO). Capacity = 220 mAh (CR2032 default).
 *
 * Targets issue #1 (v0.8.4): the CR2032 full anchor fires on every sample
 * because SOC_ANCHOR_FULL_I_X100 == 0 short-circuits the |I| condition.
 * The fix is to gate the anchor so it only fires on transition into the
 * anchor region, not while the voltage stays there.
 */

#include "unity.h"

#include <battery_sdk/battery_soc_estimator.h>
#include <battery_sdk/battery_coulomb.h>
#include <battery_sdk/battery_status.h>

#include <stdint.h>

/* ── Mock control prototypes ───────────────────────────────────────────── */

void mock_voltage_set_rc(int rc);
void mock_voltage_set_mv(uint16_t mv);
void mock_current_set_value(int32_t v);
void mock_current_set_read_rc(int rc);
void mock_current_reset(void);
void mock_nvs_reset(void);

/* ── Test fixtures ──────────────────────────────────────────────────────── */

void setUp(void)
{
    mock_nvs_reset();
    mock_current_reset();
    mock_voltage_set_mv(3322);          /* fresh CR2032 typical */
    mock_voltage_set_rc(BATTERY_STATUS_OK);
    battery_coulomb_init();
    battery_soc_estimator_init();
}

void tearDown(void) {}

/* ── Tests ──────────────────────────────────────────────────────────────── */

/*
 * Bug B regression test (issue #1).
 *
 * On CR2032 with V=3322 mV (well above the 2950 mV full-anchor threshold)
 * and a steady discharge current, the SoC estimator must not re-anchor Q
 * to full capacity on every sample. The anchor reset should be a one-shot
 * calibration event that fires when entering the anchor region, then lets
 * the coulomb counter integrate normally.
 *
 * Reproduces the v0.8.3 hardware observation: I=2.80 mA stable for 5 min,
 * Q pinned at 220.00 mAh because the estimator kept resetting it.
 */
void test_cr2032_full_anchor_fires_once_not_every_sample(void)
{
    uint16_t soc;
    int32_t q_baseline;
    int32_t q_after_integration;

    /* CR2032 at 3322 mV, 2.80 mA discharge (positive = discharge). */
    mock_voltage_set_mv(3322);
    mock_current_set_value(280);

    /* First estimator call — anchor fires, Q resets to capacity (220 mAh
     * = 22000 x100). */
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_estimator_get_pct_x100(&soc));
    TEST_ASSERT_EQUAL_UINT16(10000, soc);            /* 100.00% */
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_coulomb_get_mah_x100(&q_baseline));
    TEST_ASSERT_EQUAL_INT32(22000, q_baseline);      /* 220.00 mAh */

    /* Seed integrator prev_current. */
    battery_coulomb_update(280, 0);

    /* Simulate 100 samples at 2-second intervals (200 s total) while also
     * polling the estimator each sample (mirrors the telemetry loop). */
    for (int i = 0; i < 100; i++) {
        battery_coulomb_update(280, 2000);
        TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                              battery_soc_estimator_get_pct_x100(&soc));
    }

    /* Expected discharge after 200 s at 2.80 mA:
     *   2.80 mA * (200 / 3600) h = 0.1556 mAh ≈ 15.56 in x100.
     * Q-as-remaining: Q should be ≈ 22000 - 16 = 21984.
     *
     * With Bug B in place, Q stays pinned at 22000 because the anchor
     * resets it on every sample.
     */
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_coulomb_get_mah_x100(&q_after_integration));
    TEST_ASSERT_LESS_THAN_INT32(q_baseline, q_after_integration);
    TEST_ASSERT_INT32_WITHIN(5, 21984, q_after_integration);
}

/*
 * On the first sample at boot, the full anchor still fires — Q gets
 * calibrated to capacity. This is the desired behavior at fresh-battery
 * boot, and the regression test above confirms it's still happening.
 *
 * This redundant assertion guards against an over-correction where someone
 * disables the full anchor entirely.
 */
void test_cr2032_full_anchor_still_fires_at_boot(void)
{
    uint16_t soc;
    int32_t q;

    mock_voltage_set_mv(3322);
    mock_current_set_value(0);          /* idle */

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_estimator_get_pct_x100(&soc));
    TEST_ASSERT_EQUAL_UINT16(10000, soc);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_coulomb_get_mah_x100(&q));
    TEST_ASSERT_EQUAL_INT32(22000, q);
}

/* ── Runner ─────────────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_cr2032_full_anchor_fires_once_not_every_sample);
    RUN_TEST(test_cr2032_full_anchor_still_fires_at_boot);
    return UNITY_END();
}
