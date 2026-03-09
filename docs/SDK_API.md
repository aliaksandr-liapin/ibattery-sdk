# Battery SDK Public API

This document defines the public SDK API available to applications.

Public headers are located in:
include/battery_sdk/


---

# Voltage API

### Initialize Voltage System
int battery_voltage_init(void);


Initializes voltage acquisition and filter state.

---

### Get Battery Voltage
int battery_voltage_get_mv(uint16_t *voltage_mv_out);


Returns battery voltage in millivolts.

Notes:

- value is filtered internally
- uses moving average filter
- hardware access performed through HAL

---

# Temperature API
int battery_temperature_get_c_x100(int32_t *temperature_out);


Returns temperature in hundredths of a degree Celsius.

Example:
2534 → 25.34°C


Currently not implemented.

---

# SOC Estimator API
int battery_soc_estimator_get_pct_x100(uint16_t *soc_out);
Returns battery state-of-charge.

Format:
5000 → 50.00 %


Currently placeholder.

---

# Telemetry API
int battery_telemetry_collect(battery_telemetry_packet_t *packet);


Collects telemetry data from all battery subsystems.

Telemetry packet:
typedef struct
{
int32_t voltage_mv;
int32_t temperature_c_x100;
uint16_t soc_pct_x100;
} battery_telemetry_packet_t;


---

# Error Handling

Functions return standard error codes:

| Code | Meaning |
|----|----|
0 | success |
EINVAL | invalid parameter |
ENODEV | device unavailable |
ENOSYS | not implemented |

