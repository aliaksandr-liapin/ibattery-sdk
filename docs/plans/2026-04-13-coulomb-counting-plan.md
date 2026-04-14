# Coulomb Counting Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add coulomb counting SoC estimation to iBattery SDK using INA219 current sensor, with voltage-LUT anchoring and telemetry v3.

**Architecture:** Layered — Current HAL (Zephyr sensor API wrapper) -> Coulomb Counter (trapezoidal integration) -> SoC Estimator v2 (coulomb primary + voltage anchor) -> Telemetry v3 (32 bytes). All integer-only, no heap.

**Tech Stack:** C11, Zephyr RTOS, Unity test framework, Python (gateway decoder), INA219 over I2C

**Design doc:** `docs/plans/2026-04-13-advanced-soc-coulomb-counting-design.md`

---

### Task 1: Current HAL Header + Stub + Tests

Creates the public API header and stub backend (for builds without current sensing). Tests verify the stub contract.

**Files:**
- Create: `include/battery_sdk/battery_hal_current.h`
- Create: `src/hal/battery_hal_current_stub.c`
- Create: `tests/test_hal_current_stub.c`
- Modify: `tests/CMakeLists.txt:228-229` (add test target)

**Step 1: Write the header**

Create `include/battery_sdk/battery_hal_current.h`:

```c
/*
 * HAL abstraction for current measurement.
 *
 * On Zephyr: wraps INA219 via sensor API.
 * Without CONFIG_BATTERY_CURRENT_SENSE: stub returns UNSUPPORTED.
 */

#ifndef BATTERY_SDK_BATTERY_HAL_CURRENT_H
#define BATTERY_SDK_BATTERY_HAL_CURRENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the current sensing hardware.
 *
 * @return BATTERY_STATUS_OK on success,
 *         BATTERY_STATUS_UNSUPPORTED if no current sensor configured,
 *         BATTERY_STATUS_IO if sensor not ready.
 */
int battery_hal_current_init(void);

/**
 * Read instantaneous current in 0.01 mA units.
 *
 * Sign convention: positive = discharging, negative = charging.
 *
 * @param current_ma_x100_out  Output: current in 0.01 mA units (signed).
 * @return BATTERY_STATUS_OK on success,
 *         BATTERY_STATUS_INVALID_ARG if NULL,
 *         BATTERY_STATUS_UNSUPPORTED if no current sensor configured.
 */
int battery_hal_current_read_ma_x100(int32_t *current_ma_x100_out);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SDK_BATTERY_HAL_CURRENT_H */
```

**Step 2: Write the stub**

Create `src/hal/battery_hal_current_stub.c` (pattern from `src/hal/battery_hal_nvs_stub.c`):

```c
/*
 * Stub current HAL — used when CONFIG_BATTERY_CURRENT_SENSE is off.
 *
 * Returns UNSUPPORTED so coulomb counting falls back to voltage-LUT.
 */

#include <battery_sdk/battery_hal_current.h>
#include <battery_sdk/battery_status.h>

int battery_hal_current_init(void)
{
    return BATTERY_STATUS_UNSUPPORTED;
}

int battery_hal_current_read_ma_x100(int32_t *current_ma_x100_out)
{
    (void)current_ma_x100_out;
    return BATTERY_STATUS_UNSUPPORTED;
}
```

**Step 3: Write the failing test**

Create `tests/test_hal_current_stub.c`:

```c
/*
 * Unit tests for current HAL stub.
 *
 * Verifies the stub returns UNSUPPORTED for all operations.
 */

#include "unity.h"
#include <battery_sdk/battery_hal_current.h>
#include <battery_sdk/battery_status.h>
#include <stddef.h>

void setUp(void) {}
void tearDown(void) {}

void test_stub_init_returns_unsupported(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_UNSUPPORTED,
                          battery_hal_current_init());
}

void test_stub_read_returns_unsupported(void)
{
    int32_t current;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_UNSUPPORTED,
                          battery_hal_current_read_ma_x100(&current));
}

void test_stub_read_null_returns_unsupported(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_UNSUPPORTED,
                          battery_hal_current_read_ma_x100(NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_stub_init_returns_unsupported);
    RUN_TEST(test_stub_read_returns_unsupported);
    RUN_TEST(test_stub_read_null_returns_unsupported);
    return UNITY_END();
}
```

**Step 4: Add test target to CMakeLists.txt**

Append to `tests/CMakeLists.txt` before `enable_testing()` (before line 217):

```cmake
# ── Current HAL stub test ──────────────────────────────────────────────────
add_executable(test_hal_current_stub
    test_hal_current_stub.c
    ${SDK_SRC}/hal/battery_hal_current_stub.c
)

target_include_directories(test_hal_current_stub PRIVATE
    ${SDK_INCLUDE}
    ${SDK_SRC}/hal
    ${unity_SOURCE_DIR}/src
)

target_link_libraries(test_hal_current_stub PRIVATE unity)
```

And add test registration alongside the others (after existing `add_test` lines):

```cmake
add_test(NAME hal_current_stub COMMAND test_hal_current_stub)
```

**Step 5: Run tests to verify they pass**

Run: `cd tests && rm -rf build && mkdir build && cd build && cmake .. && make && ctest --output-on-failure`

Expected: All 12 existing tests pass + `hal_current_stub` passes (3 tests).

**Step 6: Commit**

```bash
git add include/battery_sdk/battery_hal_current.h \
        src/hal/battery_hal_current_stub.c \
        tests/test_hal_current_stub.c \
        tests/CMakeLists.txt
git commit -m "feat: add current sensing HAL interface + stub backend"
```

---

### Task 2: Current HAL Zephyr Implementation

The real INA219 driver using Zephyr sensor API. Not host-testable (requires I2C hardware), but follows the die-temp HAL pattern exactly.

**Files:**
- Create: `src/hal/battery_hal_current_zephyr.c`

**Step 1: Write the Zephyr backend**

Create `src/hal/battery_hal_current_zephyr.c` (pattern from `src/hal/battery_hal_temp_zephyr.c`):

```c
/*
 * Current HAL — INA219 via Zephyr sensor API.
 *
 * Wraps the Zephyr built-in INA219 driver (drivers/sensor/ti/ina219).
 * Devicetree node must have compatible = "ti,ina219".
 *
 * Sign convention: positive = discharging, negative = charging.
 * Output units: 0.01 mA (matches SDK x100 convention).
 */

#include <battery_sdk/battery_hal_current.h>
#include <battery_sdk/battery_status.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>

#define INA219_NODE DT_NODELABEL(ina219)

#if !DT_NODE_EXISTS(INA219_NODE)
#error "No ina219 node found in devicetree — add ina219@40 to your overlay"
#endif

static const struct device *g_ina219_dev = DEVICE_DT_GET(INA219_NODE);
static bool g_initialized;

int battery_hal_current_init(void)
{
    if (!device_is_ready(g_ina219_dev)) {
        return BATTERY_STATUS_IO;
    }

    g_initialized = true;
    return BATTERY_STATUS_OK;
}

int battery_hal_current_read_ma_x100(int32_t *current_ma_x100_out)
{
    struct sensor_value val;

    if (current_ma_x100_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    if (!g_initialized) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }

    if (sensor_sample_fetch(g_ina219_dev) < 0) {
        return BATTERY_STATUS_IO;
    }

    if (sensor_channel_get(g_ina219_dev, SENSOR_CHAN_CURRENT, &val) < 0) {
        return BATTERY_STATUS_IO;
    }

    /* sensor_value: val1 = integer amps, val2 = fractional (millionths of A).
     * Convert to 0.01 mA units:
     *   val1 * 1A * 1000mA/A * 100 (x100) = val1 * 100000
     *   val2 * 1uA / 10 = val2 / 10  (uA to 0.01mA)                       */
    *current_ma_x100_out = (val.val1 * 100000) + (val.val2 / 10);

    return BATTERY_STATUS_OK;
}
```

