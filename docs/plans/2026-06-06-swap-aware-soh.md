# Swap-aware SoH auto-reset — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** On boot, detect a battery swapped while powered off (an upward SoC jump vs a persisted baseline), auto-reset learned SoH to rated, and raise a `status_flags` bit. Opt-in; primary-cell only this phase; no wire-format change.

**Architecture:** New intelligence-layer module `battery_swap` persists the last SoC to NVS (throttled), and on the first SoC after boot compares it to that baseline; a jump ≥ threshold ⇒ swap ⇒ `battery_soh_reset()` + set a flag. The SoC estimator drives it; telemetry ORs the flag into `status_flags`. All wiring is `#ifdef CONFIG_BATTERY_SWAP_DETECT`.

**Tech Stack:** C (Zephyr SDK), Unity host tests, Kconfig, existing `battery_hal_nvs_*` u32 layer, `mock_nvs.c`.

**Design doc:** `docs/plans/2026-06-06-swap-aware-soh-design.md`

**Build/test prelude (host):**
```bash
export PATH="/opt/homebrew/bin:/usr/bin:/bin:/opt/nordic/ncs/toolchains/e5f4758bcf/bin:$PATH"
cd tests && rm -rf build && mkdir build && cd build && cmake .. && make
```

---

### Task 1: Add the NVS key and status-flag bit

**Files:**
- Modify: `src/hal/battery_hal_nvs.h` (after `BATTERY_NVS_KEY_SOH_RATED 4`)
- Modify: `include/battery_sdk/battery_types.h` (after `BATTERY_TELEMETRY_FLAG_COULOMB_ERR`)

**Step 1:** In `battery_hal_nvs.h`, add:
```c
#define BATTERY_NVS_KEY_LAST_SOC     5
```

**Step 2:** In `battery_types.h`, after the `(1U << 6)` line and before the comment block end, add:
```c
/* Event flag (not an error): set for the session when a battery swap was
 * detected on boot and the learned SoH was auto-reset. */
#define BATTERY_TELEMETRY_FLAG_BATTERY_SWAPPED (1U << 7)
```
Also widen the leading comment so it no longer claims *every* bit is an error (e.g. "A set error bit indicates the corresponding reading failed; event bits (≥ bit 7) flag notable events.").

**Step 3: Commit**
```bash
git add src/hal/battery_hal_nvs.h include/battery_sdk/battery_types.h
git commit -m "feat(swap): reserve NVS key + status flag for swap-aware SoH"
```

---

### Task 2: `battery_swap` module (TDD)

**Files:**
- Create: `include/battery_sdk/battery_swap.h`
- Create: `src/intelligence/battery_swap.c`
- Create: `tests/test_battery_swap.c`

**Step 1: Write the header** `include/battery_sdk/battery_swap.h`
```c
/*
 * Swap-aware SoH (Phase 1: primary cells, power-off swap).
 *
 * Persists the last SoC to NVS (throttled). On the first SoC after boot,
 * a sufficiently large upward jump vs that baseline is taken as a battery
 * swap: learned SoH is reset to rated and a flag is raised. Primary-cell
 * only (a rechargeable charged while off would look identical) — gate via
 * CONFIG_BATTERY_SWAP_DETECT (default y only for CR2032).
 *
 * Integer-only, no heap. NVS is best-effort; RAM is authoritative.
 * Design: docs/plans/2026-06-06-swap-aware-soh-design.md
 */
#ifndef BATTERY_SDK_BATTERY_SWAP_H
#define BATTERY_SDK_BATTERY_SWAP_H
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

/** Load the persisted SoC baseline; clear the boot-check + swap state. */
void battery_swap_init(void);

/** Feed the current SoC (0.01% units). First call after init runs the boot
 *  swap check (reset SoH + raise the flag on a detected swap); later calls
 *  persist the baseline, throttled to ≥ PERSIST_DELTA drops. */
void battery_swap_update(uint16_t soc_pct_x100);

/** True if a swap was detected this session. */
bool battery_swap_detected(void);

#ifdef __cplusplus
}
#endif
#endif /* BATTERY_SDK_BATTERY_SWAP_H */
```

