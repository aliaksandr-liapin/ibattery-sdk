# SoH NVS Persistence Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Persist the SoH-learned usable capacity to flash so it survives a power-cycle, best-effort, with zero new static RAM.

**Architecture:** `battery_soh.c` owns its persistence (mirroring `battery_cycle_counter.c`): restore on `init` behind a rated-capacity guard, write-through on each valid excursion and on `reset`. Two new NVS keys (`SOH_LEARNED`, `SOH_RATED`) over the existing `u32` HAL API. The real NVS source is compiled whenever `CYCLE_COUNTER || SOC_SOH`.

**Tech Stack:** Embedded C (C11), Zephyr NVS subsystem, Unity host test framework, CMake/CTest.

**Design doc:** `docs/plans/2026-06-01-soh-nvs-persistence-design.md`

**Conventions:** Integer-only, no heap, best-effort persistence (every NVS call `(void)`-cast; RAM value always authoritative). All NVS values are `x100` fixed-point passed through `battery_hal_nvs_{read,write}_u32`.

**How to run host tests (from repo root):**
```bash
cd tests && mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug >/dev/null && make 2>&1 | tail -5 && ctest --output-on-failure
```
To run a single suite: `ctest -R soh_persistence --output-on-failure` (or run the binary directly, e.g. `./test_soc_soh`).

---

## Task 1: Make `mock_nvs.c` key-aware (regression-safe refactor)

The current mock is single-slot (ignores `key`). SoH needs two distinct keys persisted alongside coulomb/cycle keys. Upgrade to a small key→value map while keeping every existing helper behaving identically so `test_coulomb` and `test_cycle_counter` stay green.

**Files:**
- Modify: `tests/mocks/mock_nvs.c` (full rewrite)

**Step 1: Rewrite the mock**

Replace the entire contents of `tests/mocks/mock_nvs.c` with:

```c
#include <battery_sdk/battery_status.h>
#include <stdint.h>
#include <stdbool.h>

/* ── Key-aware mock NVS ───────────────────────────────────────────────────
 * A tiny fixed key->value map. Flash survives a simulated "reboot" (the
 * stored map is only cleared by mock_nvs_reset()); RAM does not (the unit
 * under test re-runs its init()). Back-compat: mock_nvs_set_stored_value()
 * sets a wildcard fallback returned for any key with no explicit entry, so
 * the existing single-key suites keep working unchanged.
 */

#define MOCK_NVS_MAX_KEYS 8

static struct { uint16_t key; uint32_t value; bool present; }
    g_entries[MOCK_NVS_MAX_KEYS];

static int      g_init_rc       = BATTERY_STATUS_OK;
static int      g_read_rc       = BATTERY_STATUS_OK;   /* forced read error if != OK */
static int      g_write_rc      = BATTERY_STATUS_OK;
static uint32_t g_legacy_value;
static bool     g_legacy_present;
static uint32_t g_last_written;

static int find_slot(uint16_t key)
{
    for (int i = 0; i < MOCK_NVS_MAX_KEYS; i++) {
        if (g_entries[i].present && g_entries[i].key == key) return i;
    }
    return -1;
}

static int alloc_slot(uint16_t key)
{
    int i = find_slot(key);
    if (i >= 0) return i;
    for (i = 0; i < MOCK_NVS_MAX_KEYS; i++) {
        if (!g_entries[i].present) { g_entries[i].key = key; return i; }
    }
    return -1;
}

/* ── Control functions ───────────────────────────────────────────────────── */

void mock_nvs_set_init_rc(int rc)  { g_init_rc = rc; }
void mock_nvs_set_read_rc(int rc)  { g_read_rc = rc; }
void mock_nvs_set_write_rc(int rc) { g_write_rc = rc; }

/* Legacy single-value helper: wildcard fallback for any key. */
void mock_nvs_set_stored_value(uint32_t v)
{
    g_legacy_value = v;
    g_legacy_present = true;
    g_read_rc = BATTERY_STATUS_OK;
}

/* Key-aware helpers (new). */
void mock_nvs_set_stored_value_key(uint16_t key, uint32_t v)
{
    int i = alloc_slot(key);
    if (i >= 0) { g_entries[i].value = v; g_entries[i].present = true; }
}

bool mock_nvs_get_value_key(uint16_t key, uint32_t *out)
{
    int i = find_slot(key);
    if (i < 0) return false;
    if (out) *out = g_entries[i].value;
    return true;
}

uint32_t mock_nvs_get_last_written(void) { return g_last_written; }

void mock_nvs_reset(void)
{
    for (int i = 0; i < MOCK_NVS_MAX_KEYS; i++) {
        g_entries[i].present = false;
        g_entries[i].key = 0;
        g_entries[i].value = 0;
    }
    g_init_rc = BATTERY_STATUS_OK;
    g_read_rc = BATTERY_STATUS_OK;
    g_write_rc = BATTERY_STATUS_OK;
    g_legacy_value = 0;
    g_legacy_present = false;
    g_last_written = 0;
}

/* ── HAL stub implementations ────────────────────────────────────────────── */

int battery_hal_nvs_init(void) { return g_init_rc; }

int battery_hal_nvs_read_u32(uint16_t key, uint32_t *value)
{
    if (g_read_rc != BATTERY_STATUS_OK) return g_read_rc;
    int i = find_slot(key);
    if (i >= 0) { if (value) *value = g_entries[i].value; return BATTERY_STATUS_OK; }
    if (g_legacy_present) { if (value) *value = g_legacy_value; return BATTERY_STATUS_OK; }
    return BATTERY_STATUS_ERROR;  /* not found */
}

int battery_hal_nvs_write_u32(uint16_t key, uint32_t value)
{
    if (g_write_rc != BATTERY_STATUS_OK) return g_write_rc;
    int i = alloc_slot(key);
    if (i < 0) return BATTERY_STATUS_ERROR;
    g_entries[i].value = value;
    g_entries[i].present = true;
    g_last_written = value;
    return BATTERY_STATUS_OK;
}
```

