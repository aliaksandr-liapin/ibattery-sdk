---
title: How I Built a Portable Battery SDK That Runs on 3 MCU Platforms
published: true
tags: embedded, iot, opensource, bluetooth
cover_image: (attach your Grafana dashboard screenshot)
---

Every battery-powered IoT project ends up reimplementing the same things: ADC voltage reading, SoC estimation, power state management, temperature monitoring. After doing this for the third time, I built a reusable SDK that handles all of it.

## What is iBattery SDK?

It's an open-source C library (Apache 2.0) that provides a standardized battery intelligence layer for embedded devices running Zephyr RTOS. You init it with one call, and it gives you:

- **Voltage measurement** with moving average filtering
- **State-of-Charge estimation** via lookup table interpolation (CR2032 + LiPo)
- **Temperature monitoring** (on-chip die sensor or external NTC thermistor)
- **Power state machine** (ACTIVE → IDLE → SLEEP → CRITICAL, with charger integration)
- **BLE telemetry** streaming to a Python gateway → InfluxDB → Grafana dashboard

The entire core uses ~48 bytes of static RAM, integer-only math, and zero heap allocation.

## The Portability Challenge

The interesting part was making it run on 3 very different MCUs without changing any core code.

### The HAL Abstraction

All platform-specific code lives behind a Hardware Abstraction Layer. The core modules only call HAL functions — they never include vendor headers. Porting to a new platform means implementing 5-6 HAL functions and adding board config files.

### Three Different VDD Strategies

Each MCU reads battery voltage differently:

**nRF52840** — reads VDD directly via the SAADC internal input. Simplest approach: configure gain and reference voltage, read the ADC.

**STM32L476** — can't read VDD directly. Uses VREFINT: an internal ~1.21V bandgap reference. The ADC measures VREFINT against VDDA, and factory calibration data in ROM lets you back-calculate VDD.

**ESP32-C3** — also can't read VDD. Uses an external voltage divider: two 100K resistors split the battery voltage in half, and the ADC reads the midpoint. Firmware multiplies by 2.

All three strategies are hidden behind the same `battery_hal_adc_read_raw()` / `battery_hal_adc_raw_to_pin_mv()` interface. The temperature module, SoC estimator, and telemetry layer don't know or care which MCU they're running on.

### Platform Config in One Header

The `battery_adc_platform.h` header is the master switch:

```c
#if defined(CONFIG_SOC_SERIES_NRF52X)
    // Direct SAADC, 1/6 gain, 0.6V reference
#elif defined(CONFIG_SOC_SERIES_STM32L4X)
    // VREFINT sensor, factory calibration
#elif defined(CONFIG_SOC_SERIES_ESP32C3)
    // Voltage divider, 12dB attenuation
#endif
```

Adding a fourth platform is ~50 lines of config in this header plus a board overlay and Kconfig file. No C code changes needed.

## The Full Pipeline

The SDK doesn't stop at the firmware. There's a complete telemetry pipeline:

```
MCU (BLE GATT notifications, 24-byte packets every 2s)
  → Python gateway (bleak library, auto-reconnect)
    → InfluxDB 2.x (time-series storage)
      → Grafana (11-panel dashboard)
```

The gateway includes real-time anomaly detection, battery health scoring, remaining useful life estimation, and charge cycle analysis — all accessible via CLI.

## Build Matrix

| Platform | Flash | RAM | BLE |
|----------|-------|-----|-----|
| nRF52840-DK | 152 KB | 30 KB | Native |
| NUCLEO-L476RG | 38 KB | 10 KB | Shield |
| ESP32-C3 DevKitM | 356 KB | 138 KB | Native |

## Getting Started

```bash
# Clone
git clone https://github.com/aliaksandr-liapin/ibattery-sdk.git

# Run tests (no hardware needed)
cd ibattery-sdk
cmake -B build_tests tests && cmake --build build_tests
ctest --test-dir build_tests --output-on-failure
# 11 C test suites pass

# Build for nRF52840
west build -b nrf52840dk/nrf52840 app

# Start the cloud stack
cd cloud && docker compose up -d
cd gateway && pip install -e .
ibattery-gateway run
# Open http://localhost:3000 for Grafana dashboard
```

## What I Learned

1. **HAL design matters more than you think.** Getting the abstraction boundary right on the first platform (nRF52840) made the STM32 and ESP32 ports almost trivial.

2. **Integer-only math is worth the constraint.** No FPU dependency means the code runs identically on Cortex-M4F, Cortex-M4, and RISC-V without any float promotion surprises.

3. **Zephyr's devicetree is painful but powerful.** Once you understand overlays and Kconfig, adding a new board is mostly config — not code.

4. **Test on host, validate on hardware.** 69 tests run in under 1 second on macOS without any embedded toolchain. Hardware validation is the final step, not the first.

## Links

- **GitHub**: [github.com/aliaksandr-liapin/ibattery-sdk](https://github.com/aliaksandr-liapin/ibattery-sdk)
- **Wiring Guide**: [docs/WIRING.md](https://github.com/aliaksandr-liapin/ibattery-sdk/blob/main/docs/WIRING.md)
- **API Reference**: [docs/SDK_API.md](https://github.com/aliaksandr-liapin/ibattery-sdk/blob/main/docs/SDK_API.md)
- **Architecture**: [docs/ARCHITECTURE.md](https://github.com/aliaksandr-liapin/ibattery-sdk/blob/main/docs/ARCHITECTURE.md)

Feedback welcome — especially on what platforms or features you'd want next. ESP32-S3 and STM32WB are on the radar.
