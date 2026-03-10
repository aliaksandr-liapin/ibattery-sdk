# Battery SDK

Embedded firmware library providing a **standardized battery intelligence layer** for battery-powered IoT devices. Measures voltage, estimates state-of-charge, monitors temperature and power state, and packages everything into structured telemetry packets.

Currently targets the **nRF52840** (Zephyr RTOS) with a **CR2032** coin cell. Designed to scale to other MCUs and battery chemistries.

---

## Current Status: Phase 2 Complete

| Phase | Description | Status |
|-------|-------------|--------|
| Phase 0 | Hardware setup, firmware skeleton | Done |
| Phase 1 | Core battery intelligence (voltage, SoC, telemetry) | Done |
| Phase 2 | Real temperature sensor + power state machine | **Done** |
| Phase 3 | Telemetry protocol and transport | Planned |
| Phase 4 | Cloud platform integration | Planned |
| Phase 5 | AI-driven battery analytics | Planned |

---

## Hardware

- **Board**: nRF52840-DK (PCA10056 rev 3.0.3)
- **Battery**: CR2032 Energizer (3V primary lithium)
- **Power switch**: VDD position
- **SDK**: nRF Connect SDK v3.2.2 / Zephyr OS v4.2.99
- **ADC**: SAADC measuring VDD rail, 12-bit, 1/6 gain, 0.6V internal reference (3.6V full-scale)

---

## Features

- Real voltage measurement via nRF52840 SAADC (VDD input)
- Moving average voltage filter (window=12, O(1), 28 bytes RAM)
- CR2032 voltage-to-SoC lookup table with linear interpolation (integer math only)
- Real die temperature sensor via nRF52840 TEMP peripheral (±2 °C accuracy)
- Voltage-threshold power state detection with 100 mV hysteresis (CRITICAL below 2100 mV)
- Resilient telemetry collection with per-field error flags
- Graceful degradation — power state survives voltage read failures
- Unified error codes (`battery_status.h`)
- Centralized SDK initialization (`battery_sdk_init()`)
- LiPo single-cell (3.7 V nominal) discharge curve LUT (11-point, extra density in knee region)
- Host-based unit tests (Unity framework, 59 tests across 5 suites, no Zephyr required)

---

## Quick Start

### Build firmware

```bash
cd app
west build -b nrf52840dk/nrf52840
west flash
```

### Run unit tests (host, no hardware needed)

```bash
cmake -B build_tests tests
cmake --build build_tests
ctest --test-dir build_tests --output-on-failure
```

### Serial output

```
Battery SDK initialized OK
[v1 t=250]  V=3017 mV T=24.50 C SOC=100.00% PWR=1 flags=0x00000000
[v1 t=2259] V=3016 mV T=24.50 C SOC=100.00% PWR=1 flags=0x00000000
[v1 t=4269] V=3015 mV T=25.00 C SOC=100.00% PWR=1 flags=0x00000000
```

---

## Telemetry Packet Format

| Field | Type | Description |
|-------|------|-------------|
| `telemetry_version` | `uint8_t` | Protocol version (currently 1) |
| `timestamp_ms` | `uint32_t` | Uptime in milliseconds |
| `voltage_mv` | `int32_t` | Filtered battery voltage in mV |
| `temperature_c_x100` | `int32_t` | Temperature in 0.01 C units |
| `soc_pct_x100` | `uint16_t` | State of charge in 0.01% units |
| `power_state` | `uint8_t` | Power state enum |
| `status_flags` | `uint32_t` | Per-field error bits |

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
  tests/                        Host-based unit tests (Unity)
    mocks/                      Configurable test doubles
  docs/                         Documentation
```

---

## Documentation

- [SDK API Reference](docs/SDK_API.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Testing Guide](docs/TESTING.md)
- [Release Notes](docs/RELEASE_NOTES.md)

---

## License

TBD