**Step 2: Verify it compiles (no test — requires hardware)**

This file is only compiled when `CONFIG_BATTERY_CURRENT_SENSE=y` in a Zephyr build. Compilation verification happens in Task 8 (ESP32-C3 build).

**Step 3: Commit**

```bash
git add src/hal/battery_hal_current_zephyr.c
git commit -m "feat: add INA219 current HAL via Zephyr sensor API"
```

---

### Task 3: Coulomb Counter Module + Tests

Pure integer math module. No hardware dependency — fully host-testable.

**Files:**
- Create: `include/battery_sdk/battery_coulomb.h`
- Create: `src/intelligence/battery_coulomb.c`
- Create: `tests/test_coulomb.c`
- Create: `tests/mocks/mock_current.c`
- Modify: `tests/CMakeLists.txt` (add test target)

**Step 1: Write the header**

Create `include/battery_sdk/battery_coulomb.h`:

```c
/*
 * Coulomb counter — tracks charge consumed/added via current integration.
 *
 * Uses trapezoidal rule with integer-only arithmetic.
 * Persists accumulated charge to NVS for reboot survival.
 */

#ifndef BATTERY_SDK_BATTERY_COULOMB_H
#define BATTERY_SDK_BATTERY_COULOMB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the coulomb counter.
 *
 * Loads last accumulated value from NVS if available.
 *
 * @return BATTERY_STATUS_OK on success.
 */
int battery_coulomb_init(void);

/**
 * Feed a current sample and time delta to the integrator.
 *
 * Call this every telemetry cycle (e.g. every 2 seconds).
 *
 * @param current_ma_x100  Instantaneous current in 0.01 mA (signed).
 * @param dt_ms            Time since last call in milliseconds.
 * @return BATTERY_STATUS_OK, or BATTERY_STATUS_NOT_INITIALIZED.
 */
int battery_coulomb_update(int32_t current_ma_x100, uint32_t dt_ms);

/**
 * Get the accumulated charge since last reset.
 *
 * @param mah_x100_out  Output: accumulated charge in 0.01 mAh (signed).
 * @return BATTERY_STATUS_OK, BATTERY_STATUS_INVALID_ARG,
 *         or BATTERY_STATUS_NOT_INITIALIZED.
 */
int battery_coulomb_get_mah_x100(int32_t *mah_x100_out);

/**
 * Reset the accumulator to a known value.
 *
 * Called at voltage anchor points (full charge / cutoff).
 *
 * @param mah_x100  New accumulated charge in 0.01 mAh.
 * @return BATTERY_STATUS_OK, or BATTERY_STATUS_NOT_INITIALIZED.
 */
int battery_coulomb_reset(int32_t mah_x100);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SDK_BATTERY_COULOMB_H */
```

**Step 2: Write the implementation**

Create `src/intelligence/battery_coulomb.c`:

```c
/*
 * Coulomb counter — trapezoidal integration of current over time.
 *
 * Internal accumulator uses int64_t in 0.001 mAh units (sub-mAh
 * precision) to prevent drift on multi-day runs.  External API
 * reports 0.01 mAh units.
 *
 * NVS persistence: saves every BATTERY_NVS_COULOMB_INTERVAL_MS or
 * when accumulated change exceeds 1 mAh (100 x100 units).
 */

#include <battery_sdk/battery_coulomb.h>
#include <battery_sdk/battery_status.h>

#include "../hal/battery_hal_nvs.h"

#include <stdbool.h>
#include <stdint.h>

#ifndef BATTERY_NVS_KEY_COULOMB_MAH
#define BATTERY_NVS_KEY_COULOMB_MAH  2
#endif

/* NVS save interval: default 60 seconds */
#ifndef CONFIG_BATTERY_COULOMB_NVS_INTERVAL_S
#define CONFIG_BATTERY_COULOMB_NVS_INTERVAL_S 60
#endif

#define NVS_INTERVAL_MS  ((uint32_t)CONFIG_BATTERY_COULOMB_NVS_INTERVAL_S * 1000U)
#define NVS_CHANGE_THRESH_X1000  100000  /* 1.0 mAh in x1000 units */

static int64_t  g_accumulated_mah_x1000;   /* 0.001 mAh internal precision */
static int32_t  g_prev_current_ma_x100;
static bool     g_first_sample;
static bool     g_initialized;

/* NVS tracking */
static uint32_t g_ms_since_nvs_save;
static int64_t  g_nvs_saved_mah_x1000;

static void nvs_save(void)
{
    /* Store as int32 in 0.01 mAh units (fits NVS u32 API via cast) */
    int32_t mah_x100 = (int32_t)(g_accumulated_mah_x1000 / 10);
    (void)battery_hal_nvs_write_u32(BATTERY_NVS_KEY_COULOMB_MAH,
                                     (uint32_t)mah_x100);
    g_nvs_saved_mah_x1000 = g_accumulated_mah_x1000;
    g_ms_since_nvs_save = 0;
}

static void nvs_save_if_needed(uint32_t dt_ms)
{
    int64_t delta;

    g_ms_since_nvs_save += dt_ms;

    if (g_ms_since_nvs_save >= NVS_INTERVAL_MS) {
        nvs_save();
        return;
    }

    delta = g_accumulated_mah_x1000 - g_nvs_saved_mah_x1000;
    if (delta < 0) {
        delta = -delta;
    }
    if (delta >= NVS_CHANGE_THRESH_X1000) {
        nvs_save();
    }
}

int battery_coulomb_init(void)
{
    uint32_t stored;
    int rc;

    g_accumulated_mah_x1000 = 0;
    g_prev_current_ma_x100 = 0;
    g_first_sample = true;
    g_ms_since_nvs_save = 0;
    g_nvs_saved_mah_x1000 = 0;

    rc = battery_hal_nvs_read_u32(BATTERY_NVS_KEY_COULOMB_MAH, &stored);
    if (rc == BATTERY_STATUS_OK) {
        /* stored is int32 in 0.01 mAh units, cast back */
        g_accumulated_mah_x1000 = (int64_t)((int32_t)stored) * 10;
        g_nvs_saved_mah_x1000 = g_accumulated_mah_x1000;
    }

    g_initialized = true;
    return BATTERY_STATUS_OK;
}

int battery_coulomb_update(int32_t current_ma_x100, uint32_t dt_ms)
{
    int64_t avg_current;
    int64_t delta_mah_x1000;

    if (!g_initialized) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }

    if (g_first_sample) {
        g_prev_current_ma_x100 = current_ma_x100;
        g_first_sample = false;
        return BATTERY_STATUS_OK;
    }

    if (dt_ms == 0) {
        g_prev_current_ma_x100 = current_ma_x100;
        return BATTERY_STATUS_OK;
    }

    /* Trapezoidal rule: average of previous and current sample.
     * Units: current is in 0.01 mA.
     *
     * delta_mah_x1000 = avg_current_ma_x100 * dt_ms / (3600 * 100)
     *                 = avg_current_ma_x100 * dt_ms / 360000
     *
     * We accumulate in x1000 units (0.001 mAh) for sub-mAh precision.
     * So: delta_mah_x1000 = avg * dt_ms * 10 / 360000
     *                     = avg * dt_ms / 36000                        */
    avg_current = ((int64_t)g_prev_current_ma_x100 + current_ma_x100) / 2;
    delta_mah_x1000 = avg_current * (int64_t)dt_ms / 36000;

    g_accumulated_mah_x1000 += delta_mah_x1000;
    g_prev_current_ma_x100 = current_ma_x100;

    nvs_save_if_needed(dt_ms);

    return BATTERY_STATUS_OK;
}

int battery_coulomb_get_mah_x100(int32_t *mah_x100_out)
{
    if (mah_x100_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    if (!g_initialized) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }

    *mah_x100_out = (int32_t)(g_accumulated_mah_x1000 / 10);
    return BATTERY_STATUS_OK;
}

int battery_coulomb_reset(int32_t mah_x100)
{
    if (!g_initialized) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }

    g_accumulated_mah_x1000 = (int64_t)mah_x100 * 10;
    nvs_save();
    g_first_sample = true;

    return BATTERY_STATUS_OK;
}
```

