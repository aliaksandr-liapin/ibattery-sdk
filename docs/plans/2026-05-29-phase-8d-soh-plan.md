# Phase 8d State of Health (SoH) Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add an opt-in, on-device State-of-Health module that learns the battery's true usable capacity from full→empty discharge excursions and exposes SoH via a public API.

**Architecture:** New stateful module `battery_soh` (init/observe/get, like the coulomb counter). The SoC estimator arms it at the full-anchor edge and feeds it the remaining charge `Q` at the empty-anchor edge; the module computes `measured = rated − Q_before_empty`, smooths `learned_capacity` with an integer EMA, and reports `SoH = learned / rated`. RAM-only, integer-only, no heap. Opt-in via `CONFIG_BATTERY_SOC_SOH` (default n); zero impact when disabled.

**Tech Stack:** C11, Zephyr Kconfig, Unity host tests (CMake/CTest). Design doc: `docs/plans/2026-05-29-phase-8d-soh-design.md`.

**Conventions:** All charge/capacity values in `x100` units (0.01 mAh). SoH in `x100` percent (0..10000). Round-to-nearest (half away from zero) in the EMA divide — same lesson as Phase 8c.

---

### Task 1: Module skeleton — init + getters

**Files:**
- Create: `include/battery_sdk/battery_soh.h`
- Create: `src/intelligence/battery_soh.c`
- Create: `tests/test_soc_soh.c`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the header**

`include/battery_sdk/battery_soh.h`:

```c
/*
 * Phase 8d: on-device State of Health (capacity-fade learning).
 *
 * Learns the battery's true usable capacity from full->empty discharge
 * excursions: the coulomb integral between a full anchor and an empty
 * anchor is the actual delivered charge. Smooths the learned capacity
 * with an integer EMA and reports SoH = learned / rated.
 *
 * Stateful, integer-only, no heap. RAM-only (no persistence in the MVP).
 *
 * Design doc: docs/plans/2026-05-29-phase-8d-soh-design.md
 */

#ifndef BATTERY_SDK_BATTERY_SOH_H
#define BATTERY_SDK_BATTERY_SOH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize SoH tracking. learned_capacity := rated; disarmed.
 *  @param rated_mah_x100 Rated capacity in 0.01 mAh units (> 0). */
int battery_soh_init(int32_t rated_mah_x100);

/** Arm a measurable excursion: a full anchor has just fired. */
void battery_soh_note_full_anchor(void);

/** Feed the empty-anchor edge. If armed, compute measured capacity
 *  (rated - q_before_empty), EMA-update learned capacity if the value
 *  passes the plausibility guard, then disarm. No-op if not armed.
 *  @param q_before_empty_x100 Remaining charge read just before the
 *         coulomb counter is reset to 0 at the empty anchor. */
int battery_soh_observe_empty_anchor(int32_t q_before_empty_x100);

/** SoH in 0.01% units (0..10000). */
int battery_soh_get_pct_x100(uint16_t *soh_x100_out);

/** Learned usable capacity in 0.01 mAh units. */
int battery_soh_get_learned_capacity_mah_x100(int32_t *cap_x100_out);

/** Reset learned capacity to rated and disarm. */
int battery_soh_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SDK_BATTERY_SOH_H */
```

**Step 2: Write the failing test harness**

`tests/test_soc_soh.c`:

```c
/*
 * Unit tests for battery_soh (Phase 8d).
 * Kconfig values injected via tests/CMakeLists.txt:
 *   CONFIG_BATTERY_SOC_SOH_ALPHA_X1000   = 500
 *   CONFIG_BATTERY_SOC_SOH_REJECT_LO_PCT = 30
 *   CONFIG_BATTERY_SOC_SOH_REJECT_HI_PCT = 120
 */
#include "unity.h"
#include <battery_sdk/battery_soh.h>
#include <battery_sdk/battery_status.h>

#define RATED 22000  /* 220.00 mAh (CR2032), x100 */

void setUp(void) { battery_soh_init(RATED); }
void tearDown(void) {}

void test_init_reports_100pct(void)
{
    uint16_t soh;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_soh_get_pct_x100(&soh));
    TEST_ASSERT_EQUAL_UINT16(10000, soh);
}

void test_init_learned_equals_rated(void)
{
    int32_t cap;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
        battery_soh_get_learned_capacity_mah_x100(&cap));
    TEST_ASSERT_EQUAL_INT32(RATED, cap);
}

void test_null_arg_rejected(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
        battery_soh_get_pct_x100(NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_reports_100pct);
    RUN_TEST(test_init_learned_equals_rated);
    RUN_TEST(test_null_arg_rejected);
    return UNITY_END();
}
```

