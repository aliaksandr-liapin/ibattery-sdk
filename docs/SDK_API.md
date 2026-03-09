# SDK API Reference

All public headers are in `include/battery_sdk/`. Include them as `<battery_sdk/header.h>`.

All functions return `int` using the `battery_status.h` error codes. Output values are written through pointer parameters.

---

## battery_sdk.h — SDK Lifecycle

```c
#include <battery_sdk/battery_sdk.h>

int battery_sdk_init(void);
```

### `battery_sdk_init`

Initialize all Battery SDK subsystems. Must be called once before using any other SDK function.

Initializes subsystems in dependency order:
1. HAL (ADC hardware)
2. Voltage (ADC wrapper + filter)
3. Temperature
4. Power manager
5. SoC estimator
6. Telemetry

**Returns:** `BATTERY_STATUS_OK` on success, or the first error encountered (initialization stops at the first failure).

---

## battery_status.h — Error Codes

```c
#include <battery_sdk/battery_status.h>

enum battery_status {
    BATTERY_STATUS_OK              =  0,
    BATTERY_STATUS_ERROR           = -1,
    BATTERY_STATUS_INVALID_ARG     = -2,
    BATTERY_STATUS_NOT_INITIALIZED = -3,
    BATTERY_STATUS_UNSUPPORTED     = -4,
    BATTERY_STATUS_IO              = -5
};
```

All SDK functions return these values. The HAL translates platform-specific errors (Zephyr errno, nrfx codes) internally so callers never see platform errors.

| Code | Value | Meaning |
|------|-------|---------|
| `BATTERY_STATUS_OK` | 0 | Success |
| `BATTERY_STATUS_ERROR` | -1 | Generic / unclassified error |
| `BATTERY_STATUS_INVALID_ARG` | -2 | NULL pointer or out-of-range parameter |
| `BATTERY_STATUS_NOT_INITIALIZED` | -3 | Module not initialized |
| `BATTERY_STATUS_UNSUPPORTED` | -4 | Feature not available on this platform |
| `BATTERY_STATUS_IO` | -5 | Hardware / peripheral I/O failure |

---

## battery_voltage.h — Voltage Measurement

```c
#include <battery_sdk/battery_voltage.h>

int battery_voltage_init(void);
int battery_voltage_get_mv(uint16_t *voltage_mv_out);
```

### `battery_voltage_init`

Initialize the voltage measurement subsystem (ADC + moving average filter). Called automatically by `battery_sdk_init()`.

**Returns:** `BATTERY_STATUS_OK` or `BATTERY_STATUS_IO` if ADC setup fails.

### `battery_voltage_get_mv`

Read the current filtered battery voltage.

| Parameter | Direction | Description |
|-----------|-----------|-------------|
| `voltage_mv_out` | out | Filtered voltage in millivolts |

Each call triggers a new ADC sample, feeds it through the moving average filter, and writes the filtered result. Lazy-initializes if `battery_voltage_init()` was not called.

**Returns:** `BATTERY_STATUS_OK`, `BATTERY_STATUS_INVALID_ARG` (NULL pointer), or `BATTERY_STATUS_IO`.

---

## battery_temperature.h — Temperature

```c
#include <battery_sdk/battery_temperature.h>

int battery_temperature_init(void);
int battery_temperature_get_c_x100(int32_t *temperature_c_x100);
```

### `battery_temperature_get_c_x100`

Read the current temperature.

| Parameter | Direction | Description |
|-----------|-----------|-------------|
| `temperature_c_x100` | out | Temperature in 0.01 C units (e.g., 2500 = 25.00 C) |

**Note:** Current implementation returns a fixed 25.00 C stub. Real sensor integration is planned for Phase 2.

---

## battery_soc_estimator.h — State of Charge

```c
#include <battery_sdk/battery_soc_estimator.h>

int battery_soc_estimator_init(void);
int battery_soc_estimator_get_pct_x100(uint16_t *soc_pct_x100);
```

### `battery_soc_estimator_get_pct_x100`

Get the current estimated state of charge.

| Parameter | Direction | Description |
|-----------|-----------|-------------|
| `soc_pct_x100` | out | SoC in 0.01% units (0 = 0.00%, 10000 = 100.00%) |