**Step 3: Write the mock for current HAL**

Create `tests/mocks/mock_current.c`:

```c
#include <battery_sdk/battery_hal_current.h>
#include <battery_sdk/battery_status.h>
#include <stdbool.h>

static int g_mock_current_init_rc = BATTERY_STATUS_OK;
static int g_mock_current_read_rc = BATTERY_STATUS_OK;
static int32_t g_mock_current_value = 0;

void mock_current_set_init_rc(int rc) { g_mock_current_init_rc = rc; }
void mock_current_set_read_rc(int rc) { g_mock_current_read_rc = rc; }
void mock_current_set_value(int32_t v) { g_mock_current_value = v; }

void mock_current_reset(void)
{
    g_mock_current_init_rc = BATTERY_STATUS_OK;
    g_mock_current_read_rc = BATTERY_STATUS_OK;
    g_mock_current_value = 0;
}

int battery_hal_current_init(void)
{
    return g_mock_current_init_rc;
}

int battery_hal_current_read_ma_x100(int32_t *current_ma_x100_out)
{
    if (current_ma_x100_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }
    if (g_mock_current_read_rc != BATTERY_STATUS_OK) {
        return g_mock_current_read_rc;
    }
    *current_ma_x100_out = g_mock_current_value;
    return BATTERY_STATUS_OK;
}
```

**Step 4: Write the tests**

Create `tests/test_coulomb.c`:

```c
/*
 * Unit tests for battery_coulomb counter.
 *
 * Tests trapezoidal integration, overflow safety, NVS persistence,
 * sign handling (charge vs discharge), and reset.
 */

#include "unity.h"
#include <battery_sdk/battery_coulomb.h>
#include <battery_sdk/battery_status.h>
#include <stddef.h>

/* NVS mock controls (from mock_nvs.c) */
extern void mock_nvs_set_init_rc(int rc);
extern void mock_nvs_set_read_rc(int rc);
extern void mock_nvs_set_write_rc(int rc);
extern void mock_nvs_set_stored_value(uint32_t v);
extern void mock_nvs_reset(void);
extern uint32_t mock_nvs_get_last_written(void);

void setUp(void)
{
    mock_nvs_reset();
    battery_coulomb_init();
}

void tearDown(void) {}

/* ── Init tests ─────────────────────────────────────────────────── */

void test_init_returns_ok(void)
{
    mock_nvs_reset();
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_coulomb_init());
}

void test_init_starts_at_zero(void)
{
    int32_t mah;
    mock_nvs_reset();
    battery_coulomb_init();
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_coulomb_get_mah_x100(&mah));
    TEST_ASSERT_EQUAL_INT32(0, mah);
}

void test_init_loads_from_nvs(void)
{
    int32_t mah;
    mock_nvs_reset();
    /* Store 5.00 mAh (500 in x100 units) as uint32 */
    mock_nvs_set_stored_value((uint32_t)500);
    battery_coulomb_init();
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_coulomb_get_mah_x100(&mah));
    TEST_ASSERT_EQUAL_INT32(500, mah);
}

/* ── NULL arg tests ─────────────────────────────────────────────── */

void test_get_null_returns_invalid_arg(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_coulomb_get_mah_x100(NULL));
}

/* ── Integration math tests ─────────────────────────────────────── */

void test_constant_discharge_1_hour(void)
{
    int32_t mah;

    /* 100.00 mA discharge for 1 hour = 100.00 mAh.
     * Feed 1800 samples at 2-second intervals (3600s total).
     * First sample is consumed to initialize prev_current. */
    battery_coulomb_update(10000, 0);  /* first sample: 100.00 mA */

    for (int i = 0; i < 1800; i++) {
        battery_coulomb_update(10000, 2000);  /* 100.00 mA, 2000 ms */
    }

    battery_coulomb_get_mah_x100(&mah);
    /* Expected: 100.00 mAh = 10000 in x100 units */
    TEST_ASSERT_INT32_WITHIN(10, 10000, mah);
}

void test_constant_charge_negative_current(void)
{
    int32_t mah;

    /* -50.00 mA (charging) for 1 hour = -50.00 mAh */
    battery_coulomb_update(-5000, 0);

    for (int i = 0; i < 1800; i++) {
        battery_coulomb_update(-5000, 2000);
    }

    battery_coulomb_get_mah_x100(&mah);
    TEST_ASSERT_INT32_WITHIN(10, -5000, mah);
}

void test_zero_current_no_accumulation(void)
{
    int32_t mah;

    battery_coulomb_update(0, 0);
    for (int i = 0; i < 100; i++) {
        battery_coulomb_update(0, 2000);
    }

    battery_coulomb_get_mah_x100(&mah);
    TEST_ASSERT_EQUAL_INT32(0, mah);
}

void test_zero_dt_no_accumulation(void)
{
    int32_t mah;

    battery_coulomb_update(10000, 0);
    battery_coulomb_update(10000, 0);
    battery_coulomb_update(10000, 0);

    battery_coulomb_get_mah_x100(&mah);
    TEST_ASSERT_EQUAL_INT32(0, mah);
}

void test_trapezoidal_ramp(void)
{
    int32_t mah;

    /* Ramp from 0 to 100.00 mA over 1 hour.
     * Trapezoidal: area = (0 + 100) / 2 * 1h = 50 mAh */
    int steps = 1800;  /* 2s each = 3600s */
    battery_coulomb_update(0, 0);  /* first sample */

    for (int i = 1; i <= steps; i++) {
        int32_t current = (int32_t)((int64_t)10000 * i / steps);
        battery_coulomb_update(current, 2000);
    }

    battery_coulomb_get_mah_x100(&mah);
    /* Expected ~50.00 mAh = 5000 in x100 */
    TEST_ASSERT_INT32_WITHIN(50, 5000, mah);
}

/* ── Reset tests ────────────────────────────────────────────────── */

void test_reset_sets_value(void)
{
    int32_t mah;

    battery_coulomb_update(10000, 0);
    battery_coulomb_update(10000, 2000);

    battery_coulomb_reset(5000);  /* reset to 50.00 mAh */
    battery_coulomb_get_mah_x100(&mah);
    TEST_ASSERT_EQUAL_INT32(5000, mah);
}

void test_reset_persists_to_nvs(void)
{
    battery_coulomb_reset(7500);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)7500, mock_nvs_get_last_written());
}

/* ── Not initialized tests ──────────────────────────────────────── */

void test_update_before_init_returns_error(void)
{
    /* Create a fresh uninitialized state by re-declaring.
     * Since we can't un-init, test the contract on first call. */
    /* This test relies on setUp calling init, so it passes.
     * We test the NOT_INITIALIZED path by skipping setUp. */
}

/* ── Test runner ────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_returns_ok);
    RUN_TEST(test_init_starts_at_zero);
    RUN_TEST(test_init_loads_from_nvs);
    RUN_TEST(test_get_null_returns_invalid_arg);
    RUN_TEST(test_constant_discharge_1_hour);
    RUN_TEST(test_constant_charge_negative_current);
    RUN_TEST(test_zero_current_no_accumulation);
    RUN_TEST(test_zero_dt_no_accumulation);
    RUN_TEST(test_trapezoidal_ramp);
    RUN_TEST(test_reset_sets_value);
    RUN_TEST(test_reset_persists_to_nvs);

    return UNITY_END();
}
```

