# Post to r/embedded

**Title:** I built an open-source battery intelligence SDK for IoT — runs on nRF52840, STM32, and ESP32-C3

**Body:**

Hey r/embedded,

I've been building an embedded C SDK that adds a standardized battery monitoring layer to IoT devices. It measures voltage, estimates state-of-charge, monitors temperature and power state, and streams structured telemetry over BLE.

**What it does:**
- Real voltage measurement → SoC estimation via lookup tables (CR2032 + LiPo)
- Die temperature + external NTC thermistor support
- Power state machine (ACTIVE/IDLE/SLEEP/CRITICAL/CHARGING)
- TP4056 charger integration with cycle counting
- BLE GATT telemetry → Python gateway → InfluxDB → Grafana dashboard
- ~48 bytes static RAM, integer-only math, no heap allocation

**Runs on 3 platforms (all hardware-validated):**
- nRF52840-DK (native BLE)
- NUCLEO-L476RG + X-NUCLEO-IDB05A2 BLE shield
- ESP32-C3 DevKitM (native BLE)

The HAL abstraction is clean enough that adding a new platform is ~200 lines of config — no core code changes needed.

**Stack:** Zephyr RTOS, C99, Apache 2.0 license.

GitHub: https://github.com/aliaksandr-liapin/ibattery-sdk

69 tests (11 C + 58 Python), CI green, full docs including wiring diagrams. Happy to answer questions about the architecture or porting process.