> **Behaviour note:** old default `read_rc` was `ERROR` ("not found"); now it is `OK` and "not found" is signalled by an empty map + no legacy value. Existing suites always either call `mock_nvs_set_stored_value()` (sets legacy + OK) or rely on an empty map returning `ERROR` — both still hold. Verified in Step 2.

**Step 2: Run the existing NVS-dependent suites — must stay green**

Run:
```bash
cd tests/build && cmake .. >/dev/null && make test_coulomb test_cycle_counter test_soc_soh_estimator 2>&1 | tail -3 \
  && ctest -R "coulomb|cycle_counter|soc_soh_estimator" --output-on-failure
```
Expected: all PASS (regression guard for the mock rewrite).

**Step 3: Commit**

```bash
git add tests/mocks/mock_nvs.c
git commit -m "test: make mock_nvs key-aware (back-compatible)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 2: Failing test — learned capacity restores across reboot

**Files:**
- Create: `tests/test_soh_persistence.c`
- Modify: `tests/CMakeLists.txt` (new executable + `add_test`)

**Step 1: Write the failing test**

Create `tests/test_soh_persistence.c`:

```c
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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_learned_restores_across_reboot);
    return UNITY_END();
}
```

**Step 2: Wire the suite into CMake**

In `tests/CMakeLists.txt`, immediately after the `test_soc_soh` block (ends at the `target_link_libraries(test_soc_soh PRIVATE unity)` line, ~378), add:

```cmake
# ── SoH NVS persistence test ───────────────────────────────────────────────
add_executable(test_soh_persistence
    test_soh_persistence.c
    ${SDK_SRC}/intelligence/battery_soh.c
    mocks/mock_nvs.c
)
target_include_directories(test_soh_persistence PRIVATE
    ${SDK_INCLUDE} ${SDK_SRC} ${SDK_SRC}/intelligence ${SDK_SRC}/hal
    ${unity_SOURCE_DIR}/src
)
target_compile_definitions(test_soh_persistence PRIVATE
    CONFIG_BATTERY_SOC_SOH_ALPHA_X1000=500
    CONFIG_BATTERY_SOC_SOH_REJECT_LO_PCT=30
    CONFIG_BATTERY_SOC_SOH_REJECT_HI_PCT=120
    CONFIG_BATTERY_CAPACITY_MAH=220
)
target_link_libraries(test_soh_persistence PRIVATE unity)
```

And add to the `add_test` block (near line 467, after `soc_soh_estimator`):

```cmake
add_test(NAME soh_persistence COMMAND test_soh_persistence)
```

**Step 3: Run — verify it fails for the right reason**

Run:
```bash
cd tests/build && cmake .. >/dev/null && make test_soh_persistence 2>&1 | tail -5 && ./test_soh_persistence
```
Expected: builds and links (it links `mock_nvs.c`), then **FAIL** at `test_learned_restores_across_reboot` — `learned2` equals `RATED` (22000), not `learned1`, because `battery_soh_init` currently always resets to rated.

**Step 4: Commit the failing test**

```bash
git add tests/test_soh_persistence.c tests/CMakeLists.txt
git commit -m "test: SoH learned capacity should restore across reboot (failing)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 3: Add NVS keys + restore-on-init + write-on-excursion

