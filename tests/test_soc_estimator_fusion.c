/*
 * Integration tests for the SoC estimator with Phase 8c fusion enabled.
 *
 * Compiled with CONFIG_BATTERY_SOC_FUSION=1 and CR2032 chemistry
 * (default) at capacity = 220 mAh, matching the v0.8.4 validated hardware.
 */

#include "unity.h"

#include <battery_sdk/battery_soc_estimator.h>
#include <battery_sdk/battery_coulomb.h>
#include <battery_sdk/battery_status.h>

#include <stdint.h>

void mock_voltage_set_rc(int rc);
void mock_voltage_set_mv(uint16_t mv);
void mock_current_set_value(int32_t v);
void mock_current_set_read_rc(int rc);
void mock_current_reset(void);
void mock_nvs_reset(void);

void setUp(void)
{
    mock_nvs_reset();
    mock_current_reset();
    mock_voltage_set_mv(3322);
    mock_voltage_set_rc(BATTERY_STATUS_OK);
    battery_coulomb_init();
    battery_soc_estimator_init();
}

void tearDown(void) {}

/*
 * Fusion is a no-op on the first sample because soc_c is seeded from LUT,
 * so blend(lut_soc, lut_soc) = lut_soc.
 */
void test_fusion_noop_on_first_sample(void)
{
    uint16_t soc;
    mock_voltage_set_mv(3322);
    mock_current_set_value(280);          /* 2.80 mA — above CR2032 thresh (2.00 mA) */

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_estimator_get_pct_x100(&soc));
    /* At 3322 mV on a fresh CR2032 the LUT says 100% (capped). After
     * anchor fires once (V > 2950 + |I| gate disabled), Q = capacity.
     * Fusion blends 10000 with 10000 -> 10000. */
    TEST_ASSERT_EQUAL_UINT16(10000, soc);
}

/*
 * Anchor still fires on first sample at full voltage.
 */
void test_anchor_still_fires_with_fusion_enabled(void)
{
    uint16_t soc;
    int32_t q;

    mock_voltage_set_mv(3322);
    mock_current_set_value(0);            /* idle */

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_estimator_get_pct_x100(&soc));
    TEST_ASSERT_EQUAL_UINT16(10000, soc);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_coulomb_get_mah_x100(&q));
    TEST_ASSERT_EQUAL_INT32(22000, q);    /* CR2032 capacity = 220 mAh = 22000 x100 */
}

/*
 * Bug B (v0.8.4) regression: anchor must NOT re-fire on every sample.
 * Fusion enabled should not change this.
 */
void test_anchor_does_not_refire_with_fusion_enabled(void)
{
    uint16_t soc;
    int32_t q_initial, q_later;

    mock_voltage_set_mv(3322);
    mock_current_set_value(280);          /* 2.80 mA discharge */

    /* First call — anchor fires, Q = capacity. */
    battery_soc_estimator_get_pct_x100(&soc);
    battery_coulomb_get_mah_x100(&q_initial);
    TEST_ASSERT_EQUAL_INT32(22000, q_initial);

    /* Seed integrator and drive 100 samples. */
    battery_coulomb_update(280, 0);
    for (int i = 0; i < 100; i++) {
        battery_coulomb_update(280, 2000);
        battery_soc_estimator_get_pct_x100(&soc);
    }

    battery_coulomb_get_mah_x100(&q_later);
    /* Anchor must NOT have re-fired — Q should be < 22000. */
    TEST_ASSERT_LESS_THAN_INT32(22000, q_later);
}

/*
 * Drift correction at rest: bias Q below capacity, voltage shows ~100%,
 * fusion should pull SoC upward.
 *
 * Note: this test exercises the anchor + fusion interaction. With voltage
 * above the full anchor, the anchor will reset Q to capacity, making
 * fusion a no-op (both signals = 100%). To truly test fusion-only
 * drift correction, we'd need a voltage just below the full anchor.
 * For now, we just verify SoC ends up above 80% in this scenario —
 * the actual drift-correction math is unit-tested in test_soc_fusion.c.
 */
void test_fusion_corrects_drift_at_rest(void)
{
    uint16_t soc;

    /* Bypass anchor — set voltage just below anchor threshold. */
    mock_voltage_set_mv(2949);
    mock_current_set_value(100);          /* 1.00 mA — at rest */

    /* First call seeds Q from LUT at 2949 mV. LUT value depends on the
     * CR2032 table; we just need a known starting point. After seeding,
     * we'll override Q to a known biased value. */
    battery_soc_estimator_get_pct_x100(&soc);
    battery_coulomb_reset(11000);         /* force Q = 50% of 22000 x100 */

    /* Now switch to a voltage that pulls hard toward 100% LUT. */
    mock_voltage_set_mv(3322);

    /* Drive 60 samples. */
    battery_coulomb_update(100, 0);
    for (int i = 0; i < 60; i++) {
        battery_coulomb_update(100, 2000);
        battery_soc_estimator_get_pct_x100(&soc);
    }

    /* With alpha = 50 (rest), 60 samples should produce ~95% closure:
     * starting at 50% with voltage at 100%, SoC should be > 80%.
     * NOTE: anchor will fire as soon as V hits 3322 — that re-anchors Q
     * to capacity. So this test really tests "anchor + fusion don't fight
     * each other"; result should be SoC = ~100% within a sample or two. */
    TEST_ASSERT_GREATER_THAN_UINT16(8000, soc);
}

/*
 * Fusion falls back to LUT when current read fails.
 */
void test_fusion_falls_back_to_lut_on_current_error(void)
{
    uint16_t soc;
    mock_voltage_set_mv(2900);            /* mid-discharge, below full anchor */
    mock_current_set_read_rc(BATTERY_STATUS_ERROR);

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_estimator_get_pct_x100(&soc));
    /* Should return whatever LUT says — verify it's a sensible value,
     * not 0 or 10000. */
    TEST_ASSERT_GREATER_THAN_UINT16(0, soc);
    TEST_ASSERT_LESS_THAN_UINT16(10000, soc);
}

/*
 * Voltage error propagates regardless of fusion state.
 */
void test_voltage_error_propagates(void)
{
    uint16_t soc;
    mock_voltage_set_rc(BATTERY_STATUS_ERROR);

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_ERROR,
                          battery_soc_estimator_get_pct_x100(&soc));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fusion_noop_on_first_sample);
    RUN_TEST(test_anchor_still_fires_with_fusion_enabled);
    RUN_TEST(test_anchor_does_not_refire_with_fusion_enabled);
    RUN_TEST(test_fusion_corrects_drift_at_rest);
    RUN_TEST(test_fusion_falls_back_to_lut_on_current_error);
    RUN_TEST(test_voltage_error_propagates);
    return UNITY_END();
}