**Step 2: Write the failing tests** `tests/test_battery_swap.c`
```c
#include "unity.h"
#include "battery_sdk/battery_swap.h"
#include "battery_sdk/battery_soh.h"
#include "battery_sdk/battery_status.h"

/* mock_nvs.c controls (declared as in test_soh_persistence.c) */
void     mock_nvs_reset(void);
void     mock_nvs_set_stored_value_key(uint16_t key, uint32_t v);
bool     mock_nvs_get_value_key(uint16_t key, uint32_t *out);

#define KEY_LAST_SOC 5

void setUp(void)   { mock_nvs_reset(); battery_soh_init(22000); } /* rated 220 mAh */
void tearDown(void){}

/* First boot ever: no baseline -> no swap, baseline stamped to boot SoC. */
void test_first_boot_no_swap_stamps_baseline(void){
    battery_swap_init();
    battery_swap_update(10000);              /* 100.00% */
    TEST_ASSERT_FALSE(battery_swap_detected());
    uint32_t v=0; TEST_ASSERT_TRUE(mock_nvs_get_value_key(KEY_LAST_SOC,&v));
    TEST_ASSERT_EQUAL_UINT32(10000, v);
}

/* Boot reads a much fuller cell than last persisted -> swap + SoH reset. */
void test_boot_jump_detects_swap_and_resets_soh(void){
    /* Fade SoH first so the reset is observable. */
    battery_soh_note_full_anchor();
    battery_soh_observe_empty_anchor(5000);  /* measured = 22000-5000 -> learned < rated */
    uint16_t faded=0; battery_soh_get_pct_x100(&faded);
    TEST_ASSERT_TRUE(faded < 10000);

    mock_nvs_set_stored_value_key(KEY_LAST_SOC, 500);  /* old cell left at 5% */
    battery_swap_init();
    battery_swap_update(10000);              /* fresh cell 100% -> +95% >= 25% */
    TEST_ASSERT_TRUE(battery_swap_detected());
    uint16_t soh=0; battery_soh_get_pct_x100(&soh);
    TEST_ASSERT_EQUAL_UINT16(10000, soh);    /* reset to 100% */
    uint32_t v=0; mock_nvs_get_value_key(KEY_LAST_SOC,&v);
    TEST_ASSERT_EQUAL_UINT32(10000, v);      /* baseline re-stamped */
}

/* A rise below threshold is not a swap. */
void test_boot_small_rise_no_swap(void){
    mock_nvs_set_stored_value_key(KEY_LAST_SOC, 8000);
    battery_swap_init();
    battery_swap_update(9000);               /* +10% < 25% */
    TEST_ASSERT_FALSE(battery_swap_detected());
}

/* A normal power-cycle (same cell, SoC steady/lower) is not a swap. */
void test_boot_lower_soc_no_swap(void){
    mock_nvs_set_stored_value_key(KEY_LAST_SOC, 8000);
    battery_swap_init();
    battery_swap_update(7000);
    TEST_ASSERT_FALSE(battery_swap_detected());
}

/* Baseline persists only when SoC drops by >= PERSIST_DELTA (5%). */
void test_persist_throttled_on_drop(void){
    battery_swap_init();
    battery_swap_update(10000);              /* boot stamps 10000 */
    battery_swap_update(9800);               /* -2% < 5% -> no write */
    uint32_t v=0; mock_nvs_get_value_key(KEY_LAST_SOC,&v);
    TEST_ASSERT_EQUAL_UINT32(10000, v);
    battery_swap_update(9400);               /* -6% >= 5% -> write */
    mock_nvs_get_value_key(KEY_LAST_SOC,&v);
    TEST_ASSERT_EQUAL_UINT32(9400, v);
}

int main(void){
    UNITY_BEGIN();
    RUN_TEST(test_first_boot_no_swap_stamps_baseline);
    RUN_TEST(test_boot_jump_detects_swap_and_resets_soh);
    RUN_TEST(test_boot_small_rise_no_swap);
    RUN_TEST(test_boot_lower_soc_no_swap);
    RUN_TEST(test_persist_throttled_on_drop);
    return UNITY_END();
}
```

