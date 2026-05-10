# Phase 8b: Voltage Smoothing Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add median voltage filter (alternative to existing moving-average) and SoC slew limiter — software-only defense against load-induced voltage sag.

**Architecture:** Two independent compile-time-selectable layers. Median filter is a drop-in replacement for the moving-average via Kconfig + CMake conditional sources (same struct, same function signatures, different `.c` implementation). SoC slew limiter wraps the existing estimator output, capping rate of change with bypass on first sample and anchor events.

**Tech Stack:** C11, Zephyr RTOS, Unity test framework, integer-only math

**Design doc:** `docs/plans/2026-04-14-voltage-smoothing-design.md`

---

### Task 1: Median Filter Module + Tests

Drop-in replacement for moving-average filter. Same public API, different algorithm.

**Files:**
- Create: `src/core_modules/battery_voltage_filter_median.c`
- Create: `tests/test_voltage_filter_median.c`
- Modify: `tests/CMakeLists.txt` (add test target)

**Step 1: Write the test file**

Create `tests/test_voltage_filter_median.c`:

```c
/*
 * Unit tests for median voltage filter.
 *
 * Verifies median computation, outlier rejection, odd/even windows,
 * full circular wrap. Uses the same battery_voltage_filter_t struct
 * and API as the moving-average filter — implementation-only swap.
 */

#include "unity.h"
#include "battery_voltage_filter.h"
#include <battery_sdk/battery_status.h>

static battery_voltage_filter_t filter;

void setUp(void)
{
    battery_voltage_filter_init(&filter, 5);  /* odd window for median */
}

void tearDown(void) {}

/* ── Null-pointer checks ──────────────────────────────────────────── */

void test_init_null_filter(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_voltage_filter_init(NULL, 5));
}

void test_update_null_filter(void)
{
    uint16_t out;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_voltage_filter_update(NULL, 3000, &out));
}

void test_update_null_output(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_voltage_filter_update(&filter, 3000, NULL));
}

/* ── Median basic behavior ───────────────────────────────────────── */

void test_single_sample_returns_itself(void)
{
    uint16_t out = 0;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_voltage_filter_update(&filter, 3000, &out));
    TEST_ASSERT_EQUAL_UINT16(3000, out);
}

void test_three_samples_returns_middle(void)
{
    uint16_t out = 0;
    battery_voltage_filter_update(&filter, 3000, &out);
    battery_voltage_filter_update(&filter, 3500, &out);
    battery_voltage_filter_update(&filter, 3200, &out);
    /* sorted: 3000, 3200, 3500 — median = 3200 */
    TEST_ASSERT_EQUAL_UINT16(3200, out);
}

void test_outlier_rejected(void)
{
    uint16_t out = 0;
    /* Steady 3700, then a 3200 sag (BLE TX), back to 3700 */
    battery_voltage_filter_update(&filter, 3700, &out);
    battery_voltage_filter_update(&filter, 3700, &out);
    battery_voltage_filter_update(&filter, 3700, &out);
    battery_voltage_filter_update(&filter, 3200, &out);  /* outlier */
    battery_voltage_filter_update(&filter, 3700, &out);
    /* Sorted: 3200, 3700, 3700, 3700, 3700 — median = 3700 */
    TEST_ASSERT_EQUAL_UINT16(3700, out);
}

void test_multiple_outliers_eventually_overcome(void)
{
    uint16_t out = 0;
    /* 3 sags in a row out of 5 samples — median follows */
    battery_voltage_filter_update(&filter, 3700, &out);
    battery_voltage_filter_update(&filter, 3700, &out);
    battery_voltage_filter_update(&filter, 3200, &out);
    battery_voltage_filter_update(&filter, 3200, &out);
    battery_voltage_filter_update(&filter, 3200, &out);
    /* Sorted: 3200, 3200, 3200, 3700, 3700 — median = 3200 */
    TEST_ASSERT_EQUAL_UINT16(3200, out);
}

/* ── Even window: mean of two middle elements ────────────────────── */

void test_even_window_mean_of_middle_two(void)
{
    battery_voltage_filter_t f;
    uint16_t out = 0;
    battery_voltage_filter_init(&f, 4);

    battery_voltage_filter_update(&f, 3000, &out);
    battery_voltage_filter_update(&f, 3100, &out);
    battery_voltage_filter_update(&f, 3200, &out);
    battery_voltage_filter_update(&f, 3300, &out);
    /* Sorted: 3000, 3100, 3200, 3300 — mean of 3100, 3200 = 3150 */
    TEST_ASSERT_EQUAL_UINT16(3150, out);
}

/* ── Sorted / reverse input edge cases ───────────────────────────── */

void test_already_sorted_input(void)
{
    uint16_t out = 0;
    battery_voltage_filter_update(&filter, 3000, &out);
    battery_voltage_filter_update(&filter, 3100, &out);
    battery_voltage_filter_update(&filter, 3200, &out);
    battery_voltage_filter_update(&filter, 3300, &out);
    battery_voltage_filter_update(&filter, 3400, &out);
    TEST_ASSERT_EQUAL_UINT16(3200, out);  /* median */
}

void test_reverse_sorted_input(void)
{
    uint16_t out = 0;
    battery_voltage_filter_update(&filter, 3400, &out);
    battery_voltage_filter_update(&filter, 3300, &out);
    battery_voltage_filter_update(&filter, 3200, &out);
    battery_voltage_filter_update(&filter, 3100, &out);
    battery_voltage_filter_update(&filter, 3000, &out);
    TEST_ASSERT_EQUAL_UINT16(3200, out);  /* median */
}

/* ── Circular wrap ───────────────────────────────────────────────── */

void test_circular_wrap_after_window_full(void)
{
    uint16_t out = 0;
    /* Fill window with 3000s */
    for (int i = 0; i < 5; i++) {
        battery_voltage_filter_update(&filter, 3000, &out);
    }
    TEST_ASSERT_EQUAL_UINT16(3000, out);

    /* Wrap with 3500s — eventually median moves */
    battery_voltage_filter_update(&filter, 3500, &out);
    battery_voltage_filter_update(&filter, 3500, &out);
    battery_voltage_filter_update(&filter, 3500, &out);
    /* Buffer now: 3500, 3500, 3500, 3000, 3000 — sorted median = 3500 */
    TEST_ASSERT_EQUAL_UINT16(3500, out);
}

/* ── Reset ───────────────────────────────────────────────────────── */

void test_reset_clears_state(void)
{
    uint16_t out = 0;
    battery_voltage_filter_update(&filter, 5000, &out);
    battery_voltage_filter_update(&filter, 5000, &out);
    battery_voltage_filter_reset(&filter);
    battery_voltage_filter_update(&filter, 3000, &out);
    TEST_ASSERT_EQUAL_UINT16(3000, out);
}

/* ── Test runner ─────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_null_filter);
    RUN_TEST(test_update_null_filter);
    RUN_TEST(test_update_null_output);
    RUN_TEST(test_single_sample_returns_itself);
    RUN_TEST(test_three_samples_returns_middle);
    RUN_TEST(test_outlier_rejected);
    RUN_TEST(test_multiple_outliers_eventually_overcome);
    RUN_TEST(test_even_window_mean_of_middle_two);
    RUN_TEST(test_already_sorted_input);
    RUN_TEST(test_reverse_sorted_input);
    RUN_TEST(test_circular_wrap_after_window_full);
    RUN_TEST(test_reset_clears_state);
    return UNITY_END();
}
```

