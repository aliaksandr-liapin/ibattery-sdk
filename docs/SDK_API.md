# Battery SDK API Specification

This document describes the public SDK API exposed to applications.

Public headers are located in:

```
include/battery_sdk/
```

---

# Initialization

## battery_adc_init

Initializes the ADC subsystem.

```
int battery_adc_init(void);
```

---

## battery_voltage_init

Initializes the voltage measurement module.

```
int battery_voltage_init(void);
```

---

## battery_temperature_init

Initializes the temperature measurement module.

```
int battery_temperature_init(void);
```

---

## battery_soc_estimator_init

Initializes SOC estimation module.

```
int battery_soc_estimator_init(void);
```

---

## battery_power_manager_init

Initializes power state management.

```
int battery_power_manager_init(void);
```

---

## battery_telemetry_init

Initializes telemetry subsystem.

```
int battery_telemetry_init(void);
```

---

# Measurements

## battery_adc_read_raw

Reads raw ADC value.

```
int battery_adc_read_raw(int16_t *raw_value);
```

---

## battery_voltage_get_mv

Returns battery voltage in millivolts.

```
int battery_voltage_get_mv(int32_t *voltage_mv);
```

---

## battery_temperature_get_c_x100

Returns battery temperature in centi-degrees Celsius.

Example:

2500 = 25.00°C

```
int battery_temperature_get_c_x100(int32_t *temperature_c_x100);
```

---

## battery_soc_estimator_get_pct_x100

Returns battery state-of-charge.

Example:

8750 = 87.5%

```
int battery_soc_estimator_get_pct_x100(uint16_t *soc_pct_x100);
```

---

# Power State

## battery_power_manager_get_state

Returns current battery power state.

```
int battery_power_manager_get_state(enum battery_power_state *state);
```

---

# Telemetry

## battery_telemetry_collect

Collects telemetry into packet structure.

```
int battery_telemetry_collect(struct battery_telemetry_packet *packet);
```

---

# Telemetry Packet

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

# Error Codes

All SDK functions return:

```
BATTERY_STATUS_OK
BATTERY_STATUS_ERROR
BATTERY_STATUS_INVALID_ARG
BATTERY_STATUS_NOT_INITIALIZED
BATTERY_STATUS_IO
```

Defined in:

```
battery_status.h
```

---

Battery SDK API Specification

---

# Hardware Integration Status

## Voltage Measurement

The Battery SDK now supports real voltage measurements via the platform HAL.

Public API:

```c
int battery_voltage_init(void);
int battery_voltage_get_mv(int32_t *voltage_mv_out);

Source
Voltage readings originate from:
nRF52840 SAADC
via:
Zephyr ADC driver
through:
battery_hal_adc_zephyr.c

Measurement Flow
SAADC Hardware
↓
Zephyr ADC Driver
↓
HAL Layer
↓
battery_adc module
↓
battery_voltage module
↓
SDK API

Expected Output
Voltage: 3987 mV

Temperature Measurement
Temperature measurement is currently stubbed.
Current API:
int battery_temperature_get_c_x100(int32_t *temp_c_x100);

Planned future sources:
- MCU internal temperature sensor
- external thermistor
- battery pack temperature sensors

State-of-Charge Estimation
SOC estimation API:
int battery_soc_estimator_get_pct_x100(uint16_t *soc_pct_x100);

Current status:
SOC calculation not yet integrated with real voltage filtering.

SOC accuracy will improve after voltage filtering is introduced in Phase 1 Step 3.


---

# 3️⃣ Create `docs/DEVELOPMENT_ROADMAP.md`

This helps track phases clearly.

```markdown
# Battery SDK Development Roadmap

---

# Phase 0 — Hardware Bring-up

Completed.

Objectives:

- nRF52840 DK setup
- Zephyr RTOS environment
- toolchain validation
- firmware flashing pipeline

---

# Phase 1 — Embedded SDK Foundation

## Step 1 — SDK Architecture Skeleton

Completed.

Achievements:

- defined module boundaries
- implemented SDK skeleton
- verified firmware execution on hardware

Modules created:

- battery_adc
- battery_voltage
- battery_temperature
- battery_soc_estimator
- battery_power_manager
- battery_telemetry

---

## Step 2 — Real ADC Integration

Completed.

Achievements:

- integrated Zephyr ADC driver
- configured nRF52840 SAADC
- implemented HAL ADC layer
- replaced stub voltage measurement
- validated real voltage readings on hardware

This step confirms the SDK can interact with real hardware sensors.

---

## Step 3 — Voltage Stabilization (Next)

Objective:

Improve voltage measurement quality.

Implementation tasks:

- implement moving average filtering
- implement oversampling smoothing
- reduce ADC noise
- stabilize voltage readings for SOC estimation

Expected outcome:
Stable voltage telemetry suitable for battery analytics.


---

# Future Phases

## Phase 2 — Battery Intelligence Core

- advanced SOC estimation
- battery health metrics
- charging detection
- discharge modeling

---

## Phase 3 — Battery Telemetry Protocol

- telemetry data schema
- device-to-cloud protocol
- battery event logging

---

## Phase 4 — Battery Cloud Platform

- battery fleet monitoring
- anomaly detection
- predictive diagnostics

---

## Phase 5 — AI Battery Intelligence

- AI battery degradation prediction
- device battery optimization
- cloud-driven firmware updates

---

# Long-Term Vision

Battery SDK will evolve into the foundational firmware layer for the **Battery Intelligence Platform**.

Architecture evolution:
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

