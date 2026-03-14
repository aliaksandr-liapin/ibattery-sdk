# Release Notes

## v0.4.0 — Phase 4: Cloud Telemetry Pipeline (2026-03-12)

Adds a Python BLE gateway and Docker-based cloud stack (InfluxDB + Grafana) for real-time battery telemetry visualization. Closes the loop from sensor to dashboard.

### What's New

**Python BLE Gateway (`gateway/`)**
- `ibattery-gateway` CLI tool with three commands: `scan`, `stream`, `run`
- BLE connection via bleak library with auto-reconnect and exponential backoff
- Decodes the same 20-byte LE wire format as the firmware serializer
- Writes telemetry to InfluxDB 2.x as `battery_telemetry` measurement points
- Installable Python package with `pip install -e .`

**Docker Cloud Stack (`cloud/`)**
- Docker Compose with InfluxDB 2.x (time-series storage) and Grafana (visualization)
- Auto-provisioned InfluxDB datasource and Grafana dashboard
- "iBattery Telemetry" dashboard with 6 panels:
  - Voltage (V) — time series, 30min window
  - Temperature (°C) — time series, 30min window
  - State of Charge — gauge, 0–100% with color thresholds
  - Power State — stat with value mappings (BOOT/ACTIVE/LOW/CRITICAL/SHUTDOWN)
  - Raw Voltage (mV) — time series, 1h window
  - Status Flags — stat with error threshold coloring
- Anonymous viewer access enabled for local development

**Gateway Tests**
- Decoder tests: known wire bytes, mobile app packet, all power states, edge cases
- Writer tests: mock InfluxDB client, Point field verification, error resilience

### Quick Start

```bash
cd cloud && docker compose up -d          # Start InfluxDB + Grafana
cd gateway && pip install -e .            # Install gateway
ibattery-gateway run                      # BLE → InfluxDB → Grafana
```

Open http://localhost:3000 → "iBattery Telemetry" dashboard.

---

## v0.3.0 — Phase 3: BLE Telemetry Transport (2026-03-12)

Adds wireless telemetry delivery over Bluetooth Low Energy. Telemetry packets now flow off the device via BLE GATT notifications while serial output continues unchanged (dual output).

### What's New

**BLE Transport Layer**
- Custom BLE GATT service with notification characteristic for 20-byte telemetry wire packets
- Service UUID: `12340001-5678-9ABC-DEF0-123456789ABC`
- Characteristic UUID: `12340002-5678-9ABC-DEF0-123456789ABC` (Read + Notify)
- Connectable advertising with device name "iBattery" (configurable)
- Drop policy: silently succeeds when no client subscribed
- Automatic re-advertising on disconnect

**Transport Abstraction**
- Compile-time vtable pattern (`struct battery_transport_ops`) for pluggable backends
- Backend selected via Kconfig: `CONFIG_BATTERY_TRANSPORT_BLE` (or mock for testing)
- `battery_transport_send()` serializes + dispatches in one call
- Returns `BATTERY_STATUS_UNSUPPORTED` when no backend compiled in

**Wire Serialization**
- 20-byte little-endian wire format, fits BLE default ATT MTU (23 − 3 = 20)
- Explicit byte-shift encoding — no struct packing, no memcpy, fully portable
- Pack/unpack round-trip verified by unit tests

**SDK Integration**
- `battery_sdk_init()` now calls `battery_transport_init()` as the last step (guarded by `CONFIG_BATTERY_TRANSPORT`)
- Application main loop calls `battery_transport_send()` after telemetry collection
- Compile without transport: `CONFIG_BATTERY_TRANSPORT=n` → serial-only, zero BLE overhead

**Expanded Test Coverage**
- 106 tests across 8 suites (up from 80 tests across 6 suites)
- New: Serialization suite (15 tests) — round-trip, wire format, null/boundary checks
- New: Transport suite (11 tests) — backend delegation, error propagation, wire byte verification
- New mock: `mock_transport.c` with controllable rc, capture buffer, and send counter

**Build Configuration**
- New Kconfig options: `CONFIG_BATTERY_TRANSPORT`, `CONFIG_BATTERY_TRANSPORT_BLE`, `CONFIG_BATTERY_BLE_DEVICE_NAME`, `CONFIG_BATTERY_BLE_ADV_INTERVAL_MS`
- Stack sizes increased: main=4096, system workqueue=2048 (BLE stack requirements)
- BLE stack adds ~62 KB flash, ~12 KB RAM

### Known Limitations

- Single BLE connection only (`CONFIG_BT_MAX_CONN=1`)
- No authentication or encryption on the GATT characteristic
- No serial transport backend (BLE only for now)
- IDLE and SLEEP power states still not implemented
- No persistent storage of calibration data

---

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