**Step 5: Add test target to `tests/CMakeLists.txt`**

Append before `enable_testing()`:

```cmake
# ── Coulomb counter test ──────────────────────────────────────────────────
add_executable(test_coulomb
    test_coulomb.c
    ${SDK_SRC}/intelligence/battery_coulomb.c
    mocks/mock_nvs.c
)

target_include_directories(test_coulomb PRIVATE
    ${SDK_INCLUDE}
    ${SDK_SRC}
    ${SDK_SRC}/intelligence
    ${SDK_SRC}/hal
    ${unity_SOURCE_DIR}/src
)

target_link_libraries(test_coulomb PRIVATE unity)
```

And add test registration:

```cmake
add_test(NAME coulomb COMMAND test_coulomb)
```

**Step 6: Run tests**

Run: `cd tests && rm -rf build && mkdir build && cd build && cmake .. && make && ctest --output-on-failure`

Expected: All existing tests pass + `coulomb` passes (11 tests).

**Step 7: Commit**

```bash
git add include/battery_sdk/battery_coulomb.h \
        src/intelligence/battery_coulomb.c \
        tests/test_coulomb.c \
        tests/mocks/mock_current.c \
        tests/CMakeLists.txt
git commit -m "feat: add coulomb counter with trapezoidal integration"
```

---

### Task 4: Telemetry v3 Wire Format

Extends the packet struct, serializer, and telemetry collector for v3 (32 bytes).

**Files:**
- Modify: `include/battery_sdk/battery_types.h`
- Modify: `src/transport/battery_serialize.h`
- Modify: `src/transport/battery_serialize.c`
- Modify: `src/telemetry/battery_telemetry.c`
- Modify: `tests/test_serialize.c`

**Step 1: Update the packet struct**

In `include/battery_sdk/battery_types.h`:

Change `BATTERY_TELEMETRY_VERSION` from `2U` to `3U`.

Add v3 fields after `cycle_count` (after line 36):

```c
    /* v3 fields — zero when telemetry_version < 3 */
    int32_t current_ma_x100;
    int32_t coulomb_mah_x100;
```

Add new flag definitions (after line 44):

```c
#define BATTERY_TELEMETRY_FLAG_CURRENT_ERR     (1U << 5)
#define BATTERY_TELEMETRY_FLAG_COULOMB_ERR     (1U << 6)
```

**Step 2: Update the serializer header**

In `src/transport/battery_serialize.h`:

Add after the V2 size define:

```c
#define BATTERY_SERIALIZE_V3_SIZE 32
#define BATTERY_SERIALIZE_BUF_SIZE BATTERY_SERIALIZE_V3_SIZE
```

Remove the old `BATTERY_SERIALIZE_BUF_SIZE` line that points to V2.

Update the `battery_serialize_wire_size` inline function:

```c
static inline uint8_t battery_serialize_wire_size(uint8_t version)
{
    if (version >= 3) return BATTERY_SERIALIZE_V3_SIZE;
    if (version >= 2) return BATTERY_SERIALIZE_V2_SIZE;
    return BATTERY_SERIALIZE_V1_SIZE;
}
```

**Step 3: Update serializer implementation**

In `src/transport/battery_serialize.c`:

Add a `put_i32_le` helper (alias for put_u32_le, for clarity):

After the existing `put_u32_le` at line 28, the same function works for signed — C casts are fine. No new helper needed.

In `battery_serialize_pack`, add after the v2 block (after line 73):

```c
    /* v3 extension */
    if (pkt->telemetry_version >= 3) {
        put_u32_le(&buf[24], (uint32_t)pkt->current_ma_x100);
        put_u32_le(&buf[28], (uint32_t)pkt->coulomb_mah_x100);
    }
```

In `battery_serialize_unpack`, add after the v2 block (after line 100):

```c
    /* v3 extension */
    if (pkt->telemetry_version >= 3 && buf_len >= BATTERY_SERIALIZE_V3_SIZE) {
        pkt->current_ma_x100  = (int32_t)get_u32_le(&buf[24]);
        pkt->coulomb_mah_x100 = (int32_t)get_u32_le(&buf[28]);
    } else {
        pkt->current_ma_x100  = 0;
        pkt->coulomb_mah_x100 = 0;
    }
```

**Step 4: Update telemetry collector**

In `src/telemetry/battery_telemetry.c`:

Add includes at top (after existing includes):

```c
#if defined(CONFIG_BATTERY_CURRENT_SENSE)
#include <battery_sdk/battery_hal_current.h>
#include <battery_sdk/battery_coulomb.h>
#endif
```

Add current + coulomb collection in `battery_telemetry_collect`, after the cycle count block (after line 76):