**Step 3: Register the test** — add to `tests/CMakeLists.txt` (after the `test_soc_fusion` block):

```cmake
# ── SoH test (Phase 8d) ───────────────────────────────────────────────────
add_executable(test_soc_soh
    test_soc_soh.c
    ${SDK_SRC}/intelligence/battery_soh.c
)
target_include_directories(test_soc_soh PRIVATE
    ${SDK_INCLUDE}
    ${SDK_SRC}
    ${SDK_SRC}/intelligence
    ${unity_SOURCE_DIR}/src
)
target_compile_definitions(test_soc_soh PRIVATE
    CONFIG_BATTERY_SOC_SOH_ALPHA_X1000=500
    CONFIG_BATTERY_SOC_SOH_REJECT_LO_PCT=30
    CONFIG_BATTERY_SOC_SOH_REJECT_HI_PCT=120
)
target_link_libraries(test_soc_soh PRIVATE unity)
```

And add to the CTest list at the bottom:

```cmake
add_test(NAME soc_soh COMMAND test_soc_soh)
```

**Step 4: Run — verify it fails to build** (`battery_soh.c` missing)

```bash
cd tests && cmake -S . -B build >/dev/null && cmake --build build --target test_soc_soh
```
Expected: link/compile error (no `battery_soh.c` symbols).

**Step 5: Write minimal implementation**

`src/intelligence/battery_soh.c`:

```c
/*
 * Phase 8d: on-device State of Health (capacity-fade learning).
 * Design doc: docs/plans/2026-05-29-phase-8d-soh-design.md
 */
#include <battery_sdk/battery_soh.h>
#include <battery_sdk/battery_status.h>
#include <stdbool.h>
#include <stddef.h>

static int32_t g_rated_x100;
static int32_t g_learned_x100;
static bool    g_armed;

int battery_soh_init(int32_t rated_mah_x100)
{
    if (rated_mah_x100 <= 0) {
        return BATTERY_STATUS_INVALID_ARG;
    }
    g_rated_x100 = rated_mah_x100;
    g_learned_x100 = rated_mah_x100;
    g_armed = false;
    return BATTERY_STATUS_OK;
}

int battery_soh_get_pct_x100(uint16_t *soh_x100_out)
{
    if (soh_x100_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }
    int32_t soh = (g_learned_x100 * 10000) / g_rated_x100;
    if (soh < 0) soh = 0;
    if (soh > 10000) soh = 10000;
    *soh_x100_out = (uint16_t)soh;
    return BATTERY_STATUS_OK;
}

int battery_soh_get_learned_capacity_mah_x100(int32_t *cap_x100_out)
{
    if (cap_x100_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }
    *cap_x100_out = g_learned_x100;
    return BATTERY_STATUS_OK;
}

int battery_soh_reset(void)
{
    g_learned_x100 = g_rated_x100;
    g_armed = false;
    return BATTERY_STATUS_OK;
}

void battery_soh_note_full_anchor(void) { g_armed = true; }

int battery_soh_observe_empty_anchor(int32_t q_before_empty_x100)
{
    (void)q_before_empty_x100;
    return BATTERY_STATUS_OK;  /* filled in Task 2 */
}
```

**Step 6: Run — verify pass**

```bash
cd tests && cmake --build build --target test_soc_soh && ./build/test_soc_soh
```
Expected: 3 Tests 0 Failures.

**Step 7: Commit**

```bash
git add include/battery_sdk/battery_soh.h src/intelligence/battery_soh.c tests/test_soc_soh.c tests/CMakeLists.txt
git commit -m "feat(8d): battery_soh skeleton — init + getters (TDD green)"
```

---

### Task 2: Excursion measurement + EMA update

**Files:** Modify `src/intelligence/battery_soh.c`, `tests/test_soc_soh.c`

**Step 1: Add failing tests** (append to `test_soc_soh.c` and to `main`):

