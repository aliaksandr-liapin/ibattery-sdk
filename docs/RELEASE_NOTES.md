# Release Notes

## v0.7.0 — Phase 7: ESP32-C3 HAL Port + On-Target Validation (2026-04-09)

Adds ESP32-C3 as the third supported platform, making iBattery SDK a genuine 3-platform solution. Introduces a new VDD measurement strategy via external voltage divider. Native BLE — no shield needed.

### What's New

**ESP32-C3 DevKitM Board Support**
- `app/boards/esp32c3_devkitm.overlay` — ADC0, die temp sensor (coretemp), GPIO alias
- `app/boards/esp32c3_devkitm.conf` — board-specific Kconfig (native BLE, die temp default)
- Build requires vanilla Zephyr v4.2.2 workspace (not NCS)

**Voltage Divider ADC Path**
- New `BATTERY_ADC_VDD_USE_DIVIDER` flag — third VDD strategy alongside direct SAADC (nRF52) and VREFINT (STM32)
- External resistor divider: Battery+ → R1(100K) → GPIO2 → R2(100K) → GND
- ADC reads divided voltage, firmware multiplies by `BATTERY_ADC_VDD_DIVIDER_RATIO` (default 2)
- 12 dB attenuation, ~0–2500 mV input range

**Platform Abstraction Improvements**
- `battery_adc_platform.h` — ESP32-C3 section with `BATTERY_ADC_DT_NODE` macro
- `battery_hal_temp_zephyr.c` — auto-detect `coretemp` node label (ESP32)
- `battery_hal_temp_ntc_zephyr.c` — `adc0` node label for ESP32-C3
- `battery_transport_ble_zephyr.c` — conditional include for `assigned_numbers.h` (NCS-only header)
- `app/Kconfig` — `select ESP32_TEMP if SOC_SERIES_ESP32C3` for die temp driver

**Build Matrix**
- nRF52840-DK: 152 KB Flash, 30 KB RAM (NCS)
- NUCLEO-L476RG: 38 KB Flash, 10 KB RAM (NCS)
- ESP32-C3 DevKitM: 356 KB Flash, 138 KB RAM (vanilla Zephyr v4.2.2)

### Hardware Verified

ESP32-C3-DevKitM-1-N4X (RISC-V, ESP32-C3-MINI-1 module):

| Check | Expected | Result |
|-------|----------|--------|
| Boot banner | "ESP32-C3 (DevKitM)" | OK |
| Die temperature sensor | 30-50 C (ESP32 runs warm) | 38-44 C |
| BLE advertising | "iBattery-ESP32C3" visible | OK — RSSI -42/-43 dBm |
| Gateway scan | Device found | OK |
| Gateway stream | Live telemetry via BLE | OK — v2 packets, 2s intervals |
| Gateway run → InfluxDB → Grafana | Full pipeline | OK — dashboard live |
| ACTIVE→IDLE transition | ~30s | OK |
| IDLE→SLEEP transition | ~120s | OK |
| Error flags | 0x00000000 | OK |

**Note**: VDD reads ~4.0–4.2V from floating ADC pin (no voltage divider hardware connected). With R1=R2=100K divider from battery, readings will be accurate.

### No Changes

- All 11 C unit tests pass (host, unchanged)
- All 58 Python gateway tests pass (unchanged)
- No core module, intelligence, telemetry, or transport logic changes
- Wire format v1/v2 unchanged
- nRF52840 and STM32 firmware unchanged

---

## v0.6.0 — Phase 6: STM32 HAL Port + On-Target Validation (2026-04-08)

Adds multi-platform support by porting the HAL layer to STM32L476 (NUCLEO-L476RG) under Zephyr RTOS. All core modules, intelligence, telemetry, and transport code remain unchanged — only the HAL abstraction was refactored to be SoC-agnostic. Hardware-validated on NUCLEO-L476RG.

### What's New

**Platform Abstraction**
- New `src/hal/helpers/battery_adc_platform.h` replaces nRF-specific `nrfx_analog_common.h`
- Per-SoC ADC configuration (input channels, gain, reference voltage) selected via `CONFIG_SOC_SERIES_*`
- STM32 VDD measurement via VREFINT factory calibration (ROM address `0x1FFF75AA`)

**STM32L476 Board Support**
- `app/boards/nucleo_l476rg.overlay` — ADC1, die temp sensor, charger GPIO alias
- `app/boards/nucleo_l476rg.conf` — board-specific Kconfig (die temp + BLE disabled by default)
- BLE supported via X-NUCLEO-IDB05A2 shield (`-DSHIELD=x_nucleo_idb05a1`)