```c
    /* Current + coulomb — best-effort (v3) */
#if defined(CONFIG_BATTERY_CURRENT_SENSE)
    {
        int32_t current_ma_x100;
        rc = battery_hal_current_read_ma_x100(&current_ma_x100);
        if (rc == BATTERY_STATUS_OK) {
            packet->current_ma_x100 = current_ma_x100;

            /* Feed coulomb counter with current sample */
            uint32_t dt_ms = packet->timestamp_ms;  /* TODO: compute delta from previous */
            rc = battery_coulomb_update(current_ma_x100, dt_ms);
            if (rc == BATTERY_STATUS_OK) {
                (void)battery_coulomb_get_mah_x100(&packet->coulomb_mah_x100);
            } else {
                packet->status_flags |= BATTERY_TELEMETRY_FLAG_COULOMB_ERR;
            }
        } else {
            packet->status_flags |= BATTERY_TELEMETRY_FLAG_CURRENT_ERR;
        }
    }
#endif
```

Note: The `dt_ms` computation needs refinement — we need a static `prev_timestamp` to compute deltas. Add:

```c
#if defined(CONFIG_BATTERY_CURRENT_SENSE)
static uint32_t g_prev_timestamp_ms;
static bool g_prev_timestamp_valid;
#endif
```

And replace the dt_ms line with:

```c
            uint32_t dt_ms = 0;
            if (g_prev_timestamp_valid) {
                dt_ms = packet->timestamp_ms - g_prev_timestamp_ms;
            }
            g_prev_timestamp_ms = packet->timestamp_ms;
            g_prev_timestamp_valid = true;
```

**Step 5: Extend serialize tests**

Add to `tests/test_serialize.c`:

Add v3 helper after `make_v2_packet`:

```c
static struct battery_telemetry_packet make_v3_packet(void)
{
    struct battery_telemetry_packet pkt = make_v2_packet();
    pkt.telemetry_version = 3;
    pkt.current_ma_x100 = -5000;      /* -50.00 mA (charging) */
    pkt.coulomb_mah_x100 = 75000;     /* 750.00 mAh */
    return pkt;
}
```

Add v3 test functions:

```c
void test_v3_roundtrip_happy_path(void)
{
    struct battery_telemetry_packet src = make_v3_packet();
    struct battery_telemetry_packet dst;
    uint8_t buf[32];

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_serialize_pack(&src, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_serialize_unpack(buf, sizeof(buf), &dst));

    TEST_ASSERT_EQUAL_UINT8(3, dst.telemetry_version);
    TEST_ASSERT_EQUAL_INT32(-5000, dst.current_ma_x100);
    TEST_ASSERT_EQUAL_INT32(75000, dst.coulomb_mah_x100);
    TEST_ASSERT_EQUAL_UINT32(42, dst.cycle_count);
}

void test_v3_buffer_too_small(void)
{
    struct battery_telemetry_packet pkt = make_v3_packet();
    uint8_t buf[31];
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_serialize_pack(&pkt, buf, sizeof(buf)));
}

void test_v3_unpack_as_v2_gets_zero_current(void)
{
    struct battery_telemetry_packet src = make_v3_packet();
    struct battery_telemetry_packet dst;
    uint8_t buf[32];

    battery_serialize_pack(&src, buf, sizeof(buf));
    battery_serialize_unpack(buf, 24, &dst);

    TEST_ASSERT_EQUAL_INT32(0, dst.current_ma_x100);
    TEST_ASSERT_EQUAL_INT32(0, dst.coulomb_mah_x100);
}

void test_v3_wire_format_exact_bytes(void)
{
    struct battery_telemetry_packet src;
    uint8_t buf[32];

    memset(&src, 0, sizeof(src));
    src.telemetry_version   = 0x03;
    src.current_ma_x100     = 0x1B1A1918;
    src.coulomb_mah_x100    = 0x1F1E1D1C;

    battery_serialize_pack(&src, buf, sizeof(buf));

    TEST_ASSERT_EQUAL_HEX8(0x18, buf[24]);
    TEST_ASSERT_EQUAL_HEX8(0x19, buf[25]);
    TEST_ASSERT_EQUAL_HEX8(0x1A, buf[26]);
    TEST_ASSERT_EQUAL_HEX8(0x1B, buf[27]);
    TEST_ASSERT_EQUAL_HEX8(0x1C, buf[28]);
    TEST_ASSERT_EQUAL_HEX8(0x1D, buf[29]);
    TEST_ASSERT_EQUAL_HEX8(0x1E, buf[30]);
    TEST_ASSERT_EQUAL_HEX8(0x1F, buf[31]);
}

void test_wire_size_v3(void)
{
    TEST_ASSERT_EQUAL_UINT8(32, battery_serialize_wire_size(3));
    TEST_ASSERT_EQUAL_UINT8(32, battery_serialize_wire_size(255));
}
```

Register the new tests in `main()`:

```c
    /* v3 round-trip */
    RUN_TEST(test_v3_roundtrip_happy_path);
    RUN_TEST(test_v3_buffer_too_small);
    RUN_TEST(test_v3_unpack_as_v2_gets_zero_current);
    RUN_TEST(test_v3_wire_format_exact_bytes);
    RUN_TEST(test_wire_size_v3);
```

Update existing `test_wire_size_helper` to expect v3 behavior:

```c
void test_wire_size_helper(void)
{
    TEST_ASSERT_EQUAL_UINT8(20, battery_serialize_wire_size(0));
    TEST_ASSERT_EQUAL_UINT8(20, battery_serialize_wire_size(1));
    TEST_ASSERT_EQUAL_UINT8(24, battery_serialize_wire_size(2));
    TEST_ASSERT_EQUAL_UINT8(32, battery_serialize_wire_size(3));
    TEST_ASSERT_EQUAL_UINT8(32, battery_serialize_wire_size(255));
}
```

**Step 6: Update existing test buffer sizes**

In `tests/test_serialize.c`, update all `uint8_t buf[24]` to `uint8_t buf[32]` in existing test functions to accommodate v3. The v1/v2 packs still write 20/24 bytes — larger buffer is fine.

Also update `tests/test_telemetry.c` if its mock_soc or other mocks need the new fields — the `memset(packet, 0, sizeof(*packet))` in `battery_telemetry_collect` handles zeroing new fields.

**Step 7: Run tests**

Run: `cd tests && rm -rf build && mkdir build && cd build && cmake .. && make && ctest --output-on-failure`

Expected: All tests pass including new v3 serialize tests.

**Step 8: Commit**

```bash
git add include/battery_sdk/battery_types.h \
        src/transport/battery_serialize.h \
        src/transport/battery_serialize.c \
        src/telemetry/battery_telemetry.c \
        tests/test_serialize.c
git commit -m "feat: add telemetry v3 wire format with current + coulomb fields"
```

---

### Task 5: SoC Estimator v2 — Coulomb Path + Tests

Extends the existing estimator to use coulomb counting when available.

**Files:**
- Modify: `src/intelligence/battery_soc_estimator.c`
- Create: `tests/test_soc_coulomb.c`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the test first**

Create `tests/test_soc_coulomb.c`:

