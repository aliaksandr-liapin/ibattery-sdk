# Battery SDK Architecture

Battery SDK follows a **layered architecture** to ensure portability, modularity, and long-term scalability.

---

# Architecture Principles

The architecture is based on four mandatory rules.

### 1. Battery Core separated from Platform Layer

Hardware access must be isolated from battery logic.

### 2. Stable SDK API

The public API must remain stable for application developers.

### 3. Telemetry contract stability

Telemetry data structures must remain stable to support cloud integration.

### 4. HAL isolation

All hardware-specific logic must remain inside HAL.

---

# Layered Architecture
Application Layer
↓
Battery SDK API
↓
Battery Domain Modules
↓
HAL (Hardware Abstraction Layer)
↓
Platform Drivers
↓
Hardware


---

# Battery Measurement Flow

Voltage acquisition pipeline:
Application
↓
battery_voltage_get_mv()
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

# Module Overview

### Core Modules

| Module | Responsibility |
|------|------|
battery_adc | ADC acquisition wrapper |
battery_voltage | voltage access API |
battery_voltage_filter | signal conditioning |
battery_temperature | temperature acquisition |
battery_soc_estimator | SOC estimation |
battery_power_manager | battery state |
battery_telemetry | telemetry generation |

---

# HAL Responsibilities

HAL provides hardware-specific implementations.

Examples:
battery_hal_adc_init()
battery_hal_adc_read_raw()
battery_hal_adc_raw_to_pin_mv()


HAL must never contain battery business logic.

---

# Voltage Filtering

Implemented using a **moving average filter**.
V_filtered = (V1 + V2 + ... + VN) / N


Default window:
BATTERY_VOLTAGE_FILTER_DEFAULT_WINDOW_SIZE = 12


Design constraints:

- deterministic runtime
- no dynamic memory
- fixed-point math
- embedded safe

---

# Future Architecture

The architecture is designed to support expansion toward:
Battery SDK
↓
Battery Telemetry Protocol
↓
Battery Cloud Platform
↓
AI Battery Intelligence

