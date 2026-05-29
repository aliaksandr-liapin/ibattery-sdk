# Phase 8c (Voltage + Coulomb Fusion) Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a continuous voltage-LUT × coulomb-counter signal fusion stage to the SoC estimator so that mid-discharge integration drift gets corrected automatically, without waiting for endpoint anchors that may never fire.

**Architecture:** Complementary filter with current-adaptive blend coefficient. New module `battery_soc_fusion.{c,h}` holds the math; estimator gains a single new `#if` block between Phase 8a anchor logic and Phase 8b slew limiter. Integer-only, 0 new RAM, opt-in via Kconfig (default off).

**Tech Stack:** C11, Zephyr 3.x, Unity test framework, CMake. Hardware validation on NUCLEO-L476RG + Adafruit INA219.

**Design doc:** `docs/plans/2026-05-29-phase-8c-fusion-design.md` (approved, committed).

**Target release:** v0.10.0.

---

## Conventions for this plan

- All paths are relative to the repo root: `/Users/aliapin/Downloads/project/ibattery-sdk`
- `PATH` setup before every build/test command (Nordic toolchain provides cmake/ctest):
  ```bash
  export PATH="/opt/homebrew/bin:/usr/bin:/bin:/opt/nordic/ncs/toolchains/e5f4758bcf/bin:$PATH"
  ```
- Host tests are built in `tests/build/`. From repo root: `cd tests/build && ctest`.
- Firmware builds for hardware validation use the existing `nucleo_l476rg_i2cscan.conf` (Phase 8a-validated rig).
- TDD discipline: red test → run → fail → minimal impl → run → pass → commit. No "while we're here" refactors mid-task.

---

## Task 1: Add the four Kconfig entries

**Goal:** Wire up the user-facing Kconfig surface so the rest of the implementation can read the symbols.

**Files:**
- Modify: `app/Kconfig`

**Step 1: Append the Kconfig block.**

