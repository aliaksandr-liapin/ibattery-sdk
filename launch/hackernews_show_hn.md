# Hacker News — Show HN

**Title:** Show HN: Open-source battery intelligence SDK for IoT (nRF52840, STM32, ESP32-C3)

**URL:** https://github.com/aliaksandr-liapin/ibattery-sdk

**Comment (post after submitting):**

I built this because every battery-powered IoT project ends up reimplementing the same things: ADC reading, voltage filtering, SoC estimation, power state management. This SDK packages all of that into a portable C library with a clean HAL abstraction.

It runs on 3 platforms (nRF52840, STM32L476, ESP32-C3) with zero core code changes between them. The HAL handles the differences: nRF reads VDD directly via SAADC, STM32 uses VREFINT factory calibration, ESP32 uses an external voltage divider.

The full pipeline goes from sensor readings through BLE GATT notifications to a Python gateway that writes to InfluxDB, displayed on an 11-panel Grafana dashboard. 69 tests, CI green, Apache 2.0.

Interesting technical bits:
- Integer-only math throughout (no FPU dependency)
- ~48 bytes static RAM for core modules
- Temperature-compensated SoC for LiPo cells
- Charge cycle counter with NVS flash persistence