**Step 2: Write the median filter implementation**

Create `src/core_modules/battery_voltage_filter_median.c`:

```c
/*
 * Median voltage filter — alternative to moving-average for use cases
 * with bursty load-induced voltage sag (e.g. BLE TX pulling Vbat down).
 *
 * Same public API as battery_voltage_filter.c. Selected via Kconfig
 * choice BATTERY_VOLTAGE_FILTER_TYPE.
 *
 * Algorithm: insertion-sort a copy of the buffer, return middle element
 * (or mean of two middle elements for even window). O(n^2) sort but n<=16,
 * so worst case ~256 comparisons — trivial cost at 0.5 Hz sampling.
 *
 * Memory: same battery_voltage_filter_t struct as moving-average filter.
 * The `sum` field is unused in this implementation (kept for ABI compat).
 */

#include "battery_voltage_filter.h"

#include <battery_sdk/battery_status.h>

#include <string.h>

static size_t sanitize_window_size(size_t window_size)
{
    if ((window_size == 0U) ||
        (window_size > BATTERY_VOLTAGE_FILTER_MAX_WINDOW_SIZE)) {
        return BATTERY_VOLTAGE_FILTER_DEFAULT_WINDOW_SIZE;
    }
    return window_size;
}

int battery_voltage_filter_init(battery_voltage_filter_t *filter,
                                size_t window_size)
{
    if (filter == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    memset(filter, 0, sizeof(*filter));
    filter->window_size = sanitize_window_size(window_size);
    filter->initialized = true;
    return BATTERY_STATUS_OK;
}

int battery_voltage_filter_reset(battery_voltage_filter_t *filter)
{
    size_t preserved_window_size;

    if ((filter == NULL) || (filter->initialized == false)) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    preserved_window_size = filter->window_size;
    memset(filter, 0, sizeof(*filter));
    filter->window_size = preserved_window_size;
    filter->initialized = true;
    return BATTERY_STATUS_OK;
}

/**
 * Insertion-sort a copy of the active samples.
 *
 * @param filter  Source filter (read-only).
 * @param sorted  Output buffer (size >= filter->count).
 */
static void copy_and_sort(const battery_voltage_filter_t *filter,
                          uint16_t *sorted)
{
    size_t n = filter->count;

    /* Copy active samples into the output buffer */
    for (size_t i = 0; i < n; i++) {
        sorted[i] = filter->buffer[i];
    }

    /* Insertion sort — stable, in-place, fast for small n */
    for (size_t i = 1; i < n; i++) {
        uint16_t key = sorted[i];
        size_t j = i;
        while (j > 0 && sorted[j - 1] > key) {
            sorted[j] = sorted[j - 1];
            j--;
        }
        sorted[j] = key;
    }
}

static uint16_t median_of(const battery_voltage_filter_t *filter)
{
    uint16_t sorted[BATTERY_VOLTAGE_FILTER_MAX_WINDOW_SIZE];
    size_t n = filter->count;

    copy_and_sort(filter, sorted);

    if (n == 0U) {
        return 0U;
    }
    if ((n % 2U) == 1U) {
        return sorted[n / 2U];
    }
    /* Even count — mean of two middle elements */
    return (uint16_t)(((uint32_t)sorted[n / 2U - 1U] +
                       (uint32_t)sorted[n / 2U]) / 2U);
}

int battery_voltage_filter_update(battery_voltage_filter_t *filter,
                                  uint16_t sample_mv,
                                  uint16_t *filtered_mv_out)
{
    if ((filter == NULL) || (filtered_mv_out == NULL) ||
        (filter->initialized == false)) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    /* Insert sample into circular buffer */
    filter->buffer[filter->index] = sample_mv;
    if (filter->count < filter->window_size) {
        filter->count++;
    }
    filter->index++;
    if (filter->index >= filter->window_size) {
        filter->index = 0U;
    }

    *filtered_mv_out = median_of(filter);
    return BATTERY_STATUS_OK;
}

int battery_voltage_filter_get(const battery_voltage_filter_t *filter,
                               uint16_t *filtered_mv_out)
{
    if ((filter == NULL) || (filtered_mv_out == NULL) ||
        (filter->initialized == false)) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    *filtered_mv_out = median_of(filter);
    return BATTERY_STATUS_OK;
}
```

