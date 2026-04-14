# Post to r/esp32

**Title:** Open-source battery monitoring SDK now supports ESP32-C3 — BLE telemetry to Grafana dashboard

**Body:**

Just shipped ESP32-C3 support for my battery intelligence SDK. It's an embedded C library (Zephyr RTOS) that turns any MCU into a battery monitor with BLE telemetry streaming to a Grafana dashboard.

**What you get on ESP32-C3:**
- Battery voltage via external divider (2x 100K resistors on GPIO2)
- Die temperature sensor (built-in)
- SoC estimation (CR2032 + LiPo lookup tables)
- Native BLE — no shield needed
- Power state machine (ACTIVE → IDLE → SLEEP with configurable timeouts)
- Full pipeline: ESP32-C3 → BLE → Python gateway → InfluxDB → Grafana

Validated on ESP32-C3-DevKitM-1-N4X. Build size: 356 KB flash, 138 KB RAM.

Also supports nRF52840 and STM32L476 — same codebase, same API, just different board config files.

GitHub: https://github.com/aliaksandr-liapin/ibattery-sdk

Apache 2.0 license, 69 tests, CI passing. Wiring diagrams in docs/WIRING.md.