```c
void test_healthy_excursion_keeps_100pct(void)
{
    /* Healthy: q_before_empty == 0 -> measured == rated -> learned unchanged. */
    battery_soh_note_full_anchor();
    battery_soh_observe_empty_anchor(0);
    uint16_t soh; battery_soh_get_pct_x100(&soh);
    TEST_ASSERT_EQUAL_UINT16(10000, soh);
}

void test_aged_excursion_moves_toward_measured(void)
{
    /* Aged 80%: q_before = 20% of rated = 4400 -> measured = 17600.
     * alpha=500 -> learned = 22000 + (17600-22000)*500/1000 = 19800.
     * SoH = 19800/22000*10000 = 9000. */
    battery_soh_note_full_anchor();
    battery_soh_observe_empty_anchor(4400);
    int32_t cap; battery_soh_get_learned_capacity_mah_x100(&cap);
    TEST_ASSERT_EQUAL_INT32(19800, cap);
    uint16_t soh; battery_soh_get_pct_x100(&soh);
    TEST_ASSERT_EQUAL_UINT16(9000, soh);
}

void test_unarmed_observe_is_noop(void)
{
    /* No note_full_anchor() -> observe must not change learned. */
    battery_soh_observe_empty_anchor(4400);
    int32_t cap; battery_soh_get_learned_capacity_mah_x100(&cap);
    TEST_ASSERT_EQUAL_INT32(RATED, cap);
}
```

**Step 2: Run — verify the two behavior tests fail** (observe is a stub)

```bash
cd tests && cmake --build build --target test_soc_soh && ./build/test_soc_soh
```
Expected: `test_aged_excursion_moves_toward_measured` FAILS (cap == 22000, not 19800).

**Step 3: Implement** — replace the stub `battery_soh_observe_empty_anchor`:

```c
int battery_soh_observe_empty_anchor(int32_t q_before_empty_x100)
{
    if (!g_armed) {
        return BATTERY_STATUS_OK;  /* not a full->empty excursion */
    }
    g_armed = false;  /* excursion consumed regardless of outcome */

    int32_t measured = g_rated_x100 - q_before_empty_x100;

    /* Plausibility guard: reject partial cycles / noise. */
    int32_t lo = (g_rated_x100 * CONFIG_BATTERY_SOC_SOH_REJECT_LO_PCT) / 100;
    int32_t hi = (g_rated_x100 * CONFIG_BATTERY_SOC_SOH_REJECT_HI_PCT) / 100;
    if (measured < lo || measured > hi) {
        return BATTERY_STATUS_OK;  /* rejected, learned unchanged */
    }

    /* EMA, round-to-nearest (half away from zero) to avoid one-sided bias. */
    int32_t delta = measured - g_learned_x100;
    int32_t num = delta * (int32_t)CONFIG_BATTERY_SOC_SOH_ALPHA_X1000;
    int32_t step = (num >= 0) ? (num + 500) / 1000 : (num - 500) / 1000;
    g_learned_x100 += step;
    return BATTERY_STATUS_OK;
}
```

**Step 4: Run — verify pass** (6 tests).

**Step 5: Commit**

```bash
git add src/intelligence/battery_soh.c tests/test_soc_soh.c
git commit -m "feat(8d): excursion measurement + EMA update with plausibility guard"
```

---

### Task 3: Convergence + guard + reset tests

**Files:** Modify `tests/test_soc_soh.c`

**Step 1: Add tests** (append + register in `main`):

```c
void test_repeated_aged_excursions_converge(void)
{
    /* Repeated 80% excursions -> learned converges toward 17600 (80%). */
    for (int i = 0; i < 12; i++) {
        battery_soh_note_full_anchor();
        battery_soh_observe_empty_anchor(4400);
    }
    int32_t cap; battery_soh_get_learned_capacity_mah_x100(&cap);
    TEST_ASSERT_INT32_WITHIN(50, 17600, cap);  /* ~80% */
}

void test_implausible_low_rejected(void)
{
    /* q_before = 90% rated -> measured = 10% < REJECT_LO (30%) -> rejected. */
    battery_soh_note_full_anchor();
    battery_soh_observe_empty_anchor((RATED * 90) / 100);
    int32_t cap; battery_soh_get_learned_capacity_mah_x100(&cap);
    TEST_ASSERT_EQUAL_INT32(RATED, cap);
}

void test_implausible_high_rejected(void)
{
    /* q_before negative -> measured > rated; > REJECT_HI (120%) when large. */
    battery_soh_note_full_anchor();
    battery_soh_observe_empty_anchor(-(RATED / 2));  /* measured = 1.5*rated */
    int32_t cap; battery_soh_get_learned_capacity_mah_x100(&cap);
    TEST_ASSERT_EQUAL_INT32(RATED, cap);
}

void test_reset_restores_100pct(void)
{
    battery_soh_note_full_anchor();
    battery_soh_observe_empty_anchor(4400);  /* drops below 100% */
    battery_soh_reset();
    uint16_t soh; battery_soh_get_pct_x100(&soh);
    TEST_ASSERT_EQUAL_UINT16(10000, soh);
}
```