**Step 3: Add test target to `tests/CMakeLists.txt`**

Append before `enable_testing()`:

```cmake
# ── Median voltage filter test ─────────────────────────────────────────────
add_executable(test_voltage_filter_median
    test_voltage_filter_median.c
    ${SDK_SRC}/core_modules/battery_voltage_filter_median.c
)

target_include_directories(test_voltage_filter_median PRIVATE
    ${SDK_INCLUDE}
    ${SDK_SRC}/core_modules
    ${unity_SOURCE_DIR}/src
)

target_link_libraries(test_voltage_filter_median PRIVATE unity)
```

And register alongside other tests:

```cmake
add_test(NAME voltage_filter_median COMMAND test_voltage_filter_median)
```

**Step 4: Run tests**

```bash
export PATH="/opt/homebrew/bin:/usr/bin:/bin:/opt/nordic/ncs/toolchains/e5f4758bcf/bin:$PATH"
cd tests && rm -rf build && mkdir build && cd build && cmake .. && make -j4 && ctest --output-on-failure
```

Expected: All existing tests pass + new `voltage_filter_median` suite (12 tests) passes.

**Step 5: Commit**

```bash
git add src/core_modules/battery_voltage_filter_median.c \
        tests/test_voltage_filter_median.c \
        tests/CMakeLists.txt
git commit -m "feat: add median voltage filter for sag rejection"
```

---

### Task 2: Kconfig + CMake for Filter Selection

Wires the median filter into the build system as an alternative to moving-average.

**Files:**
- Modify: `app/Kconfig` (add filter type choice)
- Modify: `CMakeLists.txt` (root, conditional source)
- Modify: `app/CMakeLists.txt` (also add conditional)

**Step 1: Add Kconfig choice**

