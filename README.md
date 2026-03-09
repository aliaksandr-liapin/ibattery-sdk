# Battery SDK

Battery SDK is an embedded firmware library designed to provide a **standardized battery intelligence layer** for embedded devices.

The project targets battery-powered IoT devices and aims to evolve into a **Battery Intelligence Platform** capable of providing diagnostics, telemetry, and predictive analytics for batteries.

---

# Vision

Battery SDK will evolve from a firmware component into a full platform:

Battery Hardware  
↓  
Embedded Battery SDK  
↓  
Battery Telemetry Protocol  
↓  
Battery Cloud Platform  
↓  
AI Battery Diagnostics  
↓  
Battery Developer Ecosystem

---

# Current Status

Phase 1 is currently in progress.

### Completed Steps

| Step | Description | Status |
|-----|-------------|------|
| Phase 0 | Hardware setup, firmware skeleton | ✅ Complete |
| Phase 1 Step 1 | SDK skeleton and architecture | ✅ Complete |
| Phase 1 Step 2 | Real ADC hardware integration | ✅ Complete |
| Phase 1 Step 3 | Voltage filtering | ✅ Complete |

---

# Hardware Platform

Current development hardware:

- **nRF52840 DK**
- **Zephyr RTOS**
- **SAADC driver**

---

# Voltage Measurement Pipeline
Application
↓
Battery SDK API
↓
battery_voltage
↓
battery_voltage_filter
↓
battery_adc
↓
battery_hal
↓
Zephyr ADC driver
↓
nRF52840 SAADC


---

# Current Features

✔ Hardware ADC integration  
✔ Voltage filtering (moving average)  
✔ Clean HAL abstraction  
✔ Modular SDK architecture  
✔ Telemetry framework skeleton  
✔ SOC estimation placeholder

---

# Example Runtime Output

Example terminal output on nRF52840 DK:
Voltage: 144 mV
Voltage: 148 mV
Voltage: 150 mV
Voltage: 147 mV
Temperature read failed
SOC: 0.00 %
Battery SDK skeleton alive...


Voltage readings are stabilized using a moving average filter.

---

# Repository Structure
battery-sdk/

app/
boards/

include/battery_sdk/

src/
core/
core_modules/
hal/
intelligence/
telemetry/

docs/
tests/


---

# Development Roadmap

See:
docs/DEVELOPMENT_ROADMAP.md


---

# License

TBD