**Step 3:** Add the test target in `tests/CMakeLists.txt` (mirror the `test_soh_persistence` target): a `test_battery_swap` executable linking `test_battery_swap.c`, `../src/intelligence/battery_swap.c`, `../src/intelligence/battery_soh.c`, `mocks/mock_nvs.c`, and Unity; register it with `add_test`.

**Step 4: Run — verify it fails to build/link** (module not implemented)
```bash
cd tests/build && cmake .. && make test_battery_swap
```
Expected: FAIL — `battery_swap_init/update/detected` undefined.

**Step 5: Write the implementation** `src/intelligence/battery_swap.c`
```c
#include <battery_sdk/battery_swap.h>
#include <battery_sdk/battery_soh.h>
#include <battery_sdk/battery_status.h>
#include "../hal/battery_hal_nvs.h"
#include <stdint.h>
#include <stdbool.h>

#ifndef CONFIG_BATTERY_SWAP_SOC_THRESHOLD_PCT_X100
#define CONFIG_BATTERY_SWAP_SOC_THRESHOLD_PCT_X100 2500   /* 25.00% */
#endif
#ifndef CONFIG_BATTERY_SWAP_PERSIST_DELTA_PCT_X100
#define CONFIG_BATTERY_SWAP_PERSIST_DELTA_PCT_X100 500    /*  5.00% */
#endif

static int32_t g_baseline_x100;
static bool    g_have_baseline;
static bool    g_boot_checked;
static bool    g_swapped;

static void persist(int32_t soc_x100)
{
    g_baseline_x100 = soc_x100;
    g_have_baseline = true;
    (void)battery_hal_nvs_write_u32(BATTERY_NVS_KEY_LAST_SOC, (uint32_t)soc_x100);
}

void battery_swap_init(void)
{
    uint32_t stored = 0;
    g_boot_checked = false;
    g_swapped = false;
    if (battery_hal_nvs_read_u32(BATTERY_NVS_KEY_LAST_SOC, &stored) == BATTERY_STATUS_OK) {
        g_baseline_x100 = (int32_t)stored;
        g_have_baseline = true;
    } else {
        g_baseline_x100 = 0;
        g_have_baseline = false;
    }
}

void battery_swap_update(uint16_t soc_pct_x100)
{
    int32_t soc = (int32_t)soc_pct_x100;

    if (!g_boot_checked) {
        g_boot_checked = true;
        if (g_have_baseline &&
            (soc - g_baseline_x100) >= CONFIG_BATTERY_SWAP_SOC_THRESHOLD_PCT_X100) {
            (void)battery_soh_reset();
            g_swapped = true;
        }
        persist(soc);                 /* (re)stamp baseline to the booted cell */
        return;
    }

    if ((g_baseline_x100 - soc) >= CONFIG_BATTERY_SWAP_PERSIST_DELTA_PCT_X100) {
        persist(soc);
    }
}

bool battery_swap_detected(void) { return g_swapped; }
```

**Step 6: Run — verify all pass**
```bash
cd tests/build && make test_battery_swap && ./test_battery_swap
```
Expected: `5 Tests 0 Failures`.

**Step 7: Commit**
```bash
git add include/battery_sdk/battery_swap.h src/intelligence/battery_swap.c tests/test_battery_swap.c tests/CMakeLists.txt
git commit -m "feat(swap): battery_swap module — boot swap detection + SoH reset (TDD)"
```

---

### Task 3: Kconfig options

**Files:** Modify `app/Kconfig.battery` (after the `BATTERY_SOC_SOH` block).