**Step 2: Run — verify pass** (10 tests). These exercise existing code, so they should pass immediately; if any fail, fix the implementation, not the test.

**Step 3: Commit**

```bash
git add tests/test_soc_soh.c
git commit -m "test(8d): convergence, guard, and reset coverage"
```

---

### Task 4: Kconfig surface

**Files:** Modify `app/Kconfig`

**Step 1:** Add after the `BATTERY_SOC_FUSION` block (the `endif # BATTERY_SOC_FUSION`):

```kconfig
config BATTERY_SOC_SOH
    bool "State-of-Health (capacity-fade learning)"
    default n
    depends on BATTERY_SOC_COULOMB
    help
      Learns the battery's true usable capacity from full->empty discharge
      excursions and exposes State of Health (SoH = learned / rated). The
      coulomb integral between a full and an empty anchor is the actual
      delivered charge; an aged cell delivers less, which shows up as SoH
      below 100%. RAM-only (relearns on reboot). Opt-in; default off.

if BATTERY_SOC_SOH

config BATTERY_SOC_SOH_ALPHA_X1000
    int "SoH EMA blend per valid excursion (x1000)"
    default 500
    range 0 1000
    help
      Per-excursion pull of learned capacity toward the freshly measured
      value. 500 = 50% (converges in a few cycles); smaller is steadier.

config BATTERY_SOC_SOH_REJECT_LO_PCT
    int "Reject measured capacity below this % of rated"
    default 30
    range 0 100

config BATTERY_SOC_SOH_REJECT_HI_PCT
    int "Reject measured capacity above this % of rated"
    default 120
    range 100 200

endif # BATTERY_SOC_SOH
```

**Step 2: Commit**

```bash
git add app/Kconfig
git commit -m "feat(8d): Kconfig surface for SoH (opt-in + alpha + reject bounds)"
```

---

### Task 5: Zephyr build wiring

**Files:** Modify `app/CMakeLists.txt`

**Step 1:** Add after the SoC fusion block:

```cmake
# State of Health (capacity-fade learning)
if(CONFIG_BATTERY_SOC_SOH)
    target_sources(app PRIVATE ../src/intelligence/battery_soh.c)
endif()
```

**Step 2: Commit**

```bash
git add app/CMakeLists.txt
git commit -m "build(8d): add battery_soh.c to the Zephyr app build"
```

---

### Task 6: Integrate into the SoC estimator anchor logic

**Files:** Modify `src/intelligence/battery_soc_estimator.c`, `tests/CMakeLists.txt`, create `tests/test_soc_soh_estimator.c`

**Step 1: Hook the estimator.** Near the top includes, add (guarded):

```c
#if defined(CONFIG_BATTERY_SOC_SOH)
#include <battery_sdk/battery_soh.h>
#endif
```

In the full-anchor edge branch (where `g_full_anchor_active` is set true after the full reset), add:

```c
#if defined(CONFIG_BATTERY_SOC_SOH)
                battery_soh_note_full_anchor();
#endif
```

In the empty-anchor edge branch, **before** `battery_coulomb_reset(0)`, capture `Q` and feed SoH:

```c
#if defined(CONFIG_BATTERY_SOC_SOH)
                {
                    int32_t q_before = 0;
                    if (battery_coulomb_get_mah_x100(&q_before) == BATTERY_STATUS_OK) {
                        battery_soh_observe_empty_anchor(q_before);
                    }
                }
#endif
```

Also initialize SoH where the estimator initializes coulomb-related state (in `battery_soc_estimator_init`), guarded:

```c
#if defined(CONFIG_BATTERY_SOC_SOH)
    battery_soh_init((int32_t)CONFIG_BATTERY_CAPACITY_MAH * 100);
#endif
```

**Step 2: Integration test** `tests/test_soc_soh_estimator.c` — drive the estimator through a full→empty excursion with an aged profile (empty anchor reached while coulomb `Q` still positive via mock current), assert `battery_soh_get_pct_x100` drops below 10000. Model it on `tests/test_soc_coulomb_cr2032.c` (same mocks: `mock_voltage`, `mock_current`, `mock_nvs`, `mock_sdk_state`).

Register in `tests/CMakeLists.txt`:

