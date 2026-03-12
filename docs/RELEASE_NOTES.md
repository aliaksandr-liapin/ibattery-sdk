# Release Notes

## v0.2.1 — SAADC ADC Enum Fix & Channel Re-setup Workaround (2026-03-12)

Fixes incorrect NTC temperature readings caused by an analog input enum mismatch and an nRF SAADC driver quirk.

### Bug Fixes

**NTC reading wrong ADC pin (off-by-one)**
- Root cause: `NRF_SAADC_INPUT_AIN1` (from legacy `<hal/nrf_saadc.h>`) has value 2, but the Zephyr ADC driver interprets the `input_positive` field using the nrfx v3.x 0-based scheme where value 2 = AIN2 (P0.04), not AIN1 (P0.03)
- Fix: switched to `NRFX_ANALOG_EXTERNAL_AIN1` (value 1) from `<helpers/nrfx_analog_common.h>`
- Symptom: NTC reads ~87 mV instead of ~1500 mV at room temperature; LUT clamped to 125 °C

**SAADC channel input-mux clobbering across channels**
- Root cause: the nRF SAADC driver does not preserve per-channel input-mux (`PSELP`) settings when a different channel is read; reading internal VDD on channel 0 overwrites channel 1's AIN1 selection
- Fix: both HAL ADC drivers (`battery_hal_adc_zephyr.c` and `battery_hal_temp_ntc_zephyr.c`) now call `adc_channel_setup()` before every `adc_read()` to restore the correct input configuration
- Channel configs moved from local variables in init functions to file-scope `static const` structs so they are available in read functions
- Symptom: first NTC read after boot was correct, but subsequent reads returned ~3020 mV (VDD value) after a VDD channel read

### Hardware Verified

- nRF52840-DK (PCA10056 rev 3.0.3)
- 10K NTC (B=3950) on AIN1 (P0.03) with 10K pullup to VDD
- Stable ~29.5 °C readings at room temperature, surviving multiple reboots

---

## v0.2.0 — Phase 2: Real Temperature + Power State Machine (2026-03-09)

Replaces the two remaining stubs from Phase 1 with real implementations: die temperature sensor readings, external NTC thermistor support, and voltage-based power state detection with hysteresis.

### What's New

**Real Temperature Measurement**
- nRF52840 on-chip die temperature sensor via Zephyr sensor API (±2 °C accuracy)
- New HAL driver: `battery_hal_temp_zephyr.c` using `SENSOR_CHAN_DIE_TEMP`
- Temperature module now delegates to HAL instead of returning fixed 25.00 °C

**External NTC Thermistor Support**
- New HAL driver: `battery_hal_temp_ntc_zephyr.c` for 10K NTC (B=3950) on SAADC AIN1 (P0.03)
- New intelligence module: `battery_ntc_lut.c` with 16-point resistance-to-temperature LUT (-40 °C to +125 °C)
- Voltage divider math: ADC → millivolts → resistance → temperature, all integer math
- Compile-time selection via Kconfig: `CONFIG_BATTERY_TEMP_NTC` (default) or `CONFIG_BATTERY_TEMP_DIE`
- Same HAL interface (`battery_hal_temp_read_c_x100`) — modules above HAL are unchanged

**Voltage-Based Power State Machine**
- Threshold-based state detection: enters CRITICAL when voltage drops below 2100 mV
- Hysteresis dead band: exits CRITICAL only when voltage rises above 2200 mV (100 mV band prevents oscillation)
- Graceful degradation: returns last known state if voltage read fails
- Uses existing `battery_voltage_get_mv()` (lateral dependency, init-order safe)

**LiPo Single-Cell Discharge Curve**
- 11-point voltage-to-SoC lookup table for 3.7 V nominal LiPo cells (4200 mV → 3000 mV)
- Extra density in knee/cliff regions below 3700 mV to minimise interpolation error
- Data synthesised from multiple sources (Grepow, Adafruit, RC community measurements)
- Shared interpolation engine — no code duplication, just data