Internally reads the current voltage via `battery_voltage_get_mv()` and interpolates through the CR2032 lookup table.

**CR2032 discharge curve (9 points):**

| Voltage (mV) | SoC (%) |
|---------------|---------|
| 3000 | 100 |
| 2900 | 90 |
| 2800 | 70 |
| 2700 | 50 |
| 2600 | 30 |
| 2500 | 20 |
| 2400 | 10 |
| 2200 | 5 |
| 2000 | 0 |

Voltages above 3000 mV clamp to 100%. Voltages below 2000 mV clamp to 0%.

---

## battery_power_manager.h — Power State

```c
#include <battery_sdk/battery_power_manager.h>

int battery_power_manager_init(void);
int battery_power_manager_get_state(enum battery_power_state *state);
```

### `battery_power_state` enum

```c
enum battery_power_state {
    BATTERY_POWER_STATE_UNKNOWN  = 0,
    BATTERY_POWER_STATE_ACTIVE   = 1,
    BATTERY_POWER_STATE_IDLE     = 2,
    BATTERY_POWER_STATE_SLEEP    = 3,
    BATTERY_POWER_STATE_CRITICAL = 4
};
```

### `battery_power_manager_get_state`

| Parameter | Direction | Description |
|-----------|-----------|-------------|
| `state` | out | Current power state |

**Note:** Current implementation always returns `BATTERY_POWER_STATE_ACTIVE`. Dynamic power state transitions are planned for Phase 2.

---

## battery_telemetry.h — Telemetry Collection

```c
#include <battery_sdk/battery_telemetry.h>

int battery_telemetry_init(void);
int battery_telemetry_collect(struct battery_telemetry_packet *packet);
```

### `battery_telemetry_collect`

Collect a complete telemetry snapshot from all subsystems.

| Parameter | Direction | Description |
|-----------|-----------|-------------|
| `packet` | out | Filled telemetry packet |

This function uses **best-effort collection**: it reads each subsystem independently and sets error flag bits in `status_flags` for any that fail. The function itself returns `BATTERY_STATUS_OK` as long as at least partial data was collected.

Failed fields are zeroed and their corresponding flag bit is set.

### `battery_telemetry_packet` struct

```c
struct battery_telemetry_packet {
    uint8_t  telemetry_version;     // BATTERY_TELEMETRY_VERSION (1)
    uint32_t timestamp_ms;          // Uptime in ms
    int32_t  voltage_mv;            // Filtered voltage in mV
    int32_t  temperature_c_x100;    // Temperature in 0.01 C
    uint16_t soc_pct_x100;          // SoC in 0.01%
    uint8_t  power_state;           // battery_power_state enum
    uint32_t status_flags;          // Error flag bits
};
```

### Status flag bits

| Flag | Bit | Meaning |
|------|-----|---------|
| `BATTERY_TELEMETRY_FLAG_VOLTAGE_ERR` | 0 | Voltage read failed |
| `BATTERY_TELEMETRY_FLAG_TEMP_ERR` | 1 | Temperature read failed |
| `BATTERY_TELEMETRY_FLAG_SOC_ERR` | 2 | SoC estimation failed |
| `BATTERY_TELEMETRY_FLAG_POWER_STATE_ERR` | 3 | Power state read failed |
| `BATTERY_TELEMETRY_FLAG_TIMESTAMP_ERR` | 4 | Timestamp read failed |

A `status_flags` value of `0x00000000` means all fields were collected successfully.

---

## Usage Example

```c
#include <battery_sdk/battery_sdk.h>
#include <battery_sdk/battery_status.h>
#include <battery_sdk/battery_telemetry.h>

int main(void)
{
    struct battery_telemetry_packet pkt;

    int rc = battery_sdk_init();
    if (rc != BATTERY_STATUS_OK) {
        /* handle init error */
    }

    while (1) {
        battery_telemetry_collect(&pkt);

        /* pkt.voltage_mv      — battery voltage in mV    */
        /* pkt.soc_pct_x100    — SoC in 0.01% units       */
        /* pkt.status_flags    — 0 if all readings OK      */

        sleep(2);
    }
}
```