```cmake
add_executable(test_soc_soh_estimator
    test_soc_soh_estimator.c
    ${SDK_SRC}/intelligence/battery_soc_estimator.c
    ${SDK_SRC}/intelligence/battery_soc_lut.c
    ${SDK_SRC}/intelligence/battery_coulomb.c
    ${SDK_SRC}/intelligence/battery_soh.c
    mocks/mock_voltage.c
    mocks/mock_current.c
    mocks/mock_nvs.c
    mocks/mock_sdk_state.c
)
target_include_directories(test_soc_soh_estimator PRIVATE
    ${SDK_INCLUDE} ${SDK_SRC} ${SDK_SRC}/core ${SDK_SRC}/intelligence
    ${SDK_SRC}/hal ${unity_SOURCE_DIR}/src
)
target_compile_definitions(test_soc_soh_estimator PRIVATE
    CONFIG_BATTERY_SOC_COULOMB=1
    CONFIG_BATTERY_SOC_SOH=1
    CONFIG_BATTERY_SOC_SOH_ALPHA_X1000=500
    CONFIG_BATTERY_SOC_SOH_REJECT_LO_PCT=30
    CONFIG_BATTERY_SOC_SOH_REJECT_HI_PCT=120
    CONFIG_BATTERY_CAPACITY_MAH=220
)
target_link_libraries(test_soc_soh_estimator PRIVATE unity)
```
Add `add_test(NAME soc_soh_estimator COMMAND test_soc_soh_estimator)`.

**Step 3: Run the full suite**

```bash
cd tests && cmake -S . -B build >/dev/null && cmake --build build && ctest --test-dir build --output-on-failure
```
Expected: all suites pass (21 total: was 19 + `soc_soh` + `soc_soh_estimator`).

**Step 4: Verify the estimator with SoH disabled is unchanged** — `ctest` for `soc_coulomb`, `soc_coulomb_cr2032`, `soc_estimator_fusion` still pass (no `CONFIG_BATTERY_SOC_SOH` → hooks compile out).

**Step 5: Commit**

```bash
git add src/intelligence/battery_soc_estimator.c tests/test_soc_soh_estimator.c tests/CMakeLists.txt
git commit -m "feat(8d): wire SoH into estimator anchor edges (TDD green)"
```

---

### Task 7: Firmware build sanity + docs

**Files:** Modify `docs/RELEASE_NOTES.md`, `docs/SDK_API.md`, `docs/ROADMAP.md`, `CLAUDE.md`

**Step 1: Compile-check on hardware target** (validates Kconfig + build wiring):

```bash
export PATH="/opt/homebrew/bin:/usr/bin:/bin:/opt/nordic/ncs/toolchains/e5f4758bcf/bin:$PATH"
export ZEPHYR_BASE="/opt/nordic/ncs/v3.2.2/zephyr"
export ZEPHYR_SDK_INSTALL_DIR="/opt/nordic/ncs/toolchains/e5f4758bcf/opt/zephyr-sdk"
west build -b nucleo_l476rg app -d /tmp/build-soh --pristine -- \
  -DSHIELD=x_nucleo_idb05a1 \
  -DEXTRA_CONF_FILE=boards/nucleo_l476rg_ble_current.conf \
  -DCONFIG_BATTERY_SOC_SOH=y \
  -DZEPHYR_EXTRA_MODULES="/opt/nordic/ncs/v3.2.2/modules/hal/stm32"
```
Expected: exit 0, links cleanly.

**Step 2: Document** — add a "Phase 8d (SoH)" row to the SDK_API public-API table and ROADMAP status; add an unreleased RELEASE_NOTES section describing the MVP and its limitations (slow convergence, discharge-only, RAM-only). Note SoH is opt-in and the cloud/telemetry layer is deferred.

**Step 3: Commit**

```bash
git add docs/RELEASE_NOTES.md docs/SDK_API.md docs/ROADMAP.md CLAUDE.md
git commit -m "docs(8d): document on-device SoH (opt-in), limitations, and API"
```

---

## Out of scope (deferred, per design doc)

- Wire-v4 telemetry field + gateway/InfluxDB/Grafana SoH panel (cloud layer).
- NVS persistence (the infra exists via `battery_hal_nvs`; add only when an offline-edge case needs reboot-survival).
- Partial-excursion learning (Approach 2); charge-direction learning; current-offset auto-calibration.

## Definition of done

- `ctest` green (21 suites), including the two new SoH suites.
- SoH-disabled builds byte-unchanged in behavior (existing estimator tests pass).
- NUCLEO firmware compiles with `CONFIG_BATTERY_SOC_SOH=y`.
- Docs updated; no release/tag (that's a separate decision).
