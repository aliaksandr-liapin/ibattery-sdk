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