**HAL Portability Improvements**
- `battery_hal_adc_zephyr.c` — VREFINT path for STM32, platform macros for gain/ref
- `battery_hal_temp_ntc_zephyr.c` — platform-agnostic ADC channel and reference
- `battery_hal_temp_zephyr.c` — auto-detect `temp` (nRF) vs `die_temp` (STM32) DT node
- `battery_hal_charger_tp4056_zephyr.c` — DT alias (`battery-charger-gpio`) instead of hardcoded `gpio0`
- `battery_hal_nvs_zephyr.c` — dynamic flash page size query (4096 nRF, 2048 STM32)
- `app/Kconfig` — platform-neutral: conditional `TEMP_NRF5` select, updated help text

**Boot Diagnostics**
- `print_platform_info()` at boot — shows platform, VDD method, temp source, chemistry, transport, charger status
- Useful for on-target validation and debugging multi-platform builds

**Build Matrix**
- nRF52840-DK: `west build -b nrf52840dk/nrf52840 app` — 152 KB Flash, 30 KB RAM
- NUCLEO-L476RG: `west build -b nucleo_l476rg app` — 38 KB Flash, 10 KB RAM

### Hardware Verified

NUCLEO-L476RG (STM32L476RG, ST-Link V2, Rev 4) via OpenOCD:

| Check | Expected | Result |
|-------|----------|--------|
| VDD via VREFINT sensor | 3200-3400 mV (USB) | 3316-3324 mV |
| Die temperature sensor | 20-40 C (room) | 23.23-24.88 C |
| SoC estimation (CR2032 LUT) | 100% at 3.3V | 100.00% |
| Telemetry loop interval | 2000 ms | 2002 ms |
| Error flags | 0x00000000 | 0x00000000 |
| ACTIVE→IDLE transition | ~30s | 30028 ms |
| IDLE→SLEEP transition | ~120s | 120113 ms |

**BLE Shield Validation (X-NUCLEO-IDB05A2)**

| Check | Expected | Result |
|-------|----------|--------|
| BLE SPI init | No crash at boot | OK — telemetry streaming |
| BLE advertising | "iBattery-STM32" visible | OK — RSSI -43 dBm |
| Gateway scan | Device found | OK — `ibattery-gateway scan` |
| Gateway stream | Live telemetry via BLE | OK — v2 packets, 2s intervals |
| BLE wire format | v2, 24 bytes | OK — voltage, temp, SoC, power state |

Build: `west build -b nucleo_l476rg app -- -DSHIELD=x_nucleo_idb05a1 -DEXTRA_CONF_FILE=boards/nucleo_l476rg_ble.conf`
Footprint: 95 KB Flash, 33 KB RAM (vs 38 KB / 10 KB without BLE)

### Status

Fully validated on both platforms including BLE. Pending: NTC thermistor (requires external hardware on PA0), TP4056 charger GPIO (requires wiring PC6/PC7).

### No Changes

- All 11 C unit tests pass (host, unchanged)
- All 58 Python gateway tests pass (unchanged)
- No core module, intelligence, telemetry, or transport changes
- Wire format v1/v2 unchanged
- nRF52840 firmware binary identical in memory footprint

---

## v0.5.1 — Phase 5b: Cycle Counter, Wire v2, RUL & Cycle Analysis (2026-03-14)

Adds charge cycle counting with flash persistence, extends the wire format to 24 bytes (v2), and delivers remaining useful life estimation, cycle analysis, and an 11-panel Grafana dashboard.

### What's New

**Charge Cycle Counter**
- Tracks CHARGING → CHARGED transitions as completed charge cycles
- NVS flash persistence via new HAL interface (`battery_hal_nvs.h`) — count survives reboots
- `battery_cycle_counter_init()` loads stored count (or starts at 0)
- `battery_cycle_counter_update(power_state)` called each telemetry loop; increments on state transition
- `battery_cycle_counter_get(&count)` reads current count

**Wire Format v2 (24 bytes)**
- Extends v1 (20 bytes) with `cycle_count` field at offset 20 (uint32 LE)
- `BATTERY_TELEMETRY_VERSION` bumped from 1 to 2
- `battery_serialize_pack()` writes 24 bytes for v2, 20 bytes for v1
- `battery_serialize_unpack()` accepts both 20-byte and 24-byte buffers — backward compatible
- `BATTERY_TRANSPORT_WIRE_SIZE` updated to 24; BLE MTU configured accordingly

**Gateway Decoder v2**
- Python decoder auto-detects v1 (20B) and v2 (24B) packets
- v2 packets include `cycle_count` field in decoded output and InfluxDB writes

