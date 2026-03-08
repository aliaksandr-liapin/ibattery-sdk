# Battery SDK Architecture

This document describes the architecture of the Battery SDK embedded system.

The SDK is designed to support **large-scale battery telemetry systems** across millions or billions of devices.

---

# Architecture Principles

The system follows three mandatory rules.

## 1. Core vs Platform Separation

The battery core must never depend on specific hardware.

```
Battery Core
↓
HAL Layer
↓
Hardware Platform
```

The HAL layer isolates platform dependencies.

---

## 2. Stable Telemetry Contract

Battery telemetry structures must remain stable.

These structures will be used by:

Device firmware  
Telemetry transport layers  
Cloud ingestion systems  
AI analytics pipelines

---

## 3. Clean Public API

Public APIs are defined in:

```
include/battery_sdk/
```

Applications must only depend on these interfaces.

Internal implementation is located in:

```
src/
```

---

# System Architecture

```
Application
     ↓
Battery SDK Public API
     ↓
Battery Core Modules
     ↓
Hardware Abstraction Layer
     ↓
MCU Drivers / Zephyr
     ↓
Hardware
```

---

# SDK Modules

## battery_adc

Responsible for acquiring raw ADC measurements.

Responsibilities:

ADC initialization  
Raw ADC sampling  
Hardware abstraction

---

## battery_voltage

Converts raw ADC data into battery voltage measurements.

Responsibilities:

Voltage calculation  
Voltage filtering  
Calibration support

---

## battery_temperature

Provides battery temperature measurements.

Future sources:

Internal MCU temperature sensor  
External thermistor  
Fuel gauge IC

---

## battery_soc_estimator

Estimates battery state-of-charge.

Future improvements include:

Voltage curve modeling  
Temperature compensation  
Battery chemistry profiles

---

## battery_power_manager

Tracks battery power state.

Possible states:

Active  
Idle  
Sleep  
Critical

---

## battery_telemetry

Collects system battery telemetry and produces telemetry packets.

Telemetry includes:

Voltage  
Temperature  
SOC  
Power state  
Timestamp

---

# Telemetry Structure

```
struct battery_telemetry_packet
```

Fields:

```
telemetry_version
timestamp_ms
voltage_mv
temperature_c_x100
soc_pct_x100
power_state
status_flags
```

---

# Hardware Abstraction Layer

The HAL isolates platform dependencies.

Current implementation:

```
battery_hal_zephyr.c
```

Responsibilities:

ADC access  
Time services  
Hardware access abstraction

---

# Future Architecture Expansion

The embedded SDK will evolve to support:

Battery telemetry protocol  
BLE export  
Cloud ingestion  
AI diagnostics

---

Battery SDK Architecture Document

---

# Phase 1 — Hardware Integration Progress

## Step 1 — SDK Skeleton (Completed)

The Battery SDK architecture skeleton compiles and runs on the nRF52840 DK using Zephyr RTOS.

Implemented modules:

- battery_adc
- battery_voltage
- battery_temperature
- battery_soc_estimator
- battery_power_manager
- battery_telemetry

Terminal output confirmed SDK initialization:
---

# Phase 1 — Hardware Integration Progress

## Step 1 — SDK Skeleton (Completed)

The Battery SDK architecture skeleton compiles and runs on the nRF52840 DK using Zephyr RTOS.

Implemented modules:

- battery_adc
- battery_voltage
- battery_temperature
- battery_soc_estimator
- battery_power_manager
- battery_telemetry

Terminal output confirmed SDK initialization:
---

# Phase 1 — Hardware Integration Progress

## Step 1 — SDK Skeleton (Completed)

The Battery SDK architecture skeleton compiles and runs on the nRF52840 DK using Zephyr RTOS.

Implemented modules:

- battery_adc
- battery_voltage
- battery_temperature
- battery_soc_estimator
- battery_power_manager
- battery_telemetry

Terminal output confirmed SDK initialization:
Voltage: 2100 mV
Temperature: 25.00 C
SOC: 0.00 %
Battery SDK skeleton alive...


At this stage all values were stubbed.

---

## Step 2 — Real ADC Integration (Completed)

Phase 1 Step 2 replaced the stub ADC implementation with a real hardware-backed implementation using the **nRF52840 SAADC via Zephyr ADC driver**.

### Architecture Path
Application
↓
Battery SDK API
↓
Battery Core Modules
↓
HAL Layer
↓
Zephyr Drivers
↓
nRF52840 SAADC Hardware


### Implementation Location

HAL implementation:
src/hal/battery_hal_adc_zephyr.c

Voltage calculation:
src/core_modules/battery_voltage.c


### Devicetree Integration

Board overlay:
app/boards/nrf52840dk_nrf52840.overlay


### Hardware Input

ADC input pin:
AIN0 (P0.02)


### Validation

Firmware output now shows live voltage values:
Voltage: 206 mV
Voltage: 212 mV
Voltage: 218 mV


This confirms that:

- SAADC initialization works
- HAL integration works
- SDK voltage module uses real hardware data
- SDK architecture boundaries remain intact

### Notes

Low voltage values observed during testing were caused by a floating ADC input pin.

Proper measurements require either:
AIN0 → GND
or
AIN0 → VDD (3.3V)

or a battery connected through a resistor divider.

---

## Architecture Status

The Battery SDK architecture is now validated with real hardware signals.

The HAL abstraction successfully isolates platform-specific drivers from the SDK core modules.

This confirms the SDK architecture is scalable for future hardware platforms.