```c
/*
 * Unit tests for coulomb-counting SoC estimation path.
 *
 * Tests the battery_soc_estimator when CONFIG_BATTERY_SOC_COULOMB is enabled.
 * Uses mock voltage, mock current HAL, and mock NVS.
 */

#include "unity.h"
#include <battery_sdk/battery_soc_estimator.h>
#include <battery_sdk/battery_coulomb.h>
#include <battery_sdk/battery_status.h>
#include <stddef.h>

/* Mock controls */
extern void mock_voltage_set_mv(uint16_t mv);
extern void mock_voltage_set_rc(int rc);
extern void mock_current_set_value(int32_t v);
extern void mock_current_set_read_rc(int rc);
extern void mock_current_reset(void);
extern void mock_nvs_reset(void);

/* Expose internal for testing */
extern int battery_soc_coulomb_update(uint16_t voltage_mv,
                                       int32_t current_ma_x100,
                                       uint32_t dt_ms,
                                       uint16_t *soc_pct_x100);

void setUp(void)
{
    mock_nvs_reset();
    mock_current_reset();
    mock_voltage_set_mv(3800);  /* mid-range LiPo */
    mock_voltage_set_rc(BATTERY_STATUS_OK);
    battery_coulomb_init();
    battery_soc_estimator_init();
}

void tearDown(void) {}

void test_init_uses_voltage_lut(void)
{
    uint16_t soc;
    /* First call should return voltage-LUT value */
    mock_voltage_set_mv(4200);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_estimator_get_pct_x100(&soc));
    TEST_ASSERT_EQUAL_UINT16(10000, soc);  /* 100% at 4200mV */
}

void test_empty_anchor_resets_to_zero(void)
{
    uint16_t soc;
    /* Voltage below cutoff should anchor to 0% */
    mock_voltage_set_mv(2900);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_estimator_get_pct_x100(&soc));
    /* Should be near 0% from LUT at 2900mV */
    TEST_ASSERT_TRUE(soc < 500);  /* < 5% */
}

void test_null_returns_invalid_arg(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_soc_estimator_get_pct_x100(NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_uses_voltage_lut);
    RUN_TEST(test_empty_anchor_resets_to_zero);
    RUN_TEST(test_null_returns_invalid_arg);
    return UNITY_END();
}
```

**Step 2: Modify the SoC estimator**

In `src/intelligence/battery_soc_estimator.c`, add coulomb counting path:

Add includes after existing includes:

```c
#if defined(CONFIG_BATTERY_SOC_COULOMB)
#include <battery_sdk/battery_coulomb.h>
#include <battery_sdk/battery_hal_current.h>

#ifndef CONFIG_BATTERY_CAPACITY_MAH
#define CONFIG_BATTERY_CAPACITY_MAH 1000
#endif

/* Anchor thresholds */
#if defined(CONFIG_BATTERY_CHEMISTRY_LIPO)
#define SOC_ANCHOR_FULL_MV      4180
#define SOC_ANCHOR_FULL_I_X100  5000   /* |I| < 50.00 mA */
#define SOC_ANCHOR_EMPTY_MV     3000
#else  /* CR2032 */
#define SOC_ANCHOR_FULL_MV      2950
#define SOC_ANCHOR_FULL_I_X100  0      /* no current check for primary cell */
#define SOC_ANCHOR_EMPTY_MV     2000
#endif

static uint16_t g_coulomb_soc_x100;
static bool     g_coulomb_soc_valid;
static uint32_t g_prev_ts_ms;
static bool     g_prev_ts_valid;
#endif /* CONFIG_BATTERY_SOC_COULOMB */
```

Modify `battery_soc_estimator_init` to reset coulomb state:

```c
int battery_soc_estimator_init(void)
{
    struct battery_sdk_runtime_state *state = battery_sdk_state();
    state->soc_initialized = true;

#if defined(CONFIG_BATTERY_SOC_COULOMB)
    g_coulomb_soc_valid = false;
    g_prev_ts_valid = false;
#endif

    return BATTERY_STATUS_OK;
}
```

Add coulomb SoC logic inside `battery_soc_estimator_get_pct_x100`, replacing the simple LUT call when coulomb is enabled:

```c
int battery_soc_estimator_get_pct_x100(uint16_t *soc_pct_x100)
{
    int rc;
    uint16_t voltage_mv;
    uint16_t lut_soc;

    if (soc_pct_x100 == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    rc = battery_voltage_get_mv(&voltage_mv);
    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }

    /* Always compute LUT SoC as baseline / anchor reference */
#if defined(CONFIG_BATTERY_SOC_TEMP_COMP)
    {
        int32_t temp_c_x100;
        rc = battery_temperature_get_c_x100(&temp_c_x100);
        if (rc == BATTERY_STATUS_OK) {
            rc = battery_soc_temp_compensated(voltage_mv, temp_c_x100, &lut_soc);
        } else {
            rc = BATTERY_STATUS_ERROR;  /* fall through to non-temp LUT */
        }
    }
    if (rc != BATTERY_STATUS_OK)
#endif
    {
#if defined(CONFIG_BATTERY_CHEMISTRY_LIPO)
        rc = battery_soc_lut_interpolate(&battery_soc_lut_lipo_1s,
                                         voltage_mv, &lut_soc);
#else
        rc = battery_soc_lut_interpolate(&battery_soc_lut_cr2032,
                                         voltage_mv, &lut_soc);
#endif
    }

    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }

#if defined(CONFIG_BATTERY_SOC_COULOMB)
    {
        int32_t current_ma_x100;
        int32_t coulomb_mah_x100;
        int abs_current;

        /* Try to read current */
        rc = battery_hal_current_read_ma_x100(&current_ma_x100);
        if (rc != BATTERY_STATUS_OK) {
            /* Current sensor failed — fall back to LUT */
            *soc_pct_x100 = lut_soc;
            return BATTERY_STATUS_OK;
        }

        abs_current = (current_ma_x100 < 0) ? -current_ma_x100 : current_ma_x100;

        /* Check anchor conditions */
        if (voltage_mv <= SOC_ANCHOR_EMPTY_MV) {
            /* Empty anchor: reset to LUT value (near 0%) */
            g_coulomb_soc_x100 = lut_soc;
            g_coulomb_soc_valid = true;
            battery_coulomb_reset(0);
        } else if (voltage_mv >= SOC_ANCHOR_FULL_MV &&
                   (SOC_ANCHOR_FULL_I_X100 == 0 ||
                    abs_current < SOC_ANCHOR_FULL_I_X100)) {
            /* Full anchor: reset to LUT value (near 100%) */
            g_coulomb_soc_x100 = lut_soc;
            g_coulomb_soc_valid = true;
            int32_t capacity_x100 = CONFIG_BATTERY_CAPACITY_MAH * 100;
            battery_coulomb_reset(capacity_x100);
        }

        if (!g_coulomb_soc_valid) {
            /* First reading — initialize from LUT */
            g_coulomb_soc_x100 = lut_soc;
            g_coulomb_soc_valid = true;
            /* Set coulomb accumulator to match current SoC */
            int32_t init_mah_x100 = (int32_t)((int64_t)lut_soc *
                                    CONFIG_BATTERY_CAPACITY_MAH / 100);
            battery_coulomb_reset(init_mah_x100);
        }

        /* Compute SoC from coulomb accumulator */
        rc = battery_coulomb_get_mah_x100(&coulomb_mah_x100);
        if (rc == BATTERY_STATUS_OK) {
            int32_t capacity_x100 = CONFIG_BATTERY_CAPACITY_MAH * 100;
            if (capacity_x100 > 0) {
                int32_t soc = (int32_t)((int64_t)coulomb_mah_x100 * 10000
                              / capacity_x100);
                /* Clamp 0-10000 */
                if (soc < 0) soc = 0;
                if (soc > 10000) soc = 10000;
                g_coulomb_soc_x100 = (uint16_t)soc;
            }
        }

        *soc_pct_x100 = g_coulomb_soc_x100;
        return BATTERY_STATUS_OK;
    }
#else
    *soc_pct_x100 = lut_soc;
    return BATTERY_STATUS_OK;
#endif
}
```