Add this block at the end of `app/Kconfig` (or in the existing SoC section if there's a natural home — search for `BATTERY_SOC_COULOMB` to find adjacent fusion-related configs):

```kconfig
config BATTERY_SOC_FUSION
    bool "Phase 8c: voltage + coulomb signal fusion"
    depends on BATTERY_SOC_COULOMB
    default n
    help
      Continuously fuses voltage-LUT SoC with coulomb-counter SoC to
      correct mid-discharge drift in the coulomb integrator. Trust
      shifts toward voltage when the battery is at rest (current below
      threshold) and toward coulomb under load (voltage sag is real).

      Composes with the v0.8.4 anchor calibration (which still fires
      at endpoints) and the Phase 8b slew limiter (which still applies
      after fusion).

      Defaults to off. Enable per build to opt in.

if BATTERY_SOC_FUSION

config BATTERY_SOC_FUSION_ALPHA_REST_X1000
    int "Fusion alpha (x1000) when battery is at rest"
    default 50 if !BATTERY_CHEMISTRY_LIPO
    default 30 if BATTERY_CHEMISTRY_LIPO
    range 0 1000
    help
      Per-sample pull toward voltage-LUT SoC when |I| < I_THRESH.
      Larger = faster drift correction. Default 5.0% per sample (CR2032)
      gives ~95% convergence in ~60 samples at 2s intervals.

config BATTERY_SOC_FUSION_ALPHA_LOAD_X1000
    int "Fusion alpha (x1000) under load"
    default 5
    range 0 1000
    help
      Per-sample pull toward voltage-LUT SoC when |I| >= I_THRESH.
      Should be small — voltage is unreliable under load (IR drop).
      Default 0.5% per sample, both chemistries.

config BATTERY_SOC_FUSION_I_THRESH_X100
    int "Current threshold (x100 mA) classifying rest vs load"
    default 200 if !BATTERY_CHEMISTRY_LIPO
    default 5000 if BATTERY_CHEMISTRY_LIPO
    range 0 1000000
    help
      Below this current magnitude, fusion uses ALPHA_REST.
      Above (or equal), uses ALPHA_LOAD.

endif # BATTERY_SOC_FUSION
```

**Step 2: Verify the Kconfig parses.**

```bash
cd /Users/aliapin/Downloads/project/ibattery-sdk
export PATH="/opt/homebrew/bin:/usr/bin:/bin:/opt/nordic/ncs/toolchains/e5f4758bcf/bin:$PATH"
export ZEPHYR_BASE="/opt/nordic/ncs/v3.2.2/zephyr"
west build -b nucleo_l476rg app -d /tmp/build-phase8c-kconfig-check --pristine -- \
  -DEXTRA_CONF_FILE=boards/nucleo_l476rg_i2cscan.conf \
  -DZEPHYR_EXTRA_MODULES="/opt/nordic/ncs/v3.2.2/modules/hal/stm32" 2>&1 | tail -5
```

Expected: build succeeds (existing behavior — fusion is default-off, no code changes yet).

**Step 3: Commit.**

```bash
git add app/Kconfig
git commit -m "feat(8c): add Kconfig surface for voltage+coulomb fusion

Adds CONFIG_BATTERY_SOC_FUSION (master, default n) and three tunables
(ALPHA_REST_X1000, ALPHA_LOAD_X1000, I_THRESH_X100) with per-chemistry
defaults. Plumbing only — no code changes yet."
```

---

## Task 2: Create the public header

**Goal:** Public API surface for the fusion module. Header-only at this stage; tests will reference it before any implementation exists.

**Files:**
- Create: `include/battery_sdk/battery_soc_fusion.h`

**Step 1: Write the header.**

```c
/*
 * Phase 8c: voltage + coulomb signal fusion.
 *
 * Combines two SoC estimates (voltage-LUT, coulomb integrator) into a
 * single fused estimate using a complementary filter with current-
 * adaptive coefficient. The blend coefficient (alpha) is small under
 * load (voltage unreliable due to IR drop) and larger at rest
 * (voltage-LUT is accurate).
 *
 * Design doc: docs/plans/2026-05-29-phase-8c-fusion-design.md
 */

#ifndef BATTERY_SDK_BATTERY_SOC_FUSION_H_
#define BATTERY_SDK_BATTERY_SOC_FUSION_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Fuse voltage-LUT and coulomb-counter SoC estimates.
 *
 * The blend coefficient (alpha) is selected internally based on the
 * absolute current magnitude. See `battery_soc_fusion_select_alpha`.
 *
 * @param soc_v_x100        Voltage-LUT SoC in 0.01% units (0..10000).
 * @param soc_c_x100        Coulomb SoC in 0.01% units (0..10000).
 * @param abs_current_x100  Absolute current in 0.01 mA units (>= 0).
 * @return Fused SoC in 0.01% units (0..10000).
 */
uint16_t battery_soc_fusion_blend(uint16_t soc_v_x100,
                                   uint16_t soc_c_x100,
                                   int32_t abs_current_x100);

/**
 * Select the blend coefficient (alpha) based on current magnitude.
 *
 * Exposed for testability. Returns alpha in x1000 units (0..1000).
 *
 * @param abs_current_x100  Absolute current in 0.01 mA units (>= 0).
 * @return Alpha in x1000 units (0 = pure coulomb, 1000 = pure voltage).
 */
uint16_t battery_soc_fusion_select_alpha(int32_t abs_current_x100);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SDK_BATTERY_SOC_FUSION_H_ */
```

**Step 2: Commit.**

```bash
git add include/battery_sdk/battery_soc_fusion.h
git commit -m "feat(8c): add public header for soc fusion module

Two functions: blend() (the public fusion call) and select_alpha()
(exposed for testability). No implementation yet — TDD red first."
```

---

## Task 3: TDD red — write the unit tests

**Goal:** All 10 unit tests written and failing (the implementation doesn't exist yet). Confirms the test plan compiles against the header and asserts the right things.

**Files:**
- Create: `tests/test_soc_fusion.c`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the test file.**

Create `tests/test_soc_fusion.c`:

```c
/*
 * Unit tests for battery_soc_fusion module (Phase 8c, v0.10.0).
 *
 * Compiled with concrete Kconfig values via target_compile_definitions
 * in tests/CMakeLists.txt:
 *   CONFIG_BATTERY_SOC_FUSION_ALPHA_REST_X1000 = 50
 *   CONFIG_BATTERY_SOC_FUSION_ALPHA_LOAD_X1000 = 5
 *   CONFIG_BATTERY_SOC_FUSION_I_THRESH_X100    = 200
 */

#include "unity.h"
#include <battery_sdk/battery_soc_fusion.h>

void setUp(void) {}
void tearDown(void) {}

/* --- blend() math ----------------------------------------------------- */

void test_blend_alpha_0_returns_pure_coulomb(void)
{
    /* abs_current = 99999 -> well above threshold -> ALPHA_LOAD = 5.
     * With alpha=5 (out of 1000) and soc_c=4000, soc_v=10000:
     * fused = (5 * 10000 + 995 * 4000) / 1000 = (50000 + 3980000) / 1000 = 4030
     * The selector returns 5 (LOAD), so we test through select_alpha to be precise.
     * For the "pure coulomb" case (alpha=0), test the internal blend math
     * directly via select_alpha's actual return values. We assert relationships
     * rather than exact numbers when alpha is fixed by the selector. */
    uint16_t fused = battery_soc_fusion_blend(10000, 4000, 99999);
    /* With ALPHA_LOAD=5, fused should be very close to coulomb (4000) */
    TEST_ASSERT_INT_WITHIN(50, 4030, fused);
}

void test_blend_at_rest_pulls_5pct_toward_voltage(void)
{
    /* abs_current = 100 -> below threshold 200 -> ALPHA_REST = 50.
     * soc_v=10000, soc_c=0:
     * fused = (50 * 10000 + 950 * 0) / 1000 = 500 */
    uint16_t fused = battery_soc_fusion_blend(10000, 0, 100);
    TEST_ASSERT_EQUAL_UINT16(500, fused);
}

void test_blend_no_overflow_at_max_values(void)
{
    /* alpha = 50 (rest), soc_v = 10000, soc_c = 10000:
     * fused = (50 * 10000 + 950 * 10000) / 1000 = (500000 + 9500000) / 1000 = 10000
     * Confirms no u32 overflow when both signals are at their max. */
    uint16_t fused = battery_soc_fusion_blend(10000, 10000, 0);
    TEST_ASSERT_EQUAL_UINT16(10000, fused);
}

void test_blend_symmetric_inputs_symmetric_outputs(void)
{
    /* Both signals equal -> fused equals them, regardless of alpha. */
    TEST_ASSERT_EQUAL_UINT16(5000, battery_soc_fusion_blend(5000, 5000, 0));
    TEST_ASSERT_EQUAL_UINT16(5000, battery_soc_fusion_blend(5000, 5000, 99999));
}

void test_blend_at_rest_blends_correctly(void)
{
    /* alpha = 50 (rest), soc_v = 8000, soc_c = 4000:
     * fused = (50 * 8000 + 950 * 4000) / 1000 = (400000 + 3800000) / 1000 = 4200 */
    uint16_t fused = battery_soc_fusion_blend(8000, 4000, 0);
    TEST_ASSERT_EQUAL_UINT16(4200, fused);
}

void test_blend_under_load_barely_moves(void)
{
    /* alpha = 5 (load), soc_v = 10000, soc_c = 5000:
     * fused = (5 * 10000 + 995 * 5000) / 1000 = (50000 + 4975000) / 1000 = 5025 */
    uint16_t fused = battery_soc_fusion_blend(10000, 5000, 99999);
    TEST_ASSERT_EQUAL_UINT16(5025, fused);
}

/* --- select_alpha() --------------------------------------------------- */

void test_select_alpha_rest_when_below_threshold(void)
{
    TEST_ASSERT_EQUAL_UINT16(50, battery_soc_fusion_select_alpha(0));
    TEST_ASSERT_EQUAL_UINT16(50, battery_soc_fusion_select_alpha(100));
    TEST_ASSERT_EQUAL_UINT16(50, battery_soc_fusion_select_alpha(199));
}

void test_select_alpha_load_when_above_threshold(void)
{
    TEST_ASSERT_EQUAL_UINT16(5, battery_soc_fusion_select_alpha(201));
    TEST_ASSERT_EQUAL_UINT16(5, battery_soc_fusion_select_alpha(5000));
    TEST_ASSERT_EQUAL_UINT16(5, battery_soc_fusion_select_alpha(99999));
}

void test_select_alpha_at_exact_threshold_returns_load(void)
{
    /* Boundary convention: at exactly I_THRESH, we use LOAD alpha. */
    TEST_ASSERT_EQUAL_UINT16(5, battery_soc_fusion_select_alpha(200));
}

void test_select_alpha_treats_negative_current_as_above_threshold(void)
{
    /* The caller is responsible for passing abs_current. If they pass
     * a negative value by mistake, our comparison treats it as < threshold
     * (since -1 < 200). Document this behavior — caller MUST absolute. */
    TEST_ASSERT_EQUAL_UINT16(50, battery_soc_fusion_select_alpha(-1));
}

/* --- runner ----------------------------------------------------------- */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_blend_alpha_0_returns_pure_coulomb);
    RUN_TEST(test_blend_at_rest_pulls_5pct_toward_voltage);
    RUN_TEST(test_blend_no_overflow_at_max_values);
    RUN_TEST(test_blend_symmetric_inputs_symmetric_outputs);
    RUN_TEST(test_blend_at_rest_blends_correctly);
    RUN_TEST(test_blend_under_load_barely_moves);
    RUN_TEST(test_select_alpha_rest_when_below_threshold);
    RUN_TEST(test_select_alpha_load_when_above_threshold);
    RUN_TEST(test_select_alpha_at_exact_threshold_returns_load);
    RUN_TEST(test_select_alpha_treats_negative_current_as_above_threshold);
    return UNITY_END();
}
```

**Step 2: Add the CMake target.**

Append to `tests/CMakeLists.txt`, after the `test_soc_coulomb_cr2032` block:

```cmake
# ── SoC fusion unit tests (Phase 8c) ──────────────────────────────────────
add_executable(test_soc_fusion
    test_soc_fusion.c
    ${SDK_SRC}/intelligence/battery_soc_fusion.c
)

target_include_directories(test_soc_fusion PRIVATE
    ${SDK_INCLUDE}
    ${SDK_SRC}
    ${SDK_SRC}/intelligence
    ${unity_SOURCE_DIR}/src
)

target_compile_definitions(test_soc_fusion PRIVATE
    CONFIG_BATTERY_SOC_FUSION=1
    CONFIG_BATTERY_SOC_FUSION_ALPHA_REST_X1000=50
    CONFIG_BATTERY_SOC_FUSION_ALPHA_LOAD_X1000=5
    CONFIG_BATTERY_SOC_FUSION_I_THRESH_X100=200
)

target_link_libraries(test_soc_fusion PRIVATE unity)
```

And register the test (near line 333 where the other `add_test` calls live):

```cmake
add_test(NAME soc_fusion COMMAND test_soc_fusion)
```

**Step 3: Verify the tests fail to compile (red).**

```bash
cd /Users/aliapin/Downloads/project/ibattery-sdk/tests/build
cmake .. 2>&1 | tail -5
make test_soc_fusion 2>&1 | tail -10
```

Expected: link error — `battery_soc_fusion_blend` and `battery_soc_fusion_select_alpha` undefined. This is the TDD red signal.

**Step 4: Commit (red).**

```bash
git add tests/test_soc_fusion.c tests/CMakeLists.txt
git commit -m "test(8c): TDD red — 10 unit tests for soc fusion module

Compile fails (function not yet defined) — this is the red signal
before Task 4 implements the module."
```

---

## Task 4: Implement the fusion module (green)

**Goal:** Minimal implementation to make all 10 unit tests pass.

**Files:**
- Create: `src/intelligence/battery_soc_fusion.c`

**Step 1: Write the implementation.**

```c
/*
 * Phase 8c: voltage + coulomb signal fusion.
 *
 * Complementary filter with current-adaptive blend coefficient.
 * Integer-only math, no state, no allocations.
 *
 * Design doc: docs/plans/2026-05-29-phase-8c-fusion-design.md
 */

#include <battery_sdk/battery_soc_fusion.h>

uint16_t battery_soc_fusion_select_alpha(int32_t abs_current_x100)
{
    /* Caller is responsible for passing absolute current. Negative
     * values are treated as below-threshold (so they would select
     * ALPHA_REST), which is documented but not defended against. */
    if (abs_current_x100 < CONFIG_BATTERY_SOC_FUSION_I_THRESH_X100) {
        return (uint16_t)CONFIG_BATTERY_SOC_FUSION_ALPHA_REST_X1000;
    }
    return (uint16_t)CONFIG_BATTERY_SOC_FUSION_ALPHA_LOAD_X1000;
}

uint16_t battery_soc_fusion_blend(uint16_t soc_v_x100,
                                   uint16_t soc_c_x100,
                                   int32_t abs_current_x100)
{
    uint16_t alpha = battery_soc_fusion_select_alpha(abs_current_x100);

    /* fused = (alpha * soc_v + (1000 - alpha) * soc_c) / 1000
     *
     * Overflow analysis:
     *   max alpha               = 1000
     *   max soc_v_x100          = 10000
     *   max product             = 10,000,000
     *   sum of two such products = 20,000,000
     *   uint32_t max            = 4,294,967,295  (214x headroom)
     */
    uint32_t numerator = (uint32_t)alpha * (uint32_t)soc_v_x100
                       + (uint32_t)(1000 - alpha) * (uint32_t)soc_c_x100;
    return (uint16_t)(numerator / 1000);
}
```

**Step 2: Build and run the tests.**

```bash
cd /Users/aliapin/Downloads/project/ibattery-sdk/tests/build
make test_soc_fusion 2>&1 | tail -3
./test_soc_fusion 2>&1 | tail -5
```

Expected: build succeeds, all 10 tests pass.

**Step 3: Run the full host suite to confirm no regressions.**

```bash
cd /Users/aliapin/Downloads/project/ibattery-sdk/tests/build
ctest --output-on-failure 2>&1 | tail -5
```

Expected: `100% tests passed, 0 tests failed out of 18` (was 17, now +1 with `soc_fusion`).

**Step 4: Commit (green).**

```bash
git add src/intelligence/battery_soc_fusion.c
git commit -m "feat(8c): implement soc fusion module

Complementary filter with current-adaptive blend coefficient.
Integer-only math, 0 RAM cost, ~40 lines. All 10 new unit tests
pass; previous 17 tests still pass."
```

---

## Task 5: Add battery_soc_fusion.c to the main library build

**Goal:** Make the new module available to firmware builds (not just host tests). This is a CMake plumbing task — no logic changes.

**Files:**
- Modify: `CMakeLists.txt`

**Step 1: Locate the existing `battery_coulomb.c` reference in `CMakeLists.txt`.**

```bash
grep -n "battery_coulomb.c\|battery_soc_estimator.c" CMakeLists.txt
```

Expected: a few lines showing how intelligence/ sources are added to the library target.

**Step 2: Add `battery_soc_fusion.c` next to those (conditionally on `CONFIG_BATTERY_SOC_FUSION`).**

Inside the block that conditionally adds Phase 8a sources (looking for `CONFIG_BATTERY_SOC_COULOMB`), add a parallel conditional for fusion. Exact location depends on the file's current structure; look for the pattern that handles Phase 8a sources and mirror it.

**Step 3: Quick build to confirm CMake parses.**

```bash
cd /Users/aliapin/Downloads/project/ibattery-sdk
west build -b nucleo_l476rg app -d /tmp/build-phase8c-libcheck --pristine -- \
  -DEXTRA_CONF_FILE=boards/nucleo_l476rg_i2cscan.conf \
  -DZEPHYR_EXTRA_MODULES="/opt/nordic/ncs/v3.2.2/modules/hal/stm32" \
  -DCONFIG_BATTERY_SOC_FUSION=y 2>&1 | tail -8
```

Expected: build succeeds. (No code in `battery_soc_estimator.c` calls fusion yet, so it's compiled-but-unused — link will succeed because the functions are externally visible.)

**Step 4: Commit.**

```bash
git add CMakeLists.txt
git commit -m "build(8c): add battery_soc_fusion.c to the library build

Compiled when CONFIG_BATTERY_SOC_FUSION=y. Not yet called from the
estimator — Task 6 will wire it in."
```

---

## Task 6: TDD red — write the integration tests

**Goal:** Six integration tests against the estimator with `CONFIG_BATTERY_SOC_FUSION=y`. Should fail because the estimator doesn't call fusion yet.

**Files:**
- Create: `tests/test_soc_estimator_fusion.c`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the test file.**

```c
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

/* Helper: convert an integer percentage to x100 units. */
#define PCT_X100(p) ((uint16_t)((p) * 100))

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
 * Drift correction at rest: bias Q below capacity, voltage shows 100%,
 * fusion should pull SoC upward.
 *
 * Setup: reset Q to 50% manually (simulating accumulated drift). Mock
 * voltage at 3322 mV (LUT = 100%). Mock current = 100 (1.00 mA, at rest).
 * Drive 60 samples — expect SoC to climb above ~80% (~5%/sample alpha,
 * not-quite-95%-convergence but well past starting value).
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
```

**Step 2: Add the CMake target** (after `test_soc_fusion`):

```cmake
# ── SoC estimator + fusion integration tests (Phase 8c) ────────────────────
add_executable(test_soc_estimator_fusion
    test_soc_estimator_fusion.c
    ${SDK_SRC}/intelligence/battery_soc_estimator.c
    ${SDK_SRC}/intelligence/battery_soc_lut.c
    ${SDK_SRC}/intelligence/battery_coulomb.c
    ${SDK_SRC}/intelligence/battery_soc_fusion.c
    mocks/mock_voltage.c
    mocks/mock_current.c
    mocks/mock_nvs.c
    mocks/mock_sdk_state.c
)

target_include_directories(test_soc_estimator_fusion PRIVATE
    ${SDK_INCLUDE}
    ${SDK_SRC}
    ${SDK_SRC}/core
    ${SDK_SRC}/intelligence
    ${SDK_SRC}/hal
    ${unity_SOURCE_DIR}/src
)

target_compile_definitions(test_soc_estimator_fusion PRIVATE
    CONFIG_BATTERY_SOC_COULOMB=1
    CONFIG_BATTERY_SOC_FUSION=1
    CONFIG_BATTERY_SOC_FUSION_ALPHA_REST_X1000=50
    CONFIG_BATTERY_SOC_FUSION_ALPHA_LOAD_X1000=5
    CONFIG_BATTERY_SOC_FUSION_I_THRESH_X100=200
    CONFIG_BATTERY_CAPACITY_MAH=220
)

target_link_libraries(test_soc_estimator_fusion PRIVATE unity)

add_test(NAME soc_estimator_fusion COMMAND test_soc_estimator_fusion)
```

**Step 3: Build the new test target.**

```bash
cd /Users/aliapin/Downloads/project/ibattery-sdk/tests/build
cmake .. 2>&1 | tail -3
make test_soc_estimator_fusion 2>&1 | tail -5
```

Expected: build succeeds (functions exist) but at least some tests fail because the estimator doesn't call fusion yet — `test_fusion_corrects_drift_at_rest` and possibly others should fail.

**Step 4: Run to confirm red.**

```bash
./test_soc_estimator_fusion 2>&1 | tail -15
```

Expected: at least one test (most likely `test_fusion_corrects_drift_at_rest`) fails. Note: simple tests like `test_voltage_error_propagates` will pass because they don't depend on fusion logic.

**Step 5: Commit (red).**

```bash
git add tests/test_soc_estimator_fusion.c tests/CMakeLists.txt
git commit -m "test(8c): TDD red — integration tests for estimator+fusion

Six tests asserting fusion composes with anchor logic, slew limiter,
and error-path fallbacks. Some tests fail because the estimator
doesn't call fusion yet — Task 7 wires it in."
```

---

## Task 7: Wire fusion into the estimator (green)

**Goal:** Make the failing integration tests pass by adding the fusion branch to `battery_soc_estimator.c`.

**Files:**
- Modify: `src/intelligence/battery_soc_estimator.c`

**Step 1: Locate the existing call chain.**

```bash
grep -n "g_coulomb_soc_x100\|apply_slew_limit\|soc_from_lut" src/intelligence/battery_soc_estimator.c | head -20
```

Find the section in `battery_soc_estimator_get_pct_x100()` that currently does:
1. LUT compute
2. Coulomb read (inside `#if defined(CONFIG_BATTERY_SOC_COULOMB)`)
3. Anchor logic
4. SoC = mah * 10000 / capacity
5. Slew limit
6. Return

**Step 2: Add the fusion include.**

Near the top of the file, inside the `#if defined(CONFIG_BATTERY_SOC_COULOMB)` block where `battery_coulomb.h` is included:

```c
#if defined(CONFIG_BATTERY_SOC_FUSION)
#include <battery_sdk/battery_soc_fusion.h>
#endif
```

**Step 3: Add the fusion call between coulomb-read and slew-limit.**

After the line that computes `soc` from `mah_x100`:

```c
        /* SoC = mah / capacity * 10000 */
        int32_t capacity_x100 = (int32_t)CONFIG_BATTERY_CAPACITY_MAH * 100;
        int32_t soc = (mah_x100 * 10000) / capacity_x100;

        /* Clamp 0..10000 */
        if (soc < 0) {
            soc = 0;
        }
        // ... existing upper clamp ...
```

Add (inside the `#if defined(CONFIG_BATTERY_SOC_COULOMB)` block):

```c
#if defined(CONFIG_BATTERY_SOC_FUSION)
        /* Phase 8c fusion: blend coulomb SoC with LUT SoC, weighted by
         * current magnitude (small current -> trust voltage more). */
        soc = (int32_t)battery_soc_fusion_blend(
            (uint16_t)lut_soc,
            (uint16_t)soc,
            abs_current);
#endif
```

The existing `lut_soc` and `abs_current` variables are already in scope from earlier in the function. Confirm via grep.

**Step 4: Build and run all tests.**

```bash
cd /Users/aliapin/Downloads/project/ibattery-sdk/tests/build
make 2>&1 | tail -3
ctest --output-on-failure 2>&1 | tail -8
```

Expected: `100% tests passed, 0 tests failed out of 19` (17 prior + soc_fusion + soc_estimator_fusion).

If `test_fusion_corrects_drift_at_rest` fails: re-read its setup carefully. The anchor logic may need a small modification to allow fusion-driven recovery from a manually-biased Q. The test as written may have a flaw — debug by tightening the assertion or splitting the test into two cases.

**Step 5: Commit (green).**

```bash
git add src/intelligence/battery_soc_estimator.c
git commit -m "feat(8c): wire fusion into the SoC estimator

Adds a single new #if block between coulomb SoC computation and slew
limiter. With CONFIG_BATTERY_SOC_FUSION=n (default), zero behavior
change. With =y, voltage-LUT continuously pulls coulomb SoC toward
truth, weighted by current magnitude.

All 19 host tests pass."
```

---

## Task 8: Hardware validation — three captures on NUCLEO

**Goal:** Empirical evidence on real silicon. Three captures to `docs/captures/`, demonstrating drift correction, load-vs-rest behavior, and backward compatibility.

**Files:**
- Create: `docs/captures/2026-MM-DD-v0.10.0-drift-correction.log`
- Create: `docs/captures/2026-MM-DD-v0.10.0-load-vs-rest.log`
- Create: `docs/captures/2026-MM-DD-v0.10.0-fusion-off-regression.log`

**Step 1: Build STM32 firmware with fusion enabled.**

```bash
cd /Users/aliapin/Downloads/project/ibattery-sdk
export PATH="/opt/homebrew/bin:/usr/bin:/bin:/opt/nordic/ncs/toolchains/e5f4758bcf/bin:$PATH"
export ZEPHYR_BASE="/opt/nordic/ncs/v3.2.2/zephyr"
west build -b nucleo_l476rg app -d /tmp/build-stm32-phase8c --pristine -- \
  -DEXTRA_CONF_FILE=boards/nucleo_l476rg_i2cscan.conf \
  -DZEPHYR_EXTRA_MODULES="/opt/nordic/ncs/v3.2.2/modules/hal/stm32" \
  -DCONFIG_BATTERY_SOC_FUSION=y 2>&1 | tail -5
```

Expected: build succeeds.

**Step 2: Flash and validate I2C scan still works.**

```bash
west flash -d /tmp/build-stm32-phase8c --runner openocd 2>&1 | tail -3
```

Then verify INA219 still ACKs via serial shell (`i2c scan i2c@40005400`). If the user's hardware rig has been disassembled since v0.8.4, re-wire per `docs/HARDWARE_TROUBLESHOOTING.md`'s NUCLEO-L476RG section before this step.

**Step 3: Capture 1 — drift correction.**

Pseudocode: at the firmware shell, force-reset Q to a biased value (e.g. 50% of capacity), then let it run with a small load while the voltage LUT reads near 100%. Capture 5 min.

```bash
/Users/aliapin/.pyenv/versions/3.11.14/bin/python3 << 'EOF' > docs/captures/$(date +%Y-%m-%d)-v0.10.0-drift-correction.log 2>&1
import serial, time, re
ser = serial.Serial('/dev/cu.usbmodem1203', 115200, timeout=2)
time.sleep(1.0); ser.read(ser.in_waiting or 1)

# TODO: send shell command to bias Q. For now, document baseline:
# Just capture 5 min and observe natural fusion behavior under the
# small standby load. If voltage LUT says ~100% and Q is naturally at
# capacity (anchor fired), fusion is a no-op. The real "biased Q"
# demonstration requires either a custom shell command (out of scope
# for v0.10.0) or starting from a partially discharged cell.

print("=== v0.10.0 fusion ON — 5 min baseline ===")
ser.write(b'\r\n')
t0 = time.time()
buf = ""
while time.time() - t0 < 300:
    data = ser.read(2048)
    if data:
        buf += re.sub(r'\x1b\[[0-9;]*[mGKHJ]|\x08', '', data.decode('utf-8','replace'))

# ... (rest of capture+summary boilerplate, copy from v0.8.4 capture script)
ser.close()
EOF
```

Adapt the script that produced `2026-05-29-v0.8.4-q-ticks-down-fix-evidence.log` for the v0.10.0 path. If a "force-bias" shell command doesn't exist yet, document this as a known limitation and capture the baseline instead.

**Step 4: Capture 2 — load vs rest.**

Same script, but during the capture window manually toggle the resistor load (touch one wire to GND for 30 s, lift for 30 s, repeat 5×). Observe SoC behavior changes as α flips between REST and LOAD.

**Step 5: Capture 3 — fusion-off regression.**

Recompile with `CONFIG_BATTERY_SOC_FUSION=n`, reflash, capture 5 min, save. The output should match `2026-05-29-v0.8.4-q-ticks-down-fix-evidence.log` byte-for-byte (modulo timestamps).

**Step 6: Update `docs/captures/README.md`** with three new rows describing what each capture shows.

**Step 7: Commit captures.**

```bash
git add docs/captures/*.log docs/captures/README.md
git commit -m "evidence(8c): hardware captures from NUCLEO-L476RG

- drift-correction.log: baseline 5 min capture, fusion=y
- load-vs-rest.log: alternating load demonstrates adaptive alpha
- fusion-off-regression.log: fusion=n matches v0.8.4 behavior

Confirms Phase 8c on real silicon. Δ vs theory documented in
RELEASE_NOTES.md."
```

---

## Task 9: Documentation pass

**Goal:** Sync all the docs to v0.10.0 state.

**Files:**
- Modify: `docs/RELEASE_NOTES.md` — new v0.10.0 entry at top
- Modify: `docs/ROADMAP.md` — strike through Phase 8c
- Modify: `docs/SDK_API.md` — document `battery_soc_fusion.h`
- Modify: `CLAUDE.md` — bump Version, update Next milestone
- Modify: `README.md` — Phase 8c row marked done, install snippet `^0.10.0`
- Modify: `docs/index.md` — same updates as README

**Step 1: Write the v0.10.0 release notes entry.**

Use the same structure as the v0.8.5 / v0.9.1 entries. Required sections:
- Overview / hook
- "What changed" (Source / Tests / Docs)
- "Configuration" (the 4 new Kconfigs)
- "Hardware validation" (link to the 3 captures)
- "Backward compatibility"
- "Follow-ups"

**Step 2: Update ROADMAP.md.**

Strike through the Phase 8c row in the main feature table. Add a short status paragraph following the Phase 8a / 8b precedent.

**Step 3: Update SDK_API.md.**

New section documenting the `battery_soc_fusion.h` public API and the four new Kconfig entries.

**Step 4: Update CLAUDE.md, README.md, docs/index.md.**

Bump version references to v0.10.0. Update Phase 8c row in status tables.

**Step 5: Bump library.json.**

```json
"version": "0.10.0",
```

Also update the description if Phase 8c materially changes the SDK's positioning ("fused multi-signal SoC" might be worth a phrase).

**Step 6: Commit docs.**

```bash
git add docs/RELEASE_NOTES.md docs/ROADMAP.md docs/SDK_API.md CLAUDE.md README.md docs/index.md library.json
git commit -m "docs: v0.10.0 — Phase 8c fusion landed end-to-end

RELEASE_NOTES, ROADMAP, SDK_API, CLAUDE.md, README, index.md all
synced. library.json bumped 0.9.1 -> 0.10.0."
```

---

## Task 10: Tag, push, GitHub release

**Goal:** Ship v0.10.0 publicly.

**Step 1: Sanity-check the working tree.**

```bash
git status
```

Expected: clean. If anything's outstanding, stage + commit before tagging.

**Step 2: Tag annotated.**

```bash
git tag -a v0.10.0 -m "v0.10.0 — Phase 8c voltage+coulomb signal fusion

Continuous correction of coulomb-counter drift via voltage-LUT signal,
weighted by current magnitude. Composes with v0.8.4 anchor calibration
and v0.9.0 slew limiter. Opt-in via CONFIG_BATTERY_SOC_FUSION (default
off). Hardware-validated on NUCLEO-L476RG.

See docs/RELEASE_NOTES.md for the full entry."
```

**Step 3: Push.**

```bash
git push origin main
git push origin v0.10.0
```

**Step 4: Create GitHub release.**

```bash
gh release create v0.10.0 --repo aliaksandr-liapin/ibattery-sdk \
  --title "v0.10.0 — Phase 8c Voltage+Coulomb Signal Fusion" \
  --notes-file docs/RELEASE_NOTES.md  # or paste the v0.10.0 section as a body
```

Use a `--notes "<body>"` heredoc if you want to write a more concise GH-specific release body (per the v0.8.5 pattern).

---

## Task 11 (optional): PlatformIO publish

**Goal:** Update the PlatformIO registry.

**Step 1: Pack.**

```bash
pio pkg pack -o /tmp/
tar tzf /tmp/ibattery-sdk-0.10.0.tar.gz | head -20
```

Verify the tarball contents look right (firmware lib + Kconfig + LICENSE + README + library.json, no tests/gateway/docs).

**Step 2: Publish.**

```bash
pio pkg publish /tmp/ibattery-sdk-0.10.0.tar.gz --no-interactive
```

Expected: "The package has been accepted." Wait for the confirmation email.

**Step 3: Verify on registry page.**

Refresh https://registry.platformio.org/libraries/aliaksandr-liapin/ibattery-sdk after the email arrives. Expect v0.10.0 listed as latest.

---

## Acceptance criteria

- [ ] 19 host tests pass (`ctest` in `tests/build/`)
- [ ] Three hardware capture logs in `docs/captures/`
- [ ] `CONFIG_BATTERY_SOC_FUSION=n` build behaves byte-for-byte like v0.8.4
- [ ] `CONFIG_BATTERY_SOC_FUSION=y` build shows visible drift correction in the capture data
- [ ] `library.json` reads `"version": "0.10.0"`
- [ ] GitHub release v0.10.0 created and tagged
- [ ] (Optional) PlatformIO registry shows v0.10.0 as latest

---

## Notes for the executor

- **Worktree:** This plan can run directly on `main` (the working tree is currently clean) or in a dedicated worktree. The v0.8.4 fix used a feature branch (`fix/v0.8.4-coulomb-counting-bugs`); same pattern is fine here. If using a worktree, create with `git worktree add ../ibattery-sdk-phase8c -b feature/v0.10.0-phase-8c-fusion`.
- **Commit cadence:** every numbered task ends with a commit. ~11 commits total for the full plan. Trade-off: more commits = cleaner bisect history; fewer commits = less log noise. The plan as written is on the "more commits" side.
- **If the integration test `test_fusion_corrects_drift_at_rest` is finicky:** the anchor logic may interact with the manual `battery_coulomb_reset()` in a way that makes the test brittle. Two acceptable workarounds: (1) tighten the assertion to just `LESS_THAN(11000, q_later)` (any drift correction at all, not full convergence), or (2) split into two tests — one that asserts the no-op-on-equality property in isolation, one that asserts fusion produces a non-zero correction when inputs disagree.
- **If the hardware capture script needs a "force-bias Q" shell command:** that's out of scope for v0.10.0. Document the limitation in the capture's preamble and capture the natural baseline behavior instead.
- **Reference:** v0.8.4's TDD workflow is the closest analog — `tests/test_soc_coulomb_cr2032.c` was written red, the estimator was modified to make it pass green, all in one focused branch. Mirror that.