In `app/Kconfig`, find the section after `BATTERY_TEMP_SOURCE` choice (around line 45) and add BEFORE the `config BATTERY_TRANSPORT` line:

```kconfig
choice BATTERY_VOLTAGE_FILTER_TYPE
    prompt "Voltage filter algorithm"
    default BATTERY_VOLTAGE_FILTER_MEAN

config BATTERY_VOLTAGE_FILTER_MEAN
    bool "Moving average (legacy)"
    help
      Simple moving average over the configured window. Smooths
      gradual changes but lets short voltage sags through.

config BATTERY_VOLTAGE_FILTER_MEDIAN
    bool "Median (rejects load-induced sags)"
    help
      Sorted median over the configured window. Single-sample
      outliers (e.g. BLE TX voltage sag) are completely rejected.
      Slightly higher CPU cost (insertion sort) but bounded by
      window size (max 16 samples).

endchoice
```

**Step 2: Update root `CMakeLists.txt`**

In `CMakeLists.txt`, find the line that lists `src/core_modules/battery_voltage_filter.c` (line 28). Replace the unconditional include with a conditional:

```cmake
# Voltage filter: moving-average (default) or median
if(CONFIG_BATTERY_VOLTAGE_FILTER_MEDIAN)
    zephyr_library_sources(src/core_modules/battery_voltage_filter_median.c)
else()
    zephyr_library_sources(src/core_modules/battery_voltage_filter.c)
endif()
```

Remove the `src/core_modules/battery_voltage_filter.c` line from the unconditional `zephyr_library_sources(...)` block.

**Step 3: Update `app/CMakeLists.txt`**

Same pattern. Find the `target_sources` block that includes `../src/core_modules/battery_voltage_filter.c` (line 26) and replace with:

```cmake
# Voltage filter: moving-average (default) or median
if(CONFIG_BATTERY_VOLTAGE_FILTER_MEDIAN)
    target_sources(app PRIVATE ../src/core_modules/battery_voltage_filter_median.c)
else()
    target_sources(app PRIVATE ../src/core_modules/battery_voltage_filter.c)
endif()
```

Remove the unconditional `../src/core_modules/battery_voltage_filter.c` line from the existing `target_sources(...)` block.

**Step 4: Verify tests still pass**

```bash
export PATH="/opt/homebrew/bin:/usr/bin:/bin:/opt/nordic/ncs/toolchains/e5f4758bcf/bin:$PATH"
cd tests && rm -rf build && mkdir build && cd build && cmake .. && make -j4 && ctest --output-on-failure
```

Expected: All tests still pass (the conditionals only affect Zephyr build, not host tests).

**Step 5: Commit**

```bash
git add app/Kconfig CMakeLists.txt app/CMakeLists.txt
git commit -m "feat: add Kconfig + CMake for voltage filter type selection"
```

---

### Task 3: SoC Slew Limiter + Tests

Caps how fast reported SoC can change. Catches LUT plateau cliffs and protects against any remaining voltage filter outliers.

**Files:**
- Modify: `src/intelligence/battery_soc_estimator.c`
- Create: `tests/test_soc_slew_limit.c`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the test file**

Create `tests/test_soc_slew_limit.c`:

```c
/*
 * Unit tests for SoC slew-rate limiter in battery_soc_estimator.
 *
 * Tests with CONFIG_BATTERY_SOC_SLEW_LIMIT=1, voltage-LUT path only
 * (no coulomb counting). Uses mock voltage to feed step changes.
 */

#include "unity.h"
#include <battery_sdk/battery_soc_estimator.h>
#include <battery_sdk/battery_status.h>
#include <stddef.h>
#include <stdint.h>

extern void mock_voltage_set_mv(uint16_t mv);
extern void mock_voltage_set_rc(int rc);

/* Hook for tests to inject elapsed time between get_pct calls.
 * The implementation should call this hook (via a __weak override
 * or test-only function) instead of reading real uptime. */
extern void mock_uptime_set_ms(uint32_t ms);

void setUp(void)
{
    mock_voltage_set_mv(3700);
    mock_voltage_set_rc(BATTERY_STATUS_OK);
    mock_uptime_set_ms(0);
    battery_soc_estimator_init();
}

void tearDown(void) {}

/* ── First-sample bypass ────────────────────────────────────────── */

void test_first_sample_no_slew_limit(void)
{
    uint16_t soc;
    /* First call: voltage = 4200mV (full charge per LiPo LUT) */
    mock_voltage_set_mv(4200);
    mock_uptime_set_ms(0);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_soc_estimator_get_pct_x100(&soc));
    /* Should jump straight to 100% — no slew limit on first call */
    TEST_ASSERT_EQUAL_UINT16(10000, soc);
}

/* ── Slew cap on big drop ──────────────────────────────────────── */

void test_slew_cap_on_voltage_drop(void)
{
    uint16_t soc;
    /* Initialize at full */
    mock_voltage_set_mv(4200);
    mock_uptime_set_ms(0);
    battery_soc_estimator_get_pct_x100(&soc);
    TEST_ASSERT_EQUAL_UINT16(10000, soc);

    /* 1 second later: voltage drops to 3000mV (cutoff = 0% per LUT) */
    mock_voltage_set_mv(3000);
    mock_uptime_set_ms(1000);
    battery_soc_estimator_get_pct_x100(&soc);

    /* Slew rate is 5%/min = 5%/60s = ~0.083%/s.
     * In 1 second, max change = ~8 (0.08% in x100 units).
     * Real change would be 10000 — clamped to 10000 - 8 = 9992. */
    TEST_ASSERT_TRUE(soc > 9900);  /* Not allowed to drop more than ~1% */
}

/* ── Slew cap on big rise ──────────────────────────────────────── */

void test_slew_cap_on_voltage_rise(void)
{
    uint16_t soc;
    /* Initialize at empty */
    mock_voltage_set_mv(3000);
    mock_uptime_set_ms(0);
    battery_soc_estimator_get_pct_x100(&soc);

    /* 1 second later: voltage jumps to 4200mV */
    mock_voltage_set_mv(4200);
    mock_uptime_set_ms(1000);
    battery_soc_estimator_get_pct_x100(&soc);

    /* Cap rise to ~8 in x100 units (0.08% in 1 sec) */
    TEST_ASSERT_TRUE(soc < 1000);  /* Should still be near 0% */
}

/* ── Long elapsed time allows large change ─────────────────────── */

void test_long_elapsed_allows_full_change(void)
{
    uint16_t soc;
    mock_voltage_set_mv(4200);
    mock_uptime_set_ms(0);
    battery_soc_estimator_get_pct_x100(&soc);
    TEST_ASSERT_EQUAL_UINT16(10000, soc);

    /* 1 hour later (60 minutes * 5%/min = 300% — way more than 100%) */
    mock_voltage_set_mv(3000);
    mock_uptime_set_ms(3600 * 1000);
    battery_soc_estimator_get_pct_x100(&soc);

    /* Allowed full change to 0% */
    TEST_ASSERT_TRUE(soc < 500);
}

/* ── dt = 0 edge case ───────────────────────────────────────────── */

void test_zero_dt_freezes_value(void)
{
    uint16_t soc1, soc2;
    mock_voltage_set_mv(3700);
    mock_uptime_set_ms(0);
    battery_soc_estimator_get_pct_x100(&soc1);

    /* Same timestamp, different voltage */
    mock_voltage_set_mv(4200);
    mock_uptime_set_ms(0);
    battery_soc_estimator_get_pct_x100(&soc2);

    /* dt=0 means no time elapsed — slew cap = 0, value frozen */
    TEST_ASSERT_EQUAL_UINT16(soc1, soc2);
}

/* ── Slow real change passes through unchanged ──────────────────── */

void test_slow_change_unaffected(void)
{
    uint16_t soc1, soc2;

    /* 50% SoC */
    mock_voltage_set_mv(3830);
    mock_uptime_set_ms(0);
    battery_soc_estimator_get_pct_x100(&soc1);

    /* 60 seconds later, voltage barely changed (1% real drop) */
    mock_voltage_set_mv(3820);
    mock_uptime_set_ms(60 * 1000);
    battery_soc_estimator_get_pct_x100(&soc2);

    /* Slew cap in 60s = 5% (500 in x100). Actual change ~1% — passes through */
    TEST_ASSERT_TRUE(soc2 < soc1);
    TEST_ASSERT_TRUE((soc1 - soc2) < 500);
}

/* ── Test runner ─────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_first_sample_no_slew_limit);
    RUN_TEST(test_slew_cap_on_voltage_drop);
    RUN_TEST(test_slew_cap_on_voltage_rise);
    RUN_TEST(test_long_elapsed_allows_full_change);
    RUN_TEST(test_zero_dt_freezes_value);
    RUN_TEST(test_slow_change_unaffected);
    return UNITY_END();
}
```

**Step 2: Add a mock uptime control**

The slew limiter needs to know elapsed time. The estimator currently doesn't get timestamps. We need a way to inject time in tests.

