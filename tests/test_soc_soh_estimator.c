/*
 * Integration test: SoC estimator drives the SoH learner through the
 * full->empty discharge anchor edges (Phase 8d).
 *
 * Compiled with CONFIG_BATTERY_SOC_COULOMB=1 and CONFIG_BATTERY_SOC_SOH=1,
 * CR2032 chemistry (no CONFIG_BATTERY_CHEMISTRY_LIPO), capacity 220 mAh.
 * CR2032 anchor thresholds: full >= 2950 mV, empty <= 2000 mV.
 *
 * The estimator arms SoH at the full anchor edge and feeds it the remaining
 * coulomb charge Q at the empty anchor edge (captured *before* the counter
 * is reset to 0). For an aged cell the empty-voltage region is reached while
 * Q is still positive, so measured = rated - Q < rated and SoH drops below
 * 100%.
 *
 * Mocks (same set as test_soc_coulomb_cr2032.c): mock_voltage, mock_current,
 * mock_nvs, mock_sdk_state.
 */

#include "unity.h"

#include <battery_sdk/battery_soc_estimator.h>
#include <battery_sdk/battery_coulomb.h>
#include <battery_sdk/battery_soh.h>
#include <battery_sdk/battery_status.h>

#include <stdint.h>

/* ── Mock control prototypes ───────────────────────────────────────────── */

void mock_voltage_set_rc(int rc);
void mock_voltage_set_mv(uint16_t mv);
void mock_current_set_value(int32_t v);
void mock_current_set_read_rc(int rc);
void mock_current_reset(void);
void mock_nvs_reset(void);

/* CR2032 defaults injected via CMake: capacity 220 mAh = 22000 x100. */
#define RATED_X100  22000

/* ── Test fixtures ──────────────────────────────────────────────────────── */

void setUp(void)
{
    mock_nvs_reset();
    mock_current_reset();
    mock_voltage_set_rc(BATTERY_STATUS_OK);
    battery_coulomb_init();
    battery_soc_estimator_init();   /* also re-inits SoH to rated */
}

void tearDown(void) {}

/* ── Tests ──────────────────────────────────────────────────────────────── */

/*
 * Aged full->empty excursion: the estimator must arm SoH at the full anchor,
 * then at the empty anchor feed the still-positive remaining charge Q so the
 * learner records measured = rated - Q < rated and SoH drops below 100%.
 *
 * Q is driven down by real coulomb integration through the mid-voltage region
 * (the estimator only reads Q; the telemetry loop drives the integrator), so
 * this exercises the same data path as firmware.
 *
 * Deterministic arithmetic (alpha=500, REJECT 30..120%):
 *   Q lands at 4400 x100 (20% of rated) at the empty edge.
 *   measured = 22000 - 4400 = 17600.
 *   learned  = 22000 + (17600 - 22000) * 500 / 1000 = 19800.
 *   SoH      = 19800 / 22000 * 10000 = 9000 (90.00%).
 */
void test_aged_excursion_drops_soh_below_100(void)
{
    uint16_t soc;
    int32_t q;

    /* 1) Full anchor: CR2032 at rest above 2950 mV. SoH arms, Q := 22000. */
    mock_voltage_set_mv(3322);
    mock_current_set_value(0);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_estimator_get_pct_x100(&soc));
    TEST_ASSERT_EQUAL_UINT16(10000, soc);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_coulomb_get_mah_x100(&q));
    TEST_ASSERT_EQUAL_INT32(RATED_X100, q);

    /* 2) Mid-range discharge: tick Q down to exactly 4400 via integration.
     *    Hold the estimator in the mid region (V between empty and full) so
     *    the anchors re-arm but neither fires. Seed the integrator's prev
     *    current, then one trapezoidal step removes 176.000 mAh:
     *      delta_x1000 = avg_x100 * dt_ms / 360000
     *                  = 8800 * 7200000 / 360000 = 176000  (176.000 mAh)
     *    Q = 22000 - 17600 = 4400. */
    mock_voltage_set_mv(2500);          /* mid region, both anchors re-arm */
    mock_current_set_value(8800);       /* 88.00 mA discharge */
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_estimator_get_pct_x100(&soc));
    battery_coulomb_update(8800, 0);    /* seed prev_current (no integration) */
    battery_coulomb_update(8800, 7200000);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_coulomb_get_mah_x100(&q));
    TEST_ASSERT_EQUAL_INT32(4400, q);   /* aged: 20% remaining at cutoff */

    /* Re-poll the estimator mid-region (no anchor edge, SoH still armed). */
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_estimator_get_pct_x100(&soc));

    /* 3) Empty anchor: V drops to <= 2000 mV. The estimator captures Q=4400
     *    BEFORE resetting the counter and feeds it to the SoH learner. */
    mock_voltage_set_mv(1900);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_estimator_get_pct_x100(&soc));

    /* SoH dropped below 100% — exact deterministic value 9000 (90.00%). */
    uint16_t soh;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_soh_get_pct_x100(&soh));
    TEST_ASSERT_TRUE(soh < 10000);
    TEST_ASSERT_EQUAL_UINT16(9000, soh);

    /* Learned capacity moved toward measured: 19800 x100. */
    int32_t cap;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soh_get_learned_capacity_mah_x100(&cap));
    TEST_ASSERT_EQUAL_INT32(19800, cap);
}

/*
 * Healthy excursion: the battery is fully discharged (Q reaches 0) before the
 * empty anchor fires, so measured == rated and SoH stays at 100%. Guards
 * against the learner spuriously aging a good cell.
 */
void test_healthy_excursion_keeps_soh_at_100(void)
{
    uint16_t soc;

    /* Full anchor arms SoH, Q := 22000. */
    mock_voltage_set_mv(3322);
    mock_current_set_value(0);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_estimator_get_pct_x100(&soc));

    /* Drain Q to exactly 0 in the mid region. */
    mock_voltage_set_mv(2500);
    (void)battery_coulomb_reset(0);

    /* Empty anchor fires with Q == 0 -> measured == rated -> SoH unchanged. */
    mock_voltage_set_mv(1900);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_estimator_get_pct_x100(&soc));

    uint16_t soh;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_soh_get_pct_x100(&soh));
    TEST_ASSERT_EQUAL_UINT16(10000, soh);
}

/* ── Runner ─────────────────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_aged_excursion_drops_soh_below_100);
    RUN_TEST(test_healthy_excursion_keeps_soh_at_100);
    return UNITY_END();
}
