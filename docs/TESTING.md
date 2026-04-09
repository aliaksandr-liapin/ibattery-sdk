# Testing Guide

## Overview

The Battery SDK uses **host-based unit tests** that compile and run on the development machine (macOS/Linux) without Zephyr, nRF toolchain, or target hardware. This enables fast iteration: tests run in under 1 second.

**Framework:** [Unity](https://github.com/ThrowTheSwitch/Unity) v2.6.0 (fetched automatically via CMake FetchContent)

**Firmware test count:** 11 C test suites (Unity)

**Gateway test count:** 58 Python tests (pytest)

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
     Start  1: voltage_filter
 1/11 Test  #1: voltage_filter ...................   Passed    0.00 sec
     Start  2: soc_lut
 2/11 Test  #2: soc_lut ..........................   Passed    0.00 sec
     Start  3: soc_temp_comp
 3/11 Test  #3: soc_temp_comp ....................   Passed    0.00 sec
     Start  4: telemetry
 4/11 Test  #4: telemetry ........................   Passed    0.00 sec
     Start  5: temperature
 5/11 Test  #5: temperature ......................   Passed    0.00 sec
     Start  6: power_manager
 6/11 Test  #6: power_manager ....................   Passed    0.00 sec
     Start  7: power_manager_charger
 7/11 Test  #7: power_manager_charger ............   Passed    0.00 sec
     Start  8: ntc_lut
 8/11 Test  #8: ntc_lut ..........................   Passed    0.00 sec
     Start  9: serialize
 9/11 Test  #9: serialize ........................   Passed    0.00 sec
     Start 10: transport
10/11 Test #10: transport ........................   Passed    0.00 sec
     Start 11: cycle_counter
11/11 Test #11: cycle_counter ....................   Passed    0.00 sec

100% tests passed, 0 tests failed out of 11

Total Test time (real) =   0.05 sec
```

### Running a single suite

```bash
./build_tests/test_voltage_filter
./build_tests/test_soc_lut
./build_tests/test_soc_temp_comp
./build_tests/test_telemetry
./build_tests/test_temperature
./build_tests/test_power_manager
./build_tests/test_power_manager_charger
./build_tests/test_ntc_lut
./build_tests/test_serialize
./build_tests/test_transport
./build_tests/test_cycle_counter
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

### 3. SoC Temperature Compensation (`test_soc_temp_comp.c`)

Tests temperature compensation logic for LiPo SoC estimation. Pure integer math, no mocks.

| Test | What it verifies |
|------|-----------------|
| Temperature correction factors | Cold/hot temperature adjustments to SoC |
| CR2032 passthrough | No compensation applied when chemistry is CR2032 |
| Boundary conditions | Edge cases at temperature extremes |

### 4. Telemetry (`test_telemetry.c`) — 9 tests

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

### 5. Temperature (`test_temperature.c`) — 7 tests

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

### 6. NTC LUT (`test_ntc_lut.c`) — 21 tests

Tests the NTC thermistor resistance-to-temperature conversion pipeline: voltage divider math and LUT interpolation. Pure integer math, no mocks.

**Resistance Conversion (8 tests)**

| Test | What it verifies |
|------|-----------------|
| `test_resistance_null_output` | NULL output returns INVALID_ARG |
| `test_resistance_zero_vdd` | Zero VDD returns INVALID_ARG |
| `test_resistance_adc_equals_vdd_open_circuit` | ADC = VDD (open circuit NTC) returns IO error |
| `test_resistance_adc_above_vdd_open_circuit` | ADC > VDD returns IO error |
| `test_resistance_adc_zero_shorted` | ADC = 0 (shorted NTC) returns resistance = 0 |
| `test_resistance_half_vdd_equals_pullup` | ADC = VDD/2 → R = pullup (balanced divider) |
| `test_resistance_low_voltage_high_resistance` | Low ADC → high resistance (cold NTC) |
| `test_resistance_high_voltage_low_resistance` | High ADC → low resistance (hot NTC) |

**LUT Interpolation (13 tests)**

| Test | What it verifies |
|------|-----------------|
| `test_interpolate_null_lut` | NULL LUT returns INVALID_ARG |
| `test_interpolate_null_output` | NULL output returns INVALID_ARG |
| `test_interpolate_empty_lut` | Empty LUT returns INVALID_ARG |
| `test_interpolate_exact_25c_reference` | 10000 Ω = 25.00 °C (reference point) |
| `test_interpolate_exact_0c` | 33640 Ω = 0.00 °C |
| `test_interpolate_exact_negative_40c` | 401600 Ω = -40.00 °C (coldest entry) |
| `test_interpolate_exact_125c` | 359 Ω = 125.00 °C (hottest entry) |
| `test_interpolate_above_max_resistance_clamps_cold` | Very high R clamps to -40 °C |
| `test_interpolate_below_min_resistance_clamps_hot` | Very low R clamps to 125 °C |
| `test_interpolate_midpoint_20c_25c` | 11260 Ω → 22.50 °C (room temp interpolation) |
| `test_interpolate_negative_range` | 152900 Ω → -25.00 °C (negative temp interpolation) |
| `test_interpolate_across_zero` | 26920 Ω → 5.00 °C (crosses 0 °C boundary) |
| `test_interpolate_hot_range` | 983 Ω → 90.01 °C (high temp interpolation) |

### 7. Power Manager (`test_power_manager.c`) — 12 tests

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

### 8. Power Manager + Charger (`test_power_manager_charger`) — 23 tests

Same test file as Power Manager but compiled with `CONFIG_BATTERY_CHARGER_TP4056=1`. Tests charger-specific state transitions.

| Test | What it verifies |
|------|-----------------|
| CHARGING state | CHRG=LOW detected correctly |
| CHARGED state | STDBY=LOW detected correctly |
| DISCHARGING default | Both pins floating → DISCHARGING |
| Critical-to-charging recovery | CRITICAL → charger connected → CHARGING |
| Charger error fallback | Charger read failure → fall through to voltage logic |
| Charging overrides idle | CHARGING takes priority over inactivity-based IDLE/SLEEP |

### 9. Serialization (`test_serialize.c`) — 15 tests

Tests the telemetry packet wire serialization (pack/unpack). Pure logic, no mocks.

| Test | What it verifies |
|------|-----------------|
| `test_pack_null_pkt` | NULL packet returns INVALID_ARG |
| `test_pack_null_buf` | NULL buffer returns INVALID_ARG |
| `test_pack_short_buf` | Buffer < 20 bytes returns INVALID_ARG |
| `test_unpack_null_buf` | NULL buffer returns INVALID_ARG |
| `test_unpack_null_pkt` | NULL packet returns INVALID_ARG |
| `test_unpack_short_buf` | Buffer < 20 bytes returns INVALID_ARG |
| `test_roundtrip_typical` | Pack → unpack with typical values matches original |
| `test_roundtrip_zeroes` | All-zero packet survives round-trip |
| `test_roundtrip_max_values` | Maximum field values survive round-trip |
| `test_roundtrip_negative_values` | Negative voltage/temperature survive round-trip |
| `test_roundtrip_all_power_states` | All power state enum values survive round-trip |
| `test_wire_version_byte` | Version byte at correct offset (0) |
| `test_wire_timestamp_le` | Timestamp at offset 1, little-endian |
| `test_wire_voltage_le` | Voltage at offset 5, little-endian |
| `test_wire_format_total_size` | Pack writes exactly 20 bytes |

### 10. Transport (`test_transport.c`) — 11 tests

Tests the transport abstraction layer with a mock backend. Verifies delegation, error propagation, and wire byte correctness.

| Test | What it verifies |
|------|-----------------|
| `test_init_calls_backend` | Init delegates to backend and returns OK |
| `test_init_error_propagates` | Backend init error propagates to caller |
| `test_send_null_packet` | NULL packet returns INVALID_ARG |
| `test_send_happy_path` | Send serializes and delegates, correct wire size |
| `test_send_captures_correct_wire_bytes` | Captured bytes match independent serialization |
| `test_send_backend_error_propagates` | Backend send error propagates to caller |
| `test_deinit_calls_backend` | Deinit delegates to backend |
| `test_deinit_error_propagates` | Backend deinit error propagates |
| `test_is_connected_null_pointer` | NULL output returns INVALID_ARG |
| `test_is_connected_returns_false` | Reports disconnected state correctly |
| `test_is_connected_returns_true` | Reports connected state correctly |

### 11. Cycle Counter (`test_cycle_counter.c`)

Tests charge cycle counting logic with mock NVS backend.

| Test | What it verifies |
|------|-----------------|
| Init loads from NVS | Stored count loaded on init |
| Init with no stored value | Starts at 0 when NVS is empty |
| CHARGING → CHARGED increments | Transition detected, count incremented |
| Non-transition no increment | Other state changes don't increment |
| Get returns current count | `battery_cycle_counter_get()` returns correct value |
| NULL pointer check | NULL output returns INVALID_ARG |

---

## Python Gateway Tests

The gateway has **58 pytest tests** across 6 test files.

### Running

```bash
cd gateway
pip install -e ".[dev]"
python -m pytest tests/ -v
```

### Test Files

| File | Tests | What it covers |
|------|-------|---------------|
| `test_decoder.py` | 25 | Wire v1 (20B) and v2 (24B) decoding, error handling, format_packet |
| `test_writer.py` | 5 | InfluxDB point construction, error resilience |
| `test_realtime.py` | 16 | Per-packet anomaly detection: voltage critical, SoC inconsistency, temperature high/low, multiple anomalies, CR2032 compatibility |
| `test_rul_estimator.py` | 5 | Linear regression for RUL: positive/negative slope, flat, single point, two points |
| `test_cycle_analyzer.py` | 2 | CycleAnalyzer instantiation, context manager protocol |
| `test_health_score.py` | 5 | Health scoring logic |

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
| `mock_transport.c` | `battery_transport_ble_zephyr.c` | init/send/deinit return codes, connected state, captured send buffer + count |
| `mock_charger.c` | `battery_hal_charger_tp4056_zephyr.c` | charger state (CHRG/STDBY pins), return code |
| `mock_nvs.c` | `battery_hal_nvs_zephyr.c` | NVS read/write values, return codes (RAM-backed) |
| `mock_cycle_counter.c` | `battery_cycle_counter.c` | cycle count value, return code |

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

In addition to host tests, firmware changes are validated on real hardware.

### nRF52840-DK

1. Build: `west build -b nrf52840dk/nrf52840 app -d build-nrf --pristine`
2. Flash: `west flash -d build-nrf`
3. Monitor serial output (115200 baud) for telemetry packets
4. Verify voltage readings match expected CR2032 range (2000-3100 mV)
5. Verify `flags=0x00000000` (no collection errors)
6. Verify BLE: `ibattery-gateway scan` finds "iBattery"

### NUCLEO-L476RG (STM32)

1. Build: `west build -b nucleo_l476rg app -d build-stm32 --pristine -- -DZEPHYR_EXTRA_MODULES="..."`
2. Flash: `west flash -d build-stm32 --runner openocd`
3. Monitor serial on `/dev/tty.usbmodem*` (115200 baud)
4. Verify boot banner shows "Platform: STM32L4 (NUCLEO-L476RG)"
5. Verify VDD ~3300 mV (USB powered), die temp 20-40 C, `flags=0x00000000`
6. Verify state machine: ACTIVE (t=0) → IDLE (~30s) → SLEEP (~120s)

### NUCLEO-L476RG + BLE Shield

1. Mount X-NUCLEO-IDB05A2 on Arduino headers
2. Build with `-DSHIELD=x_nucleo_idb05a1 -DEXTRA_CONF_FILE=boards/nucleo_l476rg_ble.conf`
3. Flash and verify "iBattery-STM32" visible in `ibattery-gateway scan`
4. Verify `ibattery-gateway stream` receives v2 telemetry packets
5. Verify full pipeline: `ibattery-gateway run` → InfluxDB → Grafana dashboard
