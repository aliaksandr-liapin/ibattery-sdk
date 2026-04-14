/*
 * Test: SoC estimator with coulomb counting path.
 *
 * Compiled with CONFIG_BATTERY_SOC_COULOMB=1 and
 * CONFIG_BATTERY_CHEMISTRY_LIPO=1.
 */

#include "unity.h"

#include <battery_sdk/battery_soc_estimator.h>
#include <battery_sdk/battery_coulomb.h>
#include <battery_sdk/battery_status.h>

#include <stdint.h>

/* ── Mock control prototypes ───────────────────────────────────────────── */

/* mock_voltage.c */
void mock_voltage_set_rc(int rc);
void mock_voltage_set_mv(uint16_t mv);

/* mock_current.c */
void mock_current_set_value(int32_t v);
void mock_current_set_read_rc(int rc);
void mock_current_reset(void);

/* mock_nvs.c */
void mock_nvs_reset(void);

/* ── Test fixtures ──────────────────────────────────────────────────────── */

void setUp(void)
{
    mock_nvs_reset();
    mock_current_reset();
    mock_voltage_set_mv(3800);
    mock_voltage_set_rc(BATTERY_STATUS_OK);
    battery_coulomb_init();
    battery_soc_estimator_init();
}

void tearDown(void) {}

/* ── Tests ──────────────────────────────────────────────────────────────── */

/*
 * First call with no prior anchor: estimator seeds from LUT.
 * At 4200 mV (LiPo) the LUT returns 10000 (100%).
 * Current is 0 mA (idle) and voltage >= 4180 mV, so full anchor fires.
 */
void test_init_uses_voltage_lut(void)
{
    uint16_t soc;
    mock_voltage_set_mv(4200);
    mock_current_set_value(0);   /* 0 mA — low current */

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_estimator_get_pct_x100(&soc));
    TEST_ASSERT_EQUAL_UINT16(10000, soc);
}

/*
 * When current sensor returns an error, the estimator gracefully
 * falls back to pure voltage-LUT SoC.
 */
void test_fallback_when_current_fails(void)
{
    uint16_t soc;
    mock_voltage_set_mv(3870);   /* LiPo LUT: 55% = 5500 */
    mock_current_set_read_rc(BATTERY_STATUS_ERROR);

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_estimator_get_pct_x100(&soc));
    TEST_ASSERT_EQUAL_UINT16(5500, soc);
}

/*
 * Null output pointer returns INVALID_ARG.
 */
void test_null_returns_invalid_arg(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_soc_estimator_get_pct_x100(NULL));
}

/*
 * If voltage read fails, error propagates immediately.
 */
void test_voltage_error_propagates(void)
{
    uint16_t soc;
    mock_voltage_set_rc(BATTERY_STATUS_ERROR);

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_ERROR,
                          battery_soc_estimator_get_pct_x100(&soc));
}

/*
 * Full anchor: voltage >= 4180 mV and |current| < 50 mA
 * should reset coulomb counter to full capacity, yielding ~100%.
 */
void test_full_anchor_resets_at_4180mv(void)
{
    uint16_t soc;
    mock_voltage_set_mv(4200);
    mock_current_set_value(1000);  /* 10.00 mA — well below 50 mA threshold */

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_estimator_get_pct_x100(&soc));
    /* Full anchor should give 100% */
    TEST_ASSERT_EQUAL_UINT16(10000, soc);
}

/*
 * At 2900 mV (below 3000 mV empty anchor for LiPo), the SoC should
 * be anchored to 0%.
 */
void test_empty_anchor_near_zero(void)
{
    uint16_t soc;
    mock_voltage_set_mv(2900);
    mock_current_set_value(-5000);  /* -50 mA discharge */

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_estimator_get_pct_x100(&soc));
    TEST_ASSERT_EQUAL_UINT16(0, soc);
}

/*
 * Mid-range voltage (3800 mV) with working current sensor: the estimator
 * should seed from LUT on first call, then return a coulomb-based SoC.
 * With no integration updates, the result equals the LUT seed.
 *
 * LiPo LUT at 3800 mV: between 3790 (30%) and 3830 (45%).
 * Linear interpolation: 30% + (3800-3790)/(3830-3790) * (45%-30%)
 *                      = 3000 + 10/40 * 1500 = 3000 + 375 = 3375
 */
void test_mid_range_seeds_from_lut(void)
{
    uint16_t soc;
    mock_voltage_set_mv(3800);
    mock_current_set_value(-10000);  /* -100 mA — above anchor threshold */

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_estimator_get_pct_x100(&soc));
    /* Seed from LUT: 3375 x100, then coulomb_reset sets accumulator to
     * that fraction of capacity.  get_mah_x100 -> seed_mah_x100,
     * then soc = seed_mah_x100 * 10000 / (capacity * 100).
     * seed_mah_x100 = 3375 * 1000 / 100 = 33750
     * soc = 33750 * 10000 / 100000 = 3375
     */
    TEST_ASSERT_EQUAL_UINT16(3375, soc);
}

/* ── Runner ─────────────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_uses_voltage_lut);
    RUN_TEST(test_fallback_when_current_fails);
    RUN_TEST(test_null_returns_invalid_arg);
    RUN_TEST(test_voltage_error_propagates);
    RUN_TEST(test_full_anchor_resets_at_4180mv);
    RUN_TEST(test_empty_anchor_near_zero);
    RUN_TEST(test_mid_range_seeds_from_lut);
    return UNITY_END();
}