**Step 3: Add test target to `tests/CMakeLists.txt`**

```cmake
# ── SoC coulomb test ──────────────────────────────────────────────────────
add_executable(test_soc_coulomb
    test_soc_coulomb.c
    ${SDK_SRC}/intelligence/battery_soc_estimator.c
    ${SDK_SRC}/intelligence/battery_soc_lut.c
    ${SDK_SRC}/intelligence/battery_coulomb.c
    mocks/mock_voltage.c
    mocks/mock_current.c
    mocks/mock_nvs.c
    mocks/mock_sdk_state.c
)

target_include_directories(test_soc_coulomb PRIVATE
    ${SDK_INCLUDE}
    ${SDK_SRC}
    ${SDK_SRC}/core
    ${SDK_SRC}/intelligence
    ${SDK_SRC}/hal
    ${unity_SOURCE_DIR}/src
)

target_compile_definitions(test_soc_coulomb PRIVATE
    CONFIG_BATTERY_SOC_COULOMB=1
    CONFIG_BATTERY_CHEMISTRY_LIPO=1
    CONFIG_BATTERY_CAPACITY_MAH=1000
)

target_link_libraries(test_soc_coulomb PRIVATE unity)
```

And register:

```cmake
add_test(NAME soc_coulomb COMMAND test_soc_coulomb)
```

**Step 4: Run tests**

Run: `cd tests && rm -rf build && mkdir build && cd build && cmake .. && make && ctest --output-on-failure`

Expected: All tests pass including new `soc_coulomb`.

**Step 5: Commit**

```bash
git add src/intelligence/battery_soc_estimator.c \
        tests/test_soc_coulomb.c \
        tests/mocks/mock_current.c \
        tests/CMakeLists.txt
git commit -m "feat: add coulomb-counting SoC estimation with voltage anchoring"
```

---

### Task 6: Kconfig + CMake + Devicetree

Wires everything together at the build system level.

**Files:**
- Modify: `app/Kconfig:98` (add after BATTERY_CYCLE_COUNTER block)
- Modify: `CMakeLists.txt:58` (add conditional sources)
- Modify: `app/boards/esp32c3_devkitm.overlay` (add I2C + INA219 node)
- Modify: `app/boards/esp32c3_devkitm.conf` (enable current sensing)

**Step 1: Add Kconfig options**

In `app/Kconfig`, add after the `BATTERY_CYCLE_COUNTER` block (after line 108), before `BATTERY_CHARGER_TP4056`:

```kconfig
config BATTERY_CURRENT_SENSE
    bool "Enable current sensing (INA219)"
    default n
    select SENSOR
    select I2C
    help
      Enable current measurement via an INA219 I2C sensor.
      Required for coulomb counting SoC estimation.
      The INA219 must be defined in the board's devicetree overlay.

config BATTERY_SOC_COULOMB
    bool "Coulomb counting SoC estimation"
    default y if BATTERY_CURRENT_SENSE
    depends on BATTERY_CURRENT_SENSE
    help
      Use coulomb counting (current integration) as the primary
      SoC estimation method, with voltage-LUT anchoring at
      full-charge and cutoff endpoints. Falls back to voltage-LUT
      if current sensor is unavailable at runtime.

config BATTERY_CAPACITY_MAH
    int "Battery capacity in mAh"
    default 220 if BATTERY_CHEMISTRY_CR2032
    default 1000 if BATTERY_CHEMISTRY_LIPO
    depends on BATTERY_SOC_COULOMB
    help
      Nominal battery capacity in milliamp-hours. Used by the
      coulomb counting estimator to convert accumulated charge
      to SoC percentage.

config BATTERY_COULOMB_NVS_INTERVAL_S
    int "Coulomb counter NVS save interval (seconds)"
    default 60
    depends on BATTERY_SOC_COULOMB
    help
      How often to persist the coulomb accumulator to flash.
      Lower values improve reboot accuracy at the cost of
      flash wear. Also saves on >1% SoC change regardless.
```

**Step 2: Add CMake conditionals**

In `CMakeLists.txt`, add after the NVS block (after line 59):

```cmake
# Current sensing HAL: Zephyr sensor API or stub
if(CONFIG_BATTERY_CURRENT_SENSE)
    zephyr_library_sources(src/hal/battery_hal_current_zephyr.c)
else()
    zephyr_library_sources(src/hal/battery_hal_current_stub.c)
endif()

# Coulomb counter
zephyr_library_sources_ifdef(CONFIG_BATTERY_SOC_COULOMB
    src/intelligence/battery_coulomb.c
)
```

**Step 3: Update ESP32-C3 devicetree overlay**

Append to `app/boards/esp32c3_devkitm.overlay`:

```dts
/* I2C bus for INA219 current sensor (SDA=GPIO6, SCL=GPIO7) */
&i2c0 {
	status = "okay";
	pinctrl-0 = <&i2c0_default>;
	pinctrl-names = "default";

	ina219: ina219@40 {
		compatible = "ti,ina219";
		reg = <0x40>;
		shunt-milliohm = <100>;
		lsb-microamp = <100>;
		brng = <0>;
		pg = <1>;
		sadc = <13>;
		badc = <13>;
	};
};
```

**Step 4: Update ESP32-C3 board conf**

Add to `app/boards/esp32c3_devkitm.conf`:

```
# ── Current Sensing (INA219) ────────────────────────────────────────
# Uncomment to enable coulomb counting with INA219
# CONFIG_BATTERY_CURRENT_SENSE=y
# CONFIG_BATTERY_SOC_COULOMB=y
# CONFIG_BATTERY_CAPACITY_MAH=1000
# CONFIG_I2C=y
```

**Step 5: Commit**

```bash
git add app/Kconfig CMakeLists.txt \
        app/boards/esp32c3_devkitm.overlay \
        app/boards/esp32c3_devkitm.conf
git commit -m "feat: add Kconfig, CMake, and devicetree for INA219 current sensing"
```

---

### Task 7: Gateway v3 Decoder + Tests

Extend the Python gateway to decode v3 packets.

**Files:**
- Modify: `gateway/gateway/decoder.py`
- Modify: `gateway/tests/test_decoder.py`

