# Testing Guide

## Overview

The Battery SDK uses **host-based unit tests** that compile and run on the development machine (macOS/Linux) without Zephyr, nRF toolchain, or target hardware. This enables fast iteration: tests run in under 1 second.

**Framework:** [Unity](https://github.com/ThrowTheSwitch/Unity) v2.6.0 (fetched automatically via CMake FetchContent)

**Test count:** 59 tests across 5 suites

---

## Running Tests

```bash
# Configure (one time)
cmake -B build_tests tests

# Build
cmake --build build_tests

# Run all tests
ctest --test-dir build_tests --output-on-failure
```

Expected output:
```
Test project /path/to/ibattery-sdk/build_tests
    Start 1: voltage_filter
1/5 Test #1: voltage_filter ...................   Passed    0.00 sec
    Start 2: soc_lut
2/5 Test #2: soc_lut ..........................   Passed    0.00 sec
    Start 3: telemetry
3/5 Test #3: telemetry ........................   Passed    0.00 sec
    Start 4: temperature
4/5 Test #4: temperature ......................   Passed    0.00 sec
    Start 5: power_manager
5/5 Test #5: power_manager ....................   Passed    0.00 sec

100% tests passed, 0 tests failed out of 5

Total Test time (real) =   0.02 sec
```

### Running a single suite

```bash
./build_tests/test_voltage_filter
./build_tests/test_soc_lut
./build_tests/test_telemetry
./build_tests/test_temperature
./build_tests/test_power_manager
```

---

## Test Suites

### 1. Voltage Filter (`test_voltage_filter.c`) — 12 tests

Tests the moving average filter in isolation. No mocks needed — the filter is a pure function operating on a struct.

| Test | What it verifies |
|------|-----------------|
| `test_init_null_filter` | NULL filter pointer returns INVALID_ARG |
| `test_init_valid` | Initialization succeeds, zeroes internal state |
| `test_update_null_filter` | NULL filter pointer returns INVALID_ARG |
| `test_update_null_output` | NULL output pointer returns INVALID_ARG |
| `test_update_single_sample` | First sample returned directly (no averaging yet) |
| `test_update_averaging` | Average of two samples is correct |
| `test_update_window_full` | Window fills and computes correct average of 12 samples |
| `test_update_window_rollover` | After window overflows, oldest sample drops out correctly |
| `test_reset_clears_state` | Reset zeroes count, sum, and index |
| `test_init_window_zero_clamps` | Window size 0 clamps to 1 |
| `test_init_window_over_max_clamps` | Window above max clamps to max (64) |
| `test_get_without_update` | Get after init (no samples) returns INVALID_ARG |

### 2. SoC LUT (`test_soc_lut.c`) — 19 tests

Tests the voltage-to-SoC lookup table interpolation for both CR2032 and LiPo chemistries. Pure integer math, no mocks.

**CR2032 (11 tests)**

| Test | What it verifies |
|------|-----------------|
| `test_null_lut` | NULL LUT pointer returns INVALID_ARG |
| `test_null_output` | NULL output pointer returns INVALID_ARG |
| `test_empty_lut` | Empty LUT (count=0) returns INVALID_ARG |
| `test_exact_3000mv_is_100pct` | 3000 mV returns exactly 10000 (100.00%) |
| `test_exact_2000mv_is_0pct` | 2000 mV returns exactly 0 (0.00%) |
| `test_exact_2700mv_is_50pct` | 2700 mV returns exactly 5000 (50.00%) |
| `test_above_max_clamps_to_100pct` | 3300 mV clamps to 10000 (100.00%) |
| `test_below_min_clamps_to_0pct` | 1800 mV clamps to 0 (0.00%) |
| `test_interpolation_midpoint_2950mv` | 2950 mV interpolates to 9500 (95.00%) |
| `test_interpolation_2850mv` | 2850 mV interpolates to 8000 (80.00%) |
| `test_interpolation_2300mv` | 2300 mV interpolates to 750 (7.50%) |

**LiPo 1S (8 tests)**

| Test | What it verifies |
|------|-----------------|
| `test_lipo_4200mv_is_100pct` | 4200 mV returns exactly 10000 (100.00%) |
| `test_lipo_3000mv_is_0pct` | 3000 mV returns exactly 0 (0.00%) |
| `test_lipo_3870mv_is_55pct` | 3870 mV returns exactly 5500 (55.00%) |
| `test_lipo_above_max_clamps` | 4500 mV clamps to 10000 (100.00%) |
| `test_lipo_below_min_clamps` | 2500 mV clamps to 0 (0.00%) |
| `test_lipo_interpolation_3950mv` | Plateau region: 3950 mV interpolates to 7000 (70.00%) |
| `test_lipo_interpolation_3745mv` | Knee region: 3745 mV interpolates to 2000 (20.00%) |
| `test_lipo_interpolation_3600mv` | Steep cliff: 3600 mV interpolates to 650 (6.50%) |

### 3. Telemetry (`test_telemetry.c`) — 9 tests

Tests the telemetry collection logic with configurable mocks for all dependencies.

| Test | What it verifies |
|------|-----------------|
| `test_collect_null_packet` | NULL packet pointer returns INVALID_ARG |
| `test_collect_full_packet` | Full packet with all fields populated, flags=0 |
| `test_collect_voltage_error_sets_flag` | Voltage failure sets VOLTAGE_ERR flag, field zeroed |
| `test_collect_temp_error_sets_flag` | Temperature failure sets TEMP_ERR flag |
| `test_collect_soc_error_sets_flag` | SoC failure sets SOC_ERR flag |
| `test_collect_power_error_sets_flag` | Power state failure sets POWER_STATE_ERR flag |
| `test_collect_timestamp_error_sets_flag` | Timestamp failure sets TIMESTAMP_ERR flag, field zeroed |
| `test_collect_multiple_failures` | Multiple subsystems fail — correct flags accumulated, good fields intact |
| `test_init_returns_ok` | `battery_telemetry_init()` returns OK |

### 4. Temperature (`test_temperature.c`) — 7 tests

Tests the temperature module with mock HAL. Verifies HAL delegation, error propagation, and edge cases.

| Test | What it verifies |
|------|-----------------|
| `test_get_null_pointer` | NULL output returns INVALID_ARG |
| `test_get_returns_hal_value` | Normal read returns HAL value (23.50 C) |
| `test_get_negative_temperature` | Handles negative values (-10.50 C) |
| `test_get_zero_temperature` | Handles exactly 0.00 C |
| `test_get_hal_io_error` | HAL IO error propagates correctly |
| `test_init_hal_failure` | Init failure from HAL propagates |
| `test_init_success` | Successful init returns OK |

### 5. Power Manager (`test_power_manager.c`) — 12 tests

Tests the voltage-threshold state machine with hysteresis. Uses mock voltage to simulate battery conditions.

| Test | What it verifies |
|------|-----------------|
| `test_get_state_null_pointer` | NULL output returns INVALID_ARG |
| `test_healthy_voltage_returns_active` | 2950 mV → ACTIVE |
| `test_below_critical_threshold_returns_critical` | 2050 mV → CRITICAL |
| `test_at_critical_enter_boundary` | 2100 mV stays ACTIVE (strict less-than) |
| `test_one_below_critical_enter` | 2099 mV → CRITICAL |
| `test_hysteresis_stays_critical_in_deadband` | CRITICAL at 2150 mV stays CRITICAL |
| `test_hysteresis_exits_critical_above_exit` | CRITICAL → 2201 mV → ACTIVE |
| `test_hysteresis_at_exit_boundary` | CRITICAL at 2200 mV stays CRITICAL (strict greater-than) |
| `test_voltage_error_returns_last_known_active` | Voltage fail after ACTIVE → still ACTIVE |
| `test_voltage_error_returns_last_known_critical` | Voltage fail after CRITICAL → still CRITICAL |
| `test_init_returns_ok` | Init succeeds with OK |
| `test_init_sets_active` | Post-init state is ACTIVE |

---

## Mock Architecture

Mock files are in `tests/mocks/`. Each mock provides:
- A stub implementation of the real module's functions
- `mock_*_set_rc()` — set the return code for subsequent calls
- `mock_*_set_*()` — set the value that will be written to output parameters

### Mock files

| Mock | Replaces | Configurable state |
|------|----------|-------------------|
| `mock_hal.c` | `battery_hal_zephyr.c` + `battery_hal_adc_zephyr.c` + `battery_hal_temp_zephyr.c` | uptime_ms, adc_raw, adc_mv, temp_c_x100, return codes |
| `mock_voltage.c` | `battery_voltage.c` | voltage_mv, return code |
| `mock_temperature.c` | `battery_temperature.c` | temperature_c_x100, return code |
| `mock_soc.c` | `battery_soc_estimator.c` | soc_pct_x100, return code |
| `mock_power.c` | `battery_power_manager.c` | power_state, return code |
| `mock_sdk_state.c` | `battery_sdk.c` (state + uptime) | all-initialized state, uptime_ms, return code |

### Example: configuring a mock

```c
/* In test setup */
mock_voltage_set_rc(BATTERY_STATUS_OK);
mock_voltage_set_mv(2950);

/* Now battery_voltage_get_mv() will write 2950 and return OK */

/* Simulate a failure */
mock_voltage_set_rc(BATTERY_STATUS_IO);
/* Now battery_voltage_get_mv() will return IO error */
```

---

## Adding a New Test

1. Create `tests/test_<module>.c` with Unity test functions
2. Add to `tests/CMakeLists.txt`:
   ```cmake
   add_executable(test_<module>
       test_<module>.c
       ${SDK_SRC}/path/to/module_under_test.c
       # any mocks needed
   )
   target_include_directories(test_<module> PRIVATE
       ${SDK_INCLUDE}
       ${SDK_SRC}/relevant/dirs
       ${unity_SOURCE_DIR}/src
   )
   target_link_libraries(test_<module> PRIVATE unity)
   add_test(NAME <module> COMMAND test_<module>)
   ```
3. Run: `cmake --build build_tests && ctest --test-dir build_tests --output-on-failure`

---

## Hardware Validation

In addition to host tests, firmware changes are validated on real hardware:

1. Build: `cd app && west build -b nrf52840dk/nrf52840`
2. Flash: `west flash`
3. Monitor serial output (115200 baud) for telemetry packets
4. Verify voltage readings match expected CR2032 range (2000-3100 mV)
5. Verify `flags=0x00000000` (no collection errors)