**Files:**
- Modify: `src/hal/battery_hal_nvs.h` (add keys)
- Modify: `src/intelligence/battery_soh.c` (NVS include, restore, write)
- Modify: `tests/CMakeLists.txt` (add `mock_nvs.c` to `test_soc_soh`)

**Step 1: Add NVS keys**

In `src/hal/battery_hal_nvs.h`, after the existing key defines (line ~20):

```c
#define BATTERY_NVS_KEY_SOH_LEARNED  3
#define BATTERY_NVS_KEY_SOH_RATED    4
```

**Step 2: Add persistence to `battery_soh.c`**

Add the include near the top (after the existing includes):

```c
#include "../hal/battery_hal_nvs.h"
```

Replace `battery_soh_init` with:

```c
int battery_soh_init(int32_t rated_mah_x100)
{
    if (rated_mah_x100 <= 0) {
        return BATTERY_STATUS_INVALID_ARG;
    }
    g_rated_x100 = rated_mah_x100;
    g_learned_x100 = rated_mah_x100;   /* default: first-boot / no NVS */
    g_armed = false;
    g_initialized = true;

    /* Best-effort restore. NVS failures degrade silently to RAM-only. */
    if (battery_hal_nvs_init() == BATTERY_STATUS_OK) {
        uint32_t stored_rated = 0;
        uint32_t stored_learned = 0;
        int rc_rated = battery_hal_nvs_read_u32(BATTERY_NVS_KEY_SOH_RATED,
                                                &stored_rated);
        int rc_learn = battery_hal_nvs_read_u32(BATTERY_NVS_KEY_SOH_LEARNED,
                                                &stored_learned);
        if (rc_rated == BATTERY_STATUS_OK && rc_learn == BATTERY_STATUS_OK &&
            (int32_t)stored_rated == g_rated_x100) {
            /* Same battery profile — trust the learned value. */
            g_learned_x100 = (int32_t)stored_learned;
        } else {
            /* First boot or profile changed — stamp current rated/learned. */
            (void)battery_hal_nvs_write_u32(BATTERY_NVS_KEY_SOH_RATED,
                                            (uint32_t)g_rated_x100);
            (void)battery_hal_nvs_write_u32(BATTERY_NVS_KEY_SOH_LEARNED,
                                            (uint32_t)g_learned_x100);
        }
    }
    return BATTERY_STATUS_OK;
}
```

In `battery_soh_observe_empty_anchor`, after `g_learned_x100 += step;` and before `return BATTERY_STATUS_OK;`, add:

```c
    /* Persist the freshly learned capacity (best-effort; excursions are
     * rare so flash wear is a non-issue). */
    (void)battery_hal_nvs_write_u32(BATTERY_NVS_KEY_SOH_LEARNED,
                                    (uint32_t)g_learned_x100);
```

**Step 3: Add `mock_nvs.c` to the `test_soc_soh` target**

`battery_soh.c` now references `battery_hal_nvs_*`, so the `test_soc_soh` unit suite must link the mock. In `tests/CMakeLists.txt`, in the `add_executable(test_soc_soh ...)` block (line ~363):

```cmake
add_executable(test_soc_soh
    test_soc_soh.c
    ${SDK_SRC}/intelligence/battery_soh.c
    mocks/mock_nvs.c
)
```
and add `${SDK_SRC}/hal` to its `target_include_directories`.