Add to existing `tests/mocks/mock_sdk_state.c` (or create `tests/mocks/mock_uptime.c` if cleaner):

Append:
```c
static uint32_t g_mock_uptime_ms = 0;

void mock_uptime_set_ms(uint32_t ms) { g_mock_uptime_ms = ms; }

int battery_sdk_get_uptime_ms(uint32_t *uptime_ms_out)
{
    if (uptime_ms_out == NULL) return BATTERY_STATUS_INVALID_ARG;
    *uptime_ms_out = g_mock_uptime_ms;
    return BATTERY_STATUS_OK;
}
```

(Check if `battery_sdk_get_uptime_ms` is already mocked. If yes, just add the `mock_uptime_set_ms` controller and adjust the existing function. If no, add the whole block.)

**Step 3: Add slew limiter to SoC estimator**

In `src/intelligence/battery_soc_estimator.c`, add at the top of includes:

```c
#include <battery_sdk/battery_sdk.h>  /* for battery_sdk_get_uptime_ms */
```

Add slew-limit state at the top (after the existing static state, around line 33):

```c
#if defined(CONFIG_BATTERY_SOC_SLEW_LIMIT)

#ifndef CONFIG_BATTERY_SOC_SLEW_RATE_PCT_PER_MIN
#define CONFIG_BATTERY_SOC_SLEW_RATE_PCT_PER_MIN 5
#endif

/* Slew rate in 0.01% per millisecond.
 * pct_per_min * 100 (x100) / 60000 (ms/min). Stored at compile time. */
#define SLEW_RATE_X100_PER_MS_NUM (CONFIG_BATTERY_SOC_SLEW_RATE_PCT_PER_MIN * 100)
#define SLEW_RATE_X100_PER_MS_DEN 60000

static uint16_t g_prev_soc_x100;
static uint32_t g_prev_uptime_ms;
static bool     g_slew_initialized;

#endif  /* CONFIG_BATTERY_SOC_SLEW_LIMIT */
```

Add a helper function:

```c
#if defined(CONFIG_BATTERY_SOC_SLEW_LIMIT)
/**
 * Apply slew-rate limit to SoC value. First call returns input unchanged
 * and seeds state. Subsequent calls cap delta to SLEW_RATE * dt_ms.
 *
 * @param new_soc_x100  Proposed SoC value (0-10000).
 * @return Slew-limited SoC value.
 */
static uint16_t apply_slew_limit(uint16_t new_soc_x100)
{
    uint32_t now_ms;
    int rc = battery_sdk_get_uptime_ms(&now_ms);
    if (rc != BATTERY_STATUS_OK) {
        return new_soc_x100;  /* Can't compute dt — bypass */
    }

    if (!g_slew_initialized) {
        g_prev_soc_x100 = new_soc_x100;
        g_prev_uptime_ms = now_ms;
        g_slew_initialized = true;
        return new_soc_x100;
    }

    uint32_t dt_ms = now_ms - g_prev_uptime_ms;
    /* Max delta in x100 units: rate * dt / 60000 */
    int32_t max_delta = (int32_t)((uint64_t)SLEW_RATE_X100_PER_MS_NUM * dt_ms
                                   / SLEW_RATE_X100_PER_MS_DEN);

    int32_t delta = (int32_t)new_soc_x100 - (int32_t)g_prev_soc_x100;

    if (delta > max_delta) {
        new_soc_x100 = (uint16_t)((int32_t)g_prev_soc_x100 + max_delta);
    } else if (delta < -max_delta) {
        new_soc_x100 = (uint16_t)((int32_t)g_prev_soc_x100 - max_delta);
    }

    g_prev_soc_x100 = new_soc_x100;
    g_prev_uptime_ms = now_ms;
    return new_soc_x100;
}
#endif  /* CONFIG_BATTERY_SOC_SLEW_LIMIT */
```

Modify `battery_soc_estimator_init` (around line 56) to reset slew state:

```c
int battery_soc_estimator_init(void)
{
    struct battery_sdk_runtime_state *state = battery_sdk_state();
    state->soc_initialized = true;
#if defined(CONFIG_BATTERY_SOC_COULOMB)
    g_coulomb_soc_valid = false;
#endif
#if defined(CONFIG_BATTERY_SOC_SLEW_LIMIT)
    g_slew_initialized = false;
#endif
    return BATTERY_STATUS_OK;
}
```

Apply slew limit at the END of `battery_soc_estimator_get_pct_x100` (just before returns):