**Cloud Analytics CLI**
- `ibattery-gateway analytics rul` — remaining useful life estimation (linear regression on health vs cycles)
- `ibattery-gateway analytics cycles` — charge cycle pattern analysis (duration, capacity fade, temperature stats)
- Existing commands: `analytics health` (health score) and `analytics anomalies` (anomaly detection)

**Anomaly Detection Tuning**
- Voltage thresholds tuned for dual-chemistry compatibility (CR2032 ~3.0V + LiPo ~3.7V)
- Critical voltage: 2.5V (was 3.0V) — safe for any chemistry
- SoC inconsistency: voltage < 2.8V with SoC > 50% (was 3.2V / 30%)
- Temperature rate threshold: 15°C/min (was 5°C/min) — filters die sensor noise (~0.3°C/sample jitter)
- Eliminates false positive warnings on CR2032 hardware

**Grafana Dashboard v2 (11 panels)**
- Voltage, SoC gauge, Power State, Temperature, Cycle Count, Health Score gauge
- SoC Over Time, Capacity Fade bar chart, Anomaly Log table, RUL stat, Charge Duration Trend
- Import via `gateway/grafana/ibattery-dashboard.json`

**Expanded Tests**
- Firmware: 11 C test suites (was 9) — new: `soc_temp_comp`, `cycle_counter`
- Gateway: 58 Python tests (was 22) — new: decoder v1/v2, RUL estimator, cycle analyzer, updated realtime anomaly tests
- New mocks: `mock_nvs.c` (NVS HAL), `mock_cycle_counter.c` (cycle counter)

### SDK Init Order Update

```
1. battery_hal_init()
2. battery_voltage_init()
3. battery_temperature_init()
4. battery_hal_charger_init()    [if CONFIG_BATTERY_CHARGER_TP4056]
5. battery_power_manager_init()
6. battery_soc_estimator_init()
7. battery_cycle_counter_init()  [NEW — loads NVS count]
8. battery_telemetry_init()
9. battery_transport_init()      [if CONFIG_BATTERY_TRANSPORT]
```

### Hardware Verified

- nRF52840-DK (PCA10056 rev 3.0.3) with CR2032
- Wire v2 packets (24 bytes) confirmed via gateway stream
- Gateway → InfluxDB → Grafana pipeline with all 11 panels
- All 4 analytics CLI commands verified with live data
- Zero false positive anomaly warnings on CR2032

---

## v0.5.0 — Phase 5a: Temperature-Compensated SoC + Cloud Analytics (2026-03-14)

Adds temperature compensation to the SoC estimator for LiPo cells and introduces cloud-side analytics: battery health scoring, real-time and historical anomaly detection.

### What's New

**Temperature-Compensated SoC (LiPo only)**
- `battery_soc_temp_comp.c` — applies temperature correction factors to LUT-based SoC
- Gated by `CONFIG_BATTERY_CHEMISTRY=LIPO` — CR2032 uses raw LUT (temperature compensation is not meaningful for primary cells)
- `CONFIG_BATTERY_CHEMISTRY` Kconfig: selects CR2032 or LiPo LUT at compile time

**Cloud Analytics**
- `ibattery-gateway analytics health` — voltage-based battery health score (0-100)
- `ibattery-gateway analytics anomalies` — historical anomaly detection from InfluxDB data
- Real-time per-packet anomaly checks during BLE streaming (no InfluxDB query needed)
- Anomaly types: voltage drop, SoC inconsistency, critical voltage, high/low temperature, temperature spike

**Gateway Analytics Modules**
- `gateway/analytics/health_score.py` — health scoring from InfluxDB voltage history
- `gateway/analytics/anomaly_detector.py` — historical anomaly detection (voltage + temperature)
- `gateway/analytics/realtime.py` — inline per-packet threshold checks

**Expanded Tests**
- Firmware: new `test_soc_temp_comp` suite (temperature compensation logic)
- Gateway: new `test_realtime.py` (11 tests for per-packet anomaly detection)

---

## v0.4.1 — Expanded Battery Power States + TP4056 Charger Scaffold (2026-03-13)

Adds full battery state machine with IDLE/SLEEP inactivity timers, CHARGING/DISCHARGING/CHARGED states, and a scaffolded TP4056 GPIO charger driver (Kconfig-gated, ready for hardware integration).

### What's New

**Expanded Power State Enum**
- New states: `CHARGING` (5), `DISCHARGING` (6), `CHARGED` (7) — appended for backward compatibility
- Existing `IDLE` (2) and `SLEEP` (3) states now wired into the state machine
- 8 total states: UNKNOWN, ACTIVE, IDLE, SLEEP, CRITICAL, CHARGING, DISCHARGING, CHARGED