**Step 4: Run the new test — must pass**

```bash
cd tests/build && cmake .. >/dev/null && make test_soh_persistence test_soc_soh 2>&1 | tail -5 \
  && ./test_soh_persistence && ./test_soc_soh
```
Expected: `test_soh_persistence` PASS; `test_soc_soh` PASS (links cleanly with the mock).

**Step 5: Full regression**

```bash
cd tests/build && make 2>&1 | tail -3 && ctest --output-on-failure
```
Expected: all suites PASS.

**Step 6: Commit**

```bash
git add src/hal/battery_hal_nvs.h src/intelligence/battery_soh.c tests/CMakeLists.txt
git commit -m "feat: persist SoH learned capacity across reboot

Restore on init behind a rated-capacity guard; write-through on each
valid excursion. Best-effort, 0 new RAM.

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 4: Rated-guard test (battery-profile change discards stale value)

**Files:**
- Modify: `tests/test_soh_persistence.c`

**Step 1: Add the failing test**

```c
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
```
Register it in `main()`: `RUN_TEST(test_profile_change_discards_stored_learned);`

**Step 2: Run**

```bash
cd tests/build && make test_soh_persistence 2>&1 | tail -3 && ./test_soh_persistence
```
Expected: PASS (the guard implemented in Task 3 already covers this — this test locks the behavior in). If it fails, fix the guard in `battery_soh_init` before proceeding.

**Step 3: Commit**

```bash
git add tests/test_soh_persistence.c
git commit -m "test: profile change discards stale SoH learned value

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 5: `reset()` persists across reboot

**Files:**
- Modify: `src/intelligence/battery_soh.c`
- Modify: `tests/test_soh_persistence.c`

**Step 1: Add the failing test**

```c
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
```
Register in `main()`.

**Step 2: Run — verify it fails**

```bash
cd tests/build && make test_soh_persistence 2>&1 | tail -3 && ./test_soh_persistence
```
Expected: FAIL at `test_reset_persists_across_reboot` — after reboot the stored (faded) `SOH_LEARNED` from the excursion is restored, because `battery_soh_reset()` does not yet write NVS.

**Step 3: Implement — write on reset**

In `battery_soh.c`, replace `battery_soh_reset` with:

```c
int battery_soh_reset(void)
{
    if (!g_initialized) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }
    g_learned_x100 = g_rated_x100;
    g_armed = false;
    (void)battery_hal_nvs_write_u32(BATTERY_NVS_KEY_SOH_LEARNED,
                                    (uint32_t)g_learned_x100);
    return BATTERY_STATUS_OK;
}
```

**Step 4: Run — must pass**

```bash
cd tests/build && make test_soh_persistence 2>&1 | tail -3 && ./test_soh_persistence
```
Expected: all tests PASS.

**Step 5: Commit**

```bash
git add src/intelligence/battery_soh.c tests/test_soh_persistence.c
git commit -m "feat: SoH reset persists to NVS

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 6: Degradation tests (read-miss & write-failure)

These lock in the best-effort contract. Both should pass against the Task 3 implementation; if either fails, the implementation is not best-effort and must be fixed.

**Files:**
- Modify: `tests/test_soh_persistence.c`

**Step 1: Add the tests**

```c
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
```
Register both in `main()`.

**Step 2: Run — must pass**

```bash
cd tests/build && make test_soh_persistence 2>&1 | tail -3 && ./test_soh_persistence
```
Expected: all PASS.

**Step 3: Commit**

```bash
git add tests/test_soh_persistence.c
git commit -m "test: SoH persistence degrades gracefully (read-miss, write-fail)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 7: Broaden the firmware NVS build gate

So the real Zephyr NVS source compiles for SoH builds, not just cycle-counter builds.

**Files:**
- Modify: `CMakeLists.txt:53-58`
- Modify: `app/CMakeLists.txt:30-35`

**Step 1: Root `CMakeLists.txt`**

Replace the NVS-selection block:

```cmake
# NVS HAL: real Zephyr NVS or stub
if(CONFIG_BATTERY_CYCLE_COUNTER OR CONFIG_BATTERY_SOC_SOH)
    zephyr_library_sources(src/hal/battery_hal_nvs_zephyr.c)
else()
    zephyr_library_sources(src/hal/battery_hal_nvs_stub.c)
endif()
```

**Step 2: `app/CMakeLists.txt`**

```cmake
# NVS HAL: real Zephyr NVS or stub
if(CONFIG_BATTERY_CYCLE_COUNTER OR CONFIG_BATTERY_SOC_SOH)
    target_sources(app PRIVATE ../src/hal/battery_hal_nvs_zephyr.c)
else()
    target_sources(app PRIVATE ../src/hal/battery_hal_nvs_stub.c)
endif()
```

**Step 3: Full host regression (build-gate change is firmware-only, but confirm nothing host broke)**

```bash
cd tests/build && cmake .. >/dev/null && make 2>&1 | tail -3 && ctest --output-on-failure
```
Expected: all suites PASS (21 + 1 new = 22 suites).

**Step 4: Firmware build sanity (no flash yet)**

With the PATH/ZEPHYR env from `CLAUDE.md` exported:
```bash
west build -b nucleo_l476rg app -d build-stm32-ble-cur --pristine -- \
  -DSHIELD=x_nucleo_idb05a1 \
  -DEXTRA_CONF_FILE=boards/nucleo_l476rg_ble_current.conf \
  -DZEPHYR_EXTRA_MODULES="/opt/nordic/ncs/v3.2.2/modules/hal/stm32" \
  -DCONFIG_BATTERY_SOC_SOH=y
```
Expected: clean build; confirm `battery_hal_nvs_zephyr.c` (not the stub) is compiled. Sanity-check the map didn't regress RAM:
```bash
grep -nE "RAM:|FLASH:" build-stm32-ble-cur/zephyr/zephyr.map | head || true
```

**Step 5: Commit**

```bash
git add CMakeLists.txt app/CMakeLists.txt
git commit -m "build: compile real NVS for SoC_SOH builds, not just cycle counter

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 8: On-device validation (NUCLEO — requires hardware power-on)

> **STOP — ask the user to power on the wired NUCLEO-L476RG (BLE shield + INA219) before this task.** Everything above is host-only. BLE streaming on macOS must run from **iTerm**, not Claude Code (per `CLAUDE.md`).

**Step 1: Flash the SoH+current build**

```bash
west flash -d build-stm32-ble-cur --runner openocd
```

**Step 2: Drive a full→empty excursion**

Anchor full (voltage ≥ full region at low current), then discharge through to the empty region so `observe_empty_anchor` fires with a faded measured capacity. Watch the serial log for the SoH value settling below 100%.

**Step 3: Power-cycle and confirm restore**

Reset/power-cycle the board. On reboot, confirm via serial (and Grafana "State of Health (%)" panel via the gateway) that SoH comes back at the learned value, **not** 100%. Also confirm SoC behaves sanely (coulomb-restore is benign — recalibrates at the first anchor edge).

**Step 4: Capture evidence**

Save the serial/gateway log to `docs/captures/2026-06-01-soh-nvs-persistence-e2e.log` and reference it in `docs/RELEASE_NOTES.md` for the next version bump.

**Step 5: Commit the capture**

```bash
git add docs/captures/2026-06-01-soh-nvs-persistence-e2e.log
git commit -m "docs: hardware capture for SoH NVS persistence E2E

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Done criteria

- [ ] 22 host suites pass (`ctest`), including `soh_persistence` (5 tests).
- [ ] `test_coulomb`, `test_cycle_counter`, `test_soc_soh`, `test_soc_soh_estimator` still green after the mock rewrite and the `battery_soh.c` NVS additions.
- [ ] Firmware builds clean with `CONFIG_BATTERY_SOC_SOH=y` and compiles the real NVS source.
- [ ] 0 new static RAM (confirm via map).
- [ ] On-device: SoH restores after power-cycle; coulomb-restore confirmed benign; capture saved.
- [ ] Post-merge bookkeeping (separate): RELEASE_NOTES, CLAUDE.md "Current State" + wire/Kconfig notes (SoH is now persistent), MEMORY project_state, version bump.
```