For the `#if defined(CONFIG_BATTERY_SOC_COULOMB)` path, just before `return BATTERY_STATUS_OK;` at line 157:

```c
#if defined(CONFIG_BATTERY_SOC_SLEW_LIMIT)
        g_coulomb_soc_x100 = apply_slew_limit(g_coulomb_soc_x100);
        *soc_pct_x100 = g_coulomb_soc_x100;
#endif
        return BATTERY_STATUS_OK;
```

For the `#else` path (LUT only), at line 161:

```c
#if defined(CONFIG_BATTERY_SOC_SLEW_LIMIT)
    *soc_pct_x100 = apply_slew_limit(lut_soc);
#else
    *soc_pct_x100 = lut_soc;
#endif
    return BATTERY_STATUS_OK;
```

Also handle the early-return paths (anchor reset writes directly to `g_coulomb_soc_x100`) — those bypass the slew limiter by design (anchor events should reset to LUT immediately).

**Step 4: Add Kconfig**

In `app/Kconfig`, after the `BATTERY_CAPACITY_MAH` block, add:

```kconfig
config BATTERY_SOC_SLEW_LIMIT
    bool "Limit SoC change rate (smooths reported value)"
    default y
    help
      Caps the SoC change rate to a configurable maximum percent per
      minute. Filters out transient SoC swings caused by voltage sag
      or LUT plateau cliffs. Bypassed on first sample after init and
      on coulomb-counting anchor events (when enabled).

config BATTERY_SOC_SLEW_RATE_PCT_PER_MIN
    int "Max SoC change per minute (percent)"
    default 5
    range 1 100
    depends on BATTERY_SOC_SLEW_LIMIT
    help
      Maximum allowed SoC change per minute. 5%/min matches typical
      LiPo discharge rates for IoT loads. Higher values reduce
      smoothing but allow faster real changes.
```

**Step 5: Add test target to `tests/CMakeLists.txt`**

```cmake
# ── SoC slew limit test ──────────────────────────────────────────────────
add_executable(test_soc_slew_limit
    test_soc_slew_limit.c
    ${SDK_SRC}/intelligence/battery_soc_estimator.c
    ${SDK_SRC}/intelligence/battery_soc_lut.c
    mocks/mock_voltage.c
    mocks/mock_sdk_state.c
)

target_include_directories(test_soc_slew_limit PRIVATE
    ${SDK_INCLUDE}
    ${SDK_SRC}
    ${SDK_SRC}/core
    ${SDK_SRC}/intelligence
    ${unity_SOURCE_DIR}/src
)

target_compile_definitions(test_soc_slew_limit PRIVATE
    CONFIG_BATTERY_SOC_SLEW_LIMIT=1
    CONFIG_BATTERY_SOC_SLEW_RATE_PCT_PER_MIN=5
    CONFIG_BATTERY_CHEMISTRY_LIPO=1
)

target_link_libraries(test_soc_slew_limit PRIVATE unity)
```

And register: `add_test(NAME soc_slew_limit COMMAND test_soc_slew_limit)`

**Step 6: Run tests**

```bash
export PATH="/opt/homebrew/bin:/usr/bin:/bin:/opt/nordic/ncs/toolchains/e5f4758bcf/bin:$PATH"
cd tests && rm -rf build && mkdir build && cd build && cmake .. && make -j4 && ctest --output-on-failure
```

Expected: All tests pass (16 suites total).

**Step 7: Commit**

```bash
git add src/intelligence/battery_soc_estimator.c \
        tests/test_soc_slew_limit.c \
        tests/mocks/mock_sdk_state.c \
        tests/CMakeLists.txt \
        app/Kconfig
git commit -m "feat: add SoC slew-rate limiter to smooth reported values"
```

---

### Task 4: ESP32-C3 + nRF52840 Build Verification

Verify both platforms build with new options.

**Step 1: Build ESP32-C3 (no current sense, with median filter)**

Create `app/boards/esp32c3_devkitm_median.conf`:

```
CONFIG_BATTERY_VOLTAGE_FILTER_MEDIAN=y
```

```bash
export PATH="/opt/homebrew/bin:/usr/bin:/bin:/opt/nordic/ncs/toolchains/e5f4758bcf/bin:$PATH"
cd ~/zephyr-esp32 && source .venv/bin/activate
export ZEPHYR_BASE=~/zephyr-esp32/zephyr
export ZEPHYR_SDK_INSTALL_DIR=/opt/nordic/ncs/toolchains/e5f4758bcf/opt/zephyr-sdk
west build -b esp32c3_devkitm /Users/aliapin/Downloads/project/ibattery-sdk/app \
    -d build-esp32c3-median --pristine -- \
    -DEXTRA_CONF_FILE=boards/esp32c3_devkitm_median.conf
```

