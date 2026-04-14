# Post to r/Zephyr_RTOS (or Zephyr Discord #general / #show-and-tell)

**Title:** Battery monitoring SDK for Zephyr — 3 platforms, BLE telemetry, Grafana dashboard

**Body:**

Sharing a Zephyr-based battery intelligence SDK I've been working on. It's a C library that provides voltage measurement, SoC estimation, temperature monitoring, and power state management — packaged as structured telemetry over BLE GATT.

**Key design decisions:**
- Clean HAL layer — porting to a new SoC is board overlay + Kconfig + ~50 lines in battery_adc_platform.h
- Integer-only math, no heap, ~48 bytes static RAM (core)
- Compile-time feature selection via Kconfig (chemistry, temp source, transport, charger)
- Wire format v2 (24 bytes LE) — BLE + serial dual output

**Validated on hardware:**
- nRF52840-DK (SAADC direct VDD, native BLE)
- NUCLEO-L476RG (VREFINT sensor, X-NUCLEO-IDB05A2 shield)
- ESP32-C3 DevKitM (voltage divider ADC, native BLE) — needs vanilla Zephyr, not NCS

Full pipeline: BLE → Python gateway (bleak) → InfluxDB → 11-panel Grafana dashboard.

GitHub: https://github.com/aliaksandr-liapin/ibattery-sdk

Would love feedback on the HAL abstraction pattern and any interest in additional platform ports. ESP32-S3 and STM32WB would be natural next steps.