**Expanded Test Coverage**
- 80 tests across 6 suites (up from 32 tests across 3 suites)
- New: NTC LUT suite (21 tests) — resistance conversion edge cases, LUT interpolation across full temperature range, negative temps, clamping
- New: Temperature suite (7 tests) — HAL delegation, negative temps, error propagation
- New: Power manager suite (12 tests) — thresholds, hysteresis boundary conditions, graceful degradation
- New: LiPo LUT tests (8 tests) — exact points, clamping, interpolation across plateau/knee/cliff regions
- Updated mock_hal.c with temperature HAL stubs

**Build Configuration**
- New `app/Kconfig` with temperature source selection (NTC vs die sensor)
- Conditional compilation in CMake: NTC HAL + NTC LUT or die sensor HAL
- Added `&temp { status = "okay"; }` to devicetree overlay
- +1 byte static RAM (power state hysteresis memory)

### Known Limitations

- Die temperature measures chip temperature, not ambient or battery temperature
- IDLE and SLEEP power states defined but not implemented (require Zephyr PM integration)
- SoC lookup table is static; coulomb counting / adaptive estimation planned for future phases
- No persistent storage of calibration data
- No wireless telemetry transport (serial only)

---

## v0.1.0 — Phase 1 Complete (2026-03-09)

First functional release of the Battery SDK. All Phase 1 objectives are met: the firmware reads real battery voltage from a CR2032 coin cell, estimates state-of-charge, and outputs structured telemetry packets over serial.

### What's New

**Core SDK**
- `battery_sdk_init()` — single-call initialization of all subsystems in dependency order (HAL, voltage, temperature, power manager, SoC estimator, telemetry)
- Unified error codes via `battery_status.h` (OK, ERROR, INVALID_ARG, NOT_INITIALIZED, UNSUPPORTED, IO)
- Internal runtime state tracking for all modules

**Voltage Measurement**
- Real ADC reading via nRF52840 SAADC VDD input
- 12-bit resolution, 1/6 gain, internal 0.6V reference (3.6V full-scale)
- Moving average filter: window size 12, O(1) update, 28 bytes RAM, deterministic
- Typical readings: 3012-3017 mV on fresh CR2032

**SoC Estimation**
- CR2032 voltage-to-SoC lookup table (9-point discharge curve)
- Linear interpolation between table points, integer math only
- Range: 3000 mV = 100%, 2000 mV = 0%
- Clamped at boundaries (above 3000 mV = 100%, below 2000 mV = 0%)

**Telemetry**
- 7-field telemetry packet: version, timestamp, voltage, temperature, SoC, power state, status flags
- Resilient best-effort collection — collects what it can, sets error flags for failures
- Per-field error flag bits in `status_flags` for voltage, temperature, SoC, power state, and timestamp

**Testing**
- Unity-based host test framework (compiles and runs on macOS, no Zephyr or hardware needed)
- 32 tests across 3 test suites:
  - Voltage filter: 12 tests (null checks, single sample, averaging, window rollover, reset, edge cases)
  - SoC LUT: 11 tests (exact table points, interpolation midpoints, clamping, null safety)
  - Telemetry: 9 tests (full packet assembly, uninitialized state, null handling, partial failures)
- 6 configurable mock modules for isolated unit testing

### Hardware Verified On

- nRF52840-DK (PCA10056 rev 3.0.3)
- CR2032 Energizer coin cell
- Power switch: VDD position
- nRF Connect SDK v3.2.2
- Zephyr OS v4.2.99-fe4f0106803e

### Known Limitations

- Temperature reading is a stub (returns 25.00 C); real sensor integration planned for Phase 2
- Power manager returns ACTIVE state only; dynamic power state transitions planned for Phase 2
- SoC lookup table is static; coulomb counting / adaptive estimation planned for Phase 2
- No persistent storage of calibration data
- No wireless telemetry transport (serial only)

### Breaking Changes from Pre-release

- Removed internal `battery_telemetry.h` (3-field struct) — replaced by public `battery_types.h` (7-field struct)
- All errno-based error codes replaced with `battery_status.h` enum values
- ADC input changed from floating AIN0 pin to VDD rail
- ADC abstraction now uses `NRFX_ANALOG_INTERNAL_VDD` (nrfx v3.x API) instead of raw `NRF_SAADC_INPUT_VDD`