Expected: Clean build with `battery_voltage_filter_median.c` linked instead of `battery_voltage_filter.c`.

**Step 2: Build nRF52840-DK**

```bash
export PATH="/opt/homebrew/bin:/usr/bin:/bin:/opt/nordic/ncs/toolchains/e5f4758bcf/bin:$PATH"
export ZEPHYR_BASE="/opt/nordic/ncs/v3.2.2/zephyr"
export ZEPHYR_SDK_INSTALL_DIR="/opt/nordic/ncs/toolchains/e5f4758bcf/opt/zephyr-sdk"
west build -b nrf52840dk/nrf52840 /Users/aliapin/Downloads/project/ibattery-sdk/app \
    -d /tmp/build-nrf-median --pristine -- \
    -DCONFIG_BATTERY_VOLTAGE_FILTER_MEDIAN=y
```

Expected: Clean build.

**Step 3: Default build (mean filter) — regression check**

```bash
west build -b nrf52840dk/nrf52840 /Users/aliapin/Downloads/project/ibattery-sdk/app \
    -d /tmp/build-nrf-mean --pristine
```

Expected: Clean build with original `battery_voltage_filter.c` (existing behavior).

**Step 4: Commit board config**

```bash
git add app/boards/esp32c3_devkitm_median.conf
git commit -m "feat: add ESP32-C3 build config for median filter"
```

---

### Task 5: Documentation Update

**Files:**
- Modify: `docs/RELEASE_NOTES.md` (add v0.9.0 entry)
- Modify: `docs/ROADMAP.md` (mark Phase 8b complete)
- Modify: `docs/ARCHITECTURE.md` (note filter selection)
- Modify: `docs/SDK_API.md` (note new Kconfig)

**Step 1: Add v0.9.0 release notes**

In `docs/RELEASE_NOTES.md`, add at the top (before v0.8.0):

```markdown
## v0.9.0 — Voltage Smoothing & SoC Slew Limiter (Phase 8b) — 2026-04-XX

Software-only accuracy improvement for SoC estimation. Two
defense-in-depth layers against load-induced voltage sag and
LUT plateau cliffs.

### New Features

**Median Voltage Filter**
- Alternative to existing moving-average filter
- Selected via Kconfig: `CONFIG_BATTERY_VOLTAGE_FILTER_MEDIAN=y`
- Single-sample outliers (BLE TX sag, etc.) completely rejected
- Same struct, same API — drop-in replacement at compile time

**SoC Slew-Rate Limiter**
- Caps reported SoC change rate (default 5%/min)
- Bypassed on first sample and anchor events (when 8a enabled)
- New Kconfig: `CONFIG_BATTERY_SOC_SLEW_LIMIT`,
  `CONFIG_BATTERY_SOC_SLEW_RATE_PCT_PER_MIN`

### Tests

- 12 new median filter tests
- 6 new slew limiter tests
- All 16 C test suites pass + 65 gateway tests

### Composition with Phase 8a

When both `BATTERY_SOC_COULOMB` and `BATTERY_SOC_SLEW_LIMIT` are
enabled, slew limiter applies to the final SoC value (after coulomb).
Anchor events bypass the slew limiter for instant reset to LUT value.

---
```

**Step 2: Update ROADMAP**

In `docs/ROADMAP.md`, update the Phase 8b entry from "(planned)" to "(complete in v0.9.0)".

**Step 3: Commit**

```bash
git add docs/RELEASE_NOTES.md docs/ROADMAP.md docs/ARCHITECTURE.md docs/SDK_API.md
git commit -m "docs: add v0.9.0 release notes for Phase 8b"
```

---

### Task 6: Tag v0.9.0

```bash
git tag -a v0.9.0 -m "v0.9.0 — Voltage Smoothing & SoC Slew Limiter (Phase 8b)

Median voltage filter (alternative to moving-average) +
SoC slew-rate limiter. Software-only accuracy improvement.
16/16 C tests + 65/65 gateway tests pass.

Composes with Phase 8a coulomb counting when INA219 is available."
```

---

## Composition with Phase 8a

When all options enabled:

```
ADC raw -> median filter -> LUT SoC -> coulomb correction (8a)
        -> slew limit (8b) -> final SoC -> telemetry
```

Each layer is independent and Kconfig-gated. Disabling one doesn't affect the others.
