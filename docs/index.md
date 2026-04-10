# Battery SDK

[![CI](https://github.com/aliaksandr-liapin/ibattery-sdk/actions/workflows/ci.yml/badge.svg)](https://github.com/aliaksandr-liapin/ibattery-sdk/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)

Embedded firmware library providing a **standardized battery intelligence layer** for battery-powered IoT devices. Measures voltage, estimates state-of-charge, monitors temperature and power state, and packages everything into structured telemetry packets.

Currently targets the **nRF52840**, **STM32L476**, and **ESP32-C3** (Zephyr RTOS) with **CR2032** coin cell and **LiPo 500mAh** (via TP4056 USB-C charger). All three platforms hardware-validated with full BLE telemetry pipeline. Designed to scale to other MCUs and battery chemistries.

---

## Current Status: Phase 7 Complete (v0.7.0)

| Phase | Description | Status |
|-------|-------------|--------|
| Phase 0 | Hardware setup, firmware skeleton | Done |
| Phase 1 | Core battery intelligence (voltage, SoC, telemetry) | Done |
| Phase 2 | Real temperature sensor + power state machine | Done |
| Phase 3 | BLE telemetry transport | Done |
| Phase 4 | Cloud telemetry (BLE gateway + InfluxDB + Grafana) | Done |
| Phase 5a | Temperature-compensated SoC + cloud analytics (health score, anomaly detection) | Done |
| Phase 5b | Cycle counter, wire v2, RUL estimation, cycle analysis, Grafana dashboard v2 | Done |
| Phase 6 | STM32 HAL port (NUCLEO-L476RG) | Done — hardware-validated, BLE shield tested |
| Phase 7 | ESP32-C3 HAL port (DevKitM) | Done — hardware-validated, native BLE, full pipeline |

---

## Hardware

### nRF52840-DK (primary target)
- **Board**: nRF52840-DK (PCA10056 rev 3.0.3)
- **Battery**: CR2032 Energizer (3V primary lithium) or LiPo 500mAh PL602535 (3.7V)
- **Charger**: TP4056 HW-373 V1.2.1 (USB-C, with DW01A battery protection)
- **Power switch**: VDD position
- **SDK**: nRF Connect SDK v3.2.2 / Zephyr OS v4.2.99
- **ADC**: SAADC Ch0 measuring VDD rail, Ch1 measuring NTC thermistor (AIN1/P0.03); 12-bit, 1/6 gain, 0.6V internal reference

### NUCLEO-L476RG (STM32 port)
- **Board**: NUCLEO-L476RG (STM32L476RGT6, Cortex-M4F, 80 MHz, 1 MB Flash)
- **BLE shield**: X-NUCLEO-IDB05A2 (BlueNRG-M0, optional)
- **ADC**: ADC1 Ch0 = VREFINT (VDD measurement via factory calibration), Ch5 = PA0/A0 (NTC)
- **Charger GPIO**: PC6 (CHRG), PC7 (STDBY) on Morpho connector

### ESP32-C3 DevKitM (ESP32 port)
- **Board**: ESP32-C3-DevKitM-1-N4X (RISC-V, 160 MHz, 4 MB Flash)
- **BLE**: Native BLE 5.0 (no shield needed)
- **ADC**: ADC1 Ch2 = GPIO2 (battery voltage via external divider), Ch3 = GPIO3 (NTC)
- **Temp**: On-chip die temperature sensor (coretemp)
- **SDK**: Vanilla Zephyr v4.2.2 (not NCS)

See [Hardware Wiring Guide](WIRING.md) for pin diagrams and circuit schematics.

---

## Features

- Real voltage measurement via nRF52840 SAADC (VDD input)
- Moving average voltage filter (window=12, O(1), 28 bytes RAM)
- CR2032 voltage-to-SoC lookup table with linear interpolation (integer math only)
- External NTC thermistor support (10K B3950 on AIN1) with 16-point resistance-to-temperature LUT (-40 °C to +125 °C)
- On-chip die temperature sensor via nRF52840 TEMP peripheral (±2 °C accuracy, alternative)
- Compile-time temperature source selection via Kconfig (`CONFIG_BATTERY_TEMP_NTC` default, `CONFIG_BATTERY_TEMP_DIE` alternative)
- Voltage-threshold power state detection with 100 mV hysteresis (CRITICAL below 2100 mV)
- Resilient telemetry collection with per-field error flags
- Graceful degradation — power state survives voltage read failures
- Unified error codes (`battery_status.h`)
- Centralized SDK initialization (`battery_sdk_init()`)
- LiPo single-cell (3.7 V nominal) discharge curve LUT (11-point, extra density in knee region)
- `CONFIG_BATTERY_CHEMISTRY` Kconfig: selects CR2032 or LiPo LUT + gates temp compensation on LiPo only
- BLE telemetry transport with custom GATT service and notification characteristic
- Wire format v1 (20 bytes) and v2 (24 bytes with `cycle_count`) — backward compatible
- Compile-time transport backend selection via Kconfig (BLE or mock)
- Dual output: serial printk + BLE notifications (when `CONFIG_BATTERY_TRANSPORT=y`)
- Charge cycle counter with NVS flash persistence (CHARGING→CHARGED transitions)
- Python BLE gateway with auto-reconnect (bleak) → InfluxDB 2.x → Grafana dashboard
- Docker Compose cloud stack: InfluxDB time-series storage + 11-panel Grafana dashboard
- `ibattery-gateway` CLI: scan, stream, run, analytics (health, anomalies, rul, cycles)
- Real-time and historical anomaly detection (voltage, temperature, SoC inconsistency)
- Battery health scoring, remaining useful life (RUL) estimation, cycle analysis
- Full battery state machine: ACTIVE, IDLE (30s), SLEEP (120s), CRITICAL, CHARGING, DISCHARGING, CHARGED
- TP4056 charger IC integration via GPIO (Kconfig-gated: `CONFIG_BATTERY_CHARGER_TP4056`)
- Host-based unit tests (Unity framework, 11 C test suites + 58 Python tests)

---

## Quick Start

### Build firmware

```bash
# nRF52840-DK
west build -b nrf52840dk/nrf52840 app -d build-nrf --pristine
west flash -d build-nrf

# NUCLEO-L476RG (STM32, no BLE)
west build -b nucleo_l476rg app -d build-stm32 --pristine -- \
  -DZEPHYR_EXTRA_MODULES="/opt/nordic/ncs/v3.2.2/modules/hal/stm32"
west flash -d build-stm32

# NUCLEO-L476RG with BLE shield (X-NUCLEO-IDB05A2)
west build -b nucleo_l476rg app -d build-stm32-ble --pristine -- \
  -DSHIELD=x_nucleo_idb05a1 \
  -DEXTRA_CONF_FILE=boards/nucleo_l476rg_ble.conf \
  -DZEPHYR_EXTRA_MODULES="/opt/nordic/ncs/v3.2.2/modules/hal/stm32"
```

### Run unit tests (host, no hardware needed)

```bash
cmake -B build_tests tests
cmake --build build_tests
ctest --test-dir build_tests --output-on-failure
```

### Cloud telemetry (BLE → InfluxDB → Grafana)

```bash
cd cloud && docker compose up -d          # Start InfluxDB + Grafana
cd gateway && pip install -e .            # Install Python gateway
ibattery-gateway run                      # Connect to iBattery device → write to InfluxDB
```

Open http://localhost:3000 → "iBattery Telemetry" dashboard with live voltage, temperature, SoC, and power state panels.

### Serial output

```
Battery SDK initialized OK
[v2 t=11]   V=3019 mV T=30.89 C SOC=100.00% PWR=6 CYC=0 flags=0x00000000
[v2 t=2030] V=3014 mV T=31.00 C SOC=100.00% PWR=6 CYC=0 flags=0x00000000
[v2 t=4049] V=3014 mV T=30.82 C SOC=100.00% PWR=6 CYC=0 flags=0x00000000
```

---

## Telemetry Packet Format

### v1 (20 bytes)

| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 0 | `telemetry_version` | `uint8_t` | Protocol version (1 or 2) |
| 1 | `timestamp_ms` | `uint32_t` | Uptime in milliseconds |
| 5 | `voltage_mv` | `int32_t` | Filtered battery voltage in mV |
| 9 | `temperature_c_x100` | `int32_t` | Temperature in 0.01 C units |
| 13 | `soc_pct_x100` | `uint16_t` | State of charge in 0.01% units |
| 15 | `power_state` | `uint8_t` | Power state enum |
| 16 | `status_flags` | `uint32_t` | Per-field error bits |

### v2 (24 bytes, extends v1)

| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 20 | `cycle_count` | `uint32_t` | Charge cycle count (NVS-persisted) |

---

## Repository Structure

```
ibattery-sdk/
  app/                          Zephyr application (main.c, CMake, overlay)
  include/battery_sdk/          Public SDK headers
  src/
    core/                       SDK init, internal state
    core_modules/               Voltage, temperature, ADC, filter
    hal/                        Hardware abstraction (Zephyr/nRF)
    intelligence/               SoC estimation, LUT
    telemetry/                  Telemetry collection
    transport/                  Wire serialization + BLE backend
  tests/                        Host-based unit tests (Unity)
    mocks/                      Configurable test doubles
  gateway/                      Python BLE gateway (bleak → InfluxDB)
  cloud/                        Docker Compose (InfluxDB + Grafana)
  docs/                         Documentation
```

---

## Documentation

- [SDK API Reference](SDK_API.md)
- [Architecture](ARCHITECTURE.md)
- [Hardware Wiring Guide](WIRING.md)
- [Battery Profiles](BATTERY_PROFILES.md)
- [Testing Guide](TESTING.md)
- [Release Notes](RELEASE_NOTES.md)
- [Roadmap & Strategy](ROADMAP.md)

---

## Contributing

See [Contributing](contributing.md) for development setup, code style, and how to submit changes.

---

## License

Licensed under the [Apache License 2.0](LICENSE).

Copyright 2026 Aliaksandr Liapin
