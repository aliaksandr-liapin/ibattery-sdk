# LinkedIn Post

I'm excited to share a project I've been building: **iBattery SDK** — an open-source embedded C library that adds battery intelligence to IoT devices.

**What it does:**
🔋 Voltage measurement → SoC estimation → temperature monitoring → power state management → BLE telemetry → Grafana dashboard

**Runs on 3 platforms** (all hardware-validated):
• nRF52840-DK (Nordic)
• NUCLEO-L476RG (STM32)
• ESP32-C3 DevKitM (Espressif)

Same codebase, same API — just different board config files. The HAL abstraction is clean enough that adding a new MCU is ~200 lines of config, zero core code changes.

**Technical highlights:**
• ~48 bytes static RAM, integer-only math, no heap
• CR2032 + LiPo chemistry support
• Full pipeline: BLE → Python gateway → InfluxDB → Grafana
• 69 automated tests, CI green
• Apache 2.0 license

This started as a learning project and grew into something I think the embedded community could actually use. Every battery-powered IoT project reimplements the same things — this SDK packages it all into a portable library.

🔗 https://github.com/aliaksandr-liapin/ibattery-sdk

Feedback welcome! I'm especially interested in what platforms or features people would want next.

#embedded #IoT #opensource #firmware #bluetooth #zephyr #nrf52840 #stm32 #esp32 #batterymanagement
