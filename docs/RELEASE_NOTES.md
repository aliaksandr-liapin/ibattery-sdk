# Release Notes

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