**Inactivity Timer State Machine**
- `ACTIVE → IDLE` after 30 seconds of inactivity
- `IDLE → SLEEP` after 120 seconds of inactivity
- `battery_power_manager_report_activity()` resets the timer on BLE connections or user events
- CRITICAL always overrides IDLE/SLEEP (voltage safety takes priority)
- Graceful degradation: uptime read failure skips inactivity logic, stays ACTIVE

**TP4056 Charger Driver (scaffolded behind Kconfig)**
- `CONFIG_BATTERY_CHARGER_TP4056` — disabled by default, flip on when hardware arrives
- Reads CHRG and STDBY GPIO pins (active-low, pull-up configured)
- Truth table: CHRG=LOW → charging, STDBY=LOW → charge complete
- Configurable pin numbers: `CONFIG_BATTERY_CHARGER_TP4056_CHRG_PIN` (default P0.28), `_STDBY_PIN` (default P0.29)
- Charger state overrides inactivity logic (CHARGING > IDLE/SLEEP)
- CRITICAL → CHARGING recovery when charger connected at low voltage

**Gateway + Dashboard Alignment**
- Fixed gateway decoder: state names now match firmware enum exactly (was BOOT/LOW/SHUTDOWN, now UNKNOWN/IDLE/CRITICAL)
- Grafana Power State panel updated with 8-state color mapping
- 22 Python tests (was 20): new `test_charging_states`, `test_idle_sleep_states`

**Expanded Firmware Tests**
- New test suite: `test_power_manager_charger` — 23 tests with `CONFIG_BATTERY_CHARGER_TP4056=1`
- Tests cover: CHARGING, CHARGED, DISCHARGING, critical-to-charging recovery, charger error fallback, charging-overrides-idle
- Base `test_power_manager` suite: 17 tests (was 12) — adds IDLE/SLEEP inactivity, activity reset, critical-overrides-idle, uptime error
- Telemetry suite: 14 tests (was 9) — adds CHARGING, DISCHARGING, CHARGED, IDLE, SLEEP state collection
- Serialize suite: roundtrip for all 8 power states (was 5)
- **Total: 9 suites, 125+ tests** (was 8 suites, 106 tests)

### Hardware Verification

All 6 active power states verified end-to-end on nRF52840-DK (PCA10056 rev 3.0.3) via BLE gateway and Grafana dashboard:

| State | PWR | Verified | Method |
|-------|-----|----------|--------|
| IDLE | 2 | Yes | 30s inactivity timeout (real LiPo power) |
| SLEEP | 3 | Yes | 120s inactivity timeout (real LiPo power) |
| CRITICAL | 4 | Yes | Voltage < 2100 mV threshold |
| CHARGING | 5 | Yes | Jumper wire P0.28 → GND (simulated) |
| DISCHARGING | 6 | Yes | Both pins floating, LiPo powering DK via TP4056 OUT+/OUT- |
| CHARGED | 7 | Yes | Jumper wire P0.29 → GND (simulated) |

**LiPo + TP4056 power delivery verified:**
- LiPo 500mAh (PL 602535, 3.7V) connected to TP4056 HW-373 (USB-C) via B+/B- pads
- TP4056 OUT+/OUT- powering nRF52840-DK VDD/GND — board boots and runs on battery
- USB-C charging confirmed: voltage rises from ~3.01V to ~3.60V when charger connected
- TP4056 red LED lights during charging

> **Note:** CHARGING/CHARGED state detection was verified using jumper wires on GPIO pins (P0.28/P0.29 → GND), not by reading actual TP4056 CHRG/STDBY LED signals. The LED pad soldering for real charger state readout is pending. Power delivery and actual charging via TP4056 are confirmed working. Confidence level for real CHRG/STDBY signal detection: ~85% (depends on LED pad signal quality on HW-373 module).

### Enabling TP4056 (when hardware arrives)

```
# app/prj.conf
CONFIG_BATTERY_CHARGER_TP4056=y
CONFIG_BATTERY_CHARGER_TP4056_CHRG_PIN=28
CONFIG_BATTERY_CHARGER_TP4056_STDBY_PIN=29
```

Wire TP4056 CHRG → P0.28, STDBY → P0.29 on nRF52840-DK. Flash — charging states appear automatically.

---

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
  - Power State — stat with 8-state color-coded value mappings
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
- IDLE and SLEEP power states now implemented (see v0.4.1)
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
