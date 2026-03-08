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