**Step 1:** Add:
```kconfig
config BATTERY_SWAP_DETECT
    bool "Auto-detect a power-off battery swap and reset learned SoH"
    depends on BATTERY_SOC_SOH
    default y if BATTERY_CHEMISTRY_CR2032
    default n
    help
      On boot, if SoC jumped up vs the last persisted value, treat it as a
      swapped cell: reset learned SoH to rated and set the BATTERY_SWAPPED
      status flag. Primary-cell only — a rechargeable charged while powered
      off would look identical, so leave this OFF for LiPo until charge-vs-swap
      disambiguation (Phase 2) lands.

config BATTERY_SWAP_SOC_THRESHOLD_PCT_X100
    int "Swap detection: upward SoC jump that counts as a swap (0.01%)"
    default 2500
    range 100 10000
    depends on BATTERY_SWAP_DETECT

config BATTERY_SWAP_PERSIST_DELTA_PCT_X100
    int "Swap detection: SoC drop that triggers a baseline persist (0.01%)"
    default 500
    range 100 10000
    depends on BATTERY_SWAP_DETECT
```

**Step 2: Commit**
```bash
git add app/Kconfig.battery
git commit -m "feat(swap): Kconfig — BATTERY_SWAP_DETECT (default on for CR2032) + thresholds"
```

---

### Task 4: Build wiring (CMake)

**Files:** `CMakeLists.txt` (root), `app/CMakeLists.txt`.

**Step 1:** Root `CMakeLists.txt` — gate the new source like `battery_soh.c` is gated:
```cmake
if(CONFIG_BATTERY_SWAP_DETECT)
  list(APPEND ... src/intelligence/battery_swap.c)
endif()
```
(Match the existing idiom around line 84 / the SOC_SOH block.)

**Step 2:** `app/CMakeLists.txt` — mirror the `battery_soh.c` block (~line 56):
```cmake
if(CONFIG_BATTERY_SWAP_DETECT)
  target_sources(app PRIVATE ../src/intelligence/battery_swap.c)
endif()
```

**Step 3:** Verify the drift guard still passes:
```bash
python3 scripts/check_build_sync.py
```
Expected: `✓ app-path and module-path build/config are in sync.`

**Step 4: Commit**
```bash
git add CMakeLists.txt app/CMakeLists.txt
git commit -m "build(swap): compile battery_swap.c when CONFIG_BATTERY_SWAP_DETECT"
```

---

### Task 5: Wire into estimator + telemetry (guarded)

**Files:** `src/intelligence/battery_soc_estimator.c`, `src/telemetry/battery_telemetry.c`.

**Step 1:** In `battery_soc_estimator.c`, add `#include <battery_sdk/battery_swap.h>`. Next to `battery_soh_init(...)` (~line 141):
```c
#ifdef CONFIG_BATTERY_SWAP_DETECT
    battery_swap_init();
#endif
```
After the per-cycle SoC is finalized (the value returned to the caller; near the SoH anchor logic ~lines 231–248), add:
```c
#ifdef CONFIG_BATTERY_SWAP_DETECT
    battery_swap_update((uint16_t)soc_pct_x100);   /* use the final SoC variable */
#endif
```

**Step 2:** In `battery_telemetry.c`, add `#include <battery_sdk/battery_swap.h>`. After `status_flags` is assembled (near the `soh_pct_x100` fetch ~line 125):
```c
#ifdef CONFIG_BATTERY_SWAP_DETECT
    if (battery_swap_detected()) {
        packet->status_flags |= BATTERY_TELEMETRY_FLAG_BATTERY_SWAPPED;
    }
#endif
```

**Step 3:** Run the full host suite (unchanged behavior when SWAP_DETECT is off in host build):
```bash
cd tests/build && cmake .. && make && ctest --output-on-failure
```
Expected: all suites pass (now incl. `battery_swap`).

**Step 4: Commit**
```bash
git add src/intelligence/battery_soc_estimator.c src/telemetry/battery_telemetry.c
git commit -m "feat(swap): drive battery_swap from estimator; flag in telemetry status_flags"
```

---

### Task 6: Firmware build verification (CR2032 default = swap on)