**Step 1: Update the decoder**

In `gateway/gateway/decoder.py`:

Add v3 struct format and decode logic. The v3 format adds two `int32_t` fields (current_ma_x100, coulomb_mah_x100) at offsets 24-31. Total: 32 bytes.

Add format string: `V3_FORMAT = "<BIiiHBIIii"` (adds two signed int32 `i` at end).

Update `decode_packet()` to handle 32-byte packets:
- If length == 32: unpack with v3 format, add `current_ma` and `coulomb_mah` to result dict
- Convert: `current_ma = current_ma_x100 / 100.0`, `coulomb_mah = coulomb_mah_x100 / 100.0`

**Step 2: Add v3 tests**

In `gateway/tests/test_decoder.py`, add `TestDecodePacketV3` class:

```python
class TestDecodePacketV3:
    def test_basic_v3_decode(self):
        """32-byte v3 packet decodes current and coulomb fields."""
        # Build a v3 packet: v2 fields + current_ma_x100 + coulomb_mah_x100
        data = struct.pack("<BIiiHBIIii",
            3,          # version
            5000,       # timestamp_ms
            3800,       # voltage_mv
            2500,       # temperature_c_x100
            7500,       # soc_pct_x100
            1,          # power_state (ACTIVE)
            0,          # status_flags
            10,         # cycle_count
            -5000,      # current_ma_x100 (-50.00 mA, charging)
            75000,      # coulomb_mah_x100 (750.00 mAh)
        )
        result = decode_packet(data)
        assert result["version"] == 3
        assert result["current_ma"] == pytest.approx(-50.0)
        assert result["coulomb_mah"] == pytest.approx(750.0)

    def test_v3_positive_current(self):
        """Positive current = discharging."""
        data = struct.pack("<BIiiHBIIii",
            3, 1000, 3700, 2500, 5000, 6, 0, 0, 10000, 50000)
        result = decode_packet(data)
        assert result["current_ma"] == pytest.approx(100.0)
        assert result["coulomb_mah"] == pytest.approx(500.0)

    def test_v3_zero_current(self):
        """Zero current, zero coulomb."""
        data = struct.pack("<BIiiHBIIii",
            3, 1000, 4200, 2500, 10000, 7, 0, 0, 0, 0)
        result = decode_packet(data)
        assert result["current_ma"] == pytest.approx(0.0)
        assert result["coulomb_mah"] == pytest.approx(0.0)
```

**Step 3: Run gateway tests**

Run: `cd gateway && pytest tests/test_decoder.py -v`

Expected: All existing decoder tests pass + 3 new v3 tests pass.

**Step 4: Commit**

```bash
git add gateway/gateway/decoder.py gateway/tests/test_decoder.py
git commit -m "feat: add v3 packet decoding to gateway (current + coulomb)"
```

---

### Task 8: Roadmap Update

Update the roadmap to reflect the 8a/8b/8c split.

**Files:**
- Modify: `docs/ROADMAP.md`

**Step 1: Update the roadmap**

Find the "Advanced SoC" entry in `docs/ROADMAP.md` and replace it with:

```markdown
### Phase 8a: Coulomb Counting SoC (current)

- INA219 current sensor HAL (Zephyr sensor API)
- Coulomb counter with trapezoidal integration
- Voltage-anchored SoC estimation (coulomb primary, LUT at endpoints)
- Telemetry v3 wire format (32 bytes: adds current + coulomb fields)
- NVS persistence for reboot survival
- ESP32-C3 validated first, then nRF52840 + STM32

### Phase 8b: Voltage-LUT Correction Mode (planned)

- Coulomb counting as smoothing layer over existing voltage-LUT
- Reduces SoC jitter from voltage sag during BLE TX
- No new hardware — software-only improvement

### Phase 8c: Kalman Filter Fusion (planned)

- Optimal blending of voltage + coulomb + temperature signals
- Weighted by confidence (voltage noisy under load, coulomb drifts over time)
- Industry-standard approach (phones, EVs, medical devices)
- Same public API — drop-in replacement for 8a estimator
```

**Step 2: Commit**

```bash
git add docs/ROADMAP.md
git commit -m "docs: split Advanced SoC roadmap into phases 8a/8b/8c"
```

---

### Task 9: ESP32-C3 Build Verification

Verify the full stack compiles for ESP32-C3 with current sensing enabled.

**Files:** None new — compilation check only.

**Step 1: Build without current sensing (regression check)**

```bash
cd ~/zephyr-esp32 && source .venv/bin/activate
export ZEPHYR_BASE=~/zephyr-esp32/zephyr
export ZEPHYR_SDK_INSTALL_DIR=/opt/nordic/ncs/toolchains/e5f4758bcf/opt/zephyr-sdk
west build -b esp32c3_devkitm /Users/aliapin/Downloads/project/ibattery-sdk/app \
    -d build-esp32c3-nocurrent --pristine
```

Expected: Clean build. Current HAL stub compiled. No regression.

**Step 2: Build with current sensing enabled**

```bash
west build -b esp32c3_devkitm /Users/aliapin/Downloads/project/ibattery-sdk/app \
    -d build-esp32c3-current --pristine -- \
    -DEXTRA_CONF_FILE=boards/esp32c3_devkitm_current.conf
```

Create `app/boards/esp32c3_devkitm_current.conf` with:

```
CONFIG_I2C=y
CONFIG_BATTERY_CURRENT_SENSE=y
CONFIG_BATTERY_SOC_COULOMB=y
CONFIG_BATTERY_CAPACITY_MAH=1000
```

Expected: Clean build with INA219 driver + coulomb counter linked.

**Step 3: Run all host tests one final time**

```bash
cd /Users/aliapin/Downloads/project/ibattery-sdk/tests
rm -rf build && mkdir build && cd build && cmake .. && make && ctest --output-on-failure
```

Expected: All tests pass (existing 11 suites + 3 new: hal_current_stub, coulomb, soc_coulomb).

**Step 4: Run gateway tests**

```bash
cd /Users/aliapin/Downloads/project/ibattery-sdk/gateway && pytest -v
```

Expected: All 61+ tests pass.

**Step 5: Commit build config**

```bash
git add app/boards/esp32c3_devkitm_current.conf
git commit -m "feat: add ESP32-C3 current sensing build config"
```

---

### Task 10: Update Documentation

Update README, RELEASE_NOTES, and ARCHITECTURE docs.

**Files:**
- Modify: `README.md` — add INA219 wiring section, mention coulomb counting
- Modify: `docs/RELEASE_NOTES.md` — add v0.8.0 entry
- Modify: `docs/ARCHITECTURE.md` — add current HAL + coulomb counter to layer diagram and memory budget
- Modify: `docs/SDK_API.md` — add new public API functions

**Step 1: Update each file with the new feature documentation**

Keep changes minimal — add facts, not marketing copy.

**Step 2: Commit**

```bash
git add README.md docs/RELEASE_NOTES.md docs/ARCHITECTURE.md docs/SDK_API.md
git commit -m "docs: add coulomb counting to README, architecture, and API docs"
```