**Step 1:** Build the NUCLEO extADC+BLE+SoH image (SoH on ⇒ SWAP_DETECT defaults on for CR2032):
```bash
export PATH="/opt/homebrew/bin:/usr/bin:/bin:/opt/nordic/ncs/toolchains/e5f4758bcf/bin:$PATH"
export ZEPHYR_BASE="/opt/nordic/ncs/v3.2.2/zephyr"
export ZEPHYR_SDK_INSTALL_DIR="/opt/nordic/ncs/toolchains/e5f4758bcf/opt/zephyr-sdk"
cd <repo>
west build -b nucleo_l476rg app -d build-swap --pristine -- \
  -DSHIELD=x_nucleo_idb05a1 \
  -DEXTRA_CONF_FILE=boards/nucleo_l476rg_ble_extadc.conf \
  -DEXTRA_DTC_OVERLAY_FILE=boards/nucleo_l476rg_extadc.overlay \
  -DZEPHYR_EXTRA_MODULES="/opt/nordic/ncs/v3.2.2/modules/hal/stm32"
grep -E "CONFIG_BATTERY_SWAP_DETECT=" build-swap/zephyr/.config
```
Expected: build succeeds; `CONFIG_BATTERY_SWAP_DETECT=y`.

**Step 2:** Confirm a LiPo build leaves it OFF:
```bash
west build -b nucleo_l476rg app -d build-swap-lipo --pristine -- \
  -DCONFIG_BATTERY_CHEMISTRY_LIPO=y -DCONFIG_BATTERY_SOC_SOH=y \
  -DZEPHYR_EXTRA_MODULES="/opt/nordic/ncs/v3.2.2/modules/hal/stm32"
grep -E "BATTERY_SWAP_DETECT" build-swap-lipo/zephyr/.config   # expect: not set
```

**Step 3:** No commit (verification only).

---

### Task 7: Docs

**Files:** `docs/USE_CASES.md`, `CLAUDE.md`, `docs/plans/2026-06-06-swap-aware-soh-design.md`.

**Step 1:** `USE_CASES.md` — update the "Battery swap → you must call `battery_soh_reset()`" section: with `CONFIG_BATTERY_SWAP_DETECT` (default on for CR2032) the SDK now auto-detects a power-off swap and resets SoH + raises `BATTERY_TELEMETRY_FLAG_BATTERY_SWAPPED`; the manual call remains for LiPo / hot-swap / sub-threshold cases. Note the limitations (similarly-depleted used cell, power-off path only, primary only).

**Step 2:** `CLAUDE.md` — add `CONFIG_BATTERY_SWAP_DETECT` to the Kconfig table; add the new status flag to a "Known Quirks"/wire note (no wire bump); bump the host test count (+1 suite).

**Step 3:** Flip the design doc `Status:` to "implemented".

**Step 4: Commit**
```bash
git add docs/USE_CASES.md CLAUDE.md docs/plans/2026-06-06-swap-aware-soh-design.md
git commit -m "docs(swap): document auto swap-detect (primary), Kconfig, status flag"
```

---

### Task 8 (optional): Gateway — surface the swap flag

**Files:** `gateway/gateway/decoder.py`, `gateway/tests/test_decoder.py`.

**Step 1 (TDD):** Add a test: a packet with bit 7 set in `status_flags` decodes to `battery_swapped == True`.
**Step 2:** In the decoder, expose `"battery_swapped": bool(status_flags & (1 << 7))`.
**Step 3:** Run `cd gateway && ./.venv/bin/python -m pytest -q` (expect +1).
**Step 4:** Commit `feat(gateway): expose battery_swapped flag from status_flags`.

(Grafana surfacing of the flag is a later, separate change.)

---

## Verification before PR
- `cd tests/build && ctest` — all suites green (incl. `battery_swap`).
- `cd gateway && ./.venv/bin/python -m pytest -q` — green.
- `python3 scripts/check_build_sync.py` — in sync.
- `west build ... build-swap` — `CONFIG_BATTERY_SWAP_DETECT=y`; LiPo build leaves it unset.

## Hardware E2E (after merge, on the bench)
Flash `build-swap` with a depleted/low CR2032 in the holder, let it persist a low baseline, power off, insert a fresh CR2032, power on → serial/Grafana should show SoH reset to 100% and the `BATTERY_SWAPPED` flag set on that boot. (You have CR2032s + holders.)
