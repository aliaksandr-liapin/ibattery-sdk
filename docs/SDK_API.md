# SDK API Reference

All public headers are in `include/battery_sdk/`. Include them as `<battery_sdk/header.h>`.

All functions return `int` using the `battery_status.h` error codes. Output values are written through pointer parameters.

---

## battery_sdk.h — SDK Lifecycle & Utilities

```c
#include <battery_sdk/battery_sdk.h>

int battery_sdk_init(void);
int battery_sdk_get_uptime_ms(uint32_t *uptime_ms_out);
```

### `battery_sdk_init`

Initialize all Battery SDK subsystems. Must be called once before using any other SDK function.

Initializes subsystems in dependency order:
1. HAL (ADC hardware)
2. Voltage (ADC wrapper + filter)
3. Temperature
4. Charger (if `CONFIG_BATTERY_CHARGER_TP4056`)
5. Power manager
6. SoC estimator
7. Cycle counter (loads NVS count)
8. Telemetry
9. Transport (if `CONFIG_BATTERY_TRANSPORT`)

**Returns:** `BATTERY_STATUS_OK` on success, or the first error encountered (initialization stops at the first failure).

### `battery_sdk_get_uptime_ms`

Get system uptime in milliseconds. Provides a platform-independent timestamp via the HAL.

| Parameter | Direction | Description |
|-----------|-----------|-------------|
| `uptime_ms_out` | out | System uptime in milliseconds |

**Returns:** `BATTERY_STATUS_OK`, `BATTERY_STATUS_INVALID_ARG` (NULL pointer), or `BATTERY_STATUS_IO`.

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

### `battery_temperature_init`

Initialize the temperature measurement subsystem. Configures the selected temperature sensor via the HAL. Called automatically by `battery_sdk_init()`.

The sensor source is selected at compile time via Kconfig:

| Config | Sensor | Description |
|--------|--------|-------------|
| `CONFIG_BATTERY_TEMP_NTC=y` | External 10K NTC thermistor (B=3950) | nRF52: AIN1 (P0.03); STM32: PA0 (A0); ESP32-C3: GPIO3. Voltage divider with 10K pullup |
| `CONFIG_BATTERY_TEMP_DIE=y` | On-chip die temperature sensor | nRF52: TEMP peripheral (±2 °C); STM32: internal temp sensor (±1.5 °C); ESP32-C3: coretemp sensor |

Default: NTC on nRF52840-DK, die temp on NUCLEO-L476RG and ESP32-C3 DevKitM (set per-board in `app/boards/<board>.conf`).

Both sensors use the same HAL interface — modules above the HAL are unchanged regardless of which sensor or platform is selected.

**Returns:** `BATTERY_STATUS_OK` or `BATTERY_STATUS_IO` if the sensor is not ready.

### `battery_temperature_get_c_x100`

Read the current temperature.

| Parameter | Direction | Description |
|-----------|-----------|-------------|
| `temperature_c_x100` | out | Temperature in 0.01 C units (e.g., 2350 = 23.50 C) |

**NTC mode**: Reads ADC (nRF52: SAADC AIN1; STM32: ADC1 Ch5; ESP32-C3: ADC1 Ch3/GPIO3), converts millivolts to NTC resistance via voltage divider math, then interpolates through a 16-point resistance-to-temperature lookup table (-40 °C to +125 °C). All integer math.

**Die sensor mode**: Reads the on-chip die temperature sensor via Zephyr sensor API. Accuracy: ±2 °C (nRF52840), ±1.5 °C (STM32L476), ~1 °C (ESP32-C3). Note: measures chip temperature, not ambient or battery temperature.

**Returns:** `BATTERY_STATUS_OK`, `BATTERY_STATUS_INVALID_ARG` (NULL pointer), `BATTERY_STATUS_NOT_INITIALIZED`, or `BATTERY_STATUS_IO`.

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

Internally reads the current voltage via `battery_voltage_get_mv()` and interpolates through the active lookup table.

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

**LiPo 1S discharge curve (11 points):**

| Voltage (mV) | SoC (%) |
|---------------|---------|
| 4200 | 100 |
| 4100 | 90 |
| 4000 | 80 |
| 3950 | 70 |
| 3870 | 55 |
| 3820 | 40 |
| 3780 | 30 |
| 3745 | 20 |
| 3700 | 15 |
| 3600 | 6.5 |
| 3000 | 0 |

Voltages above 4200 mV clamp to 100%. Voltages below 3000 mV clamp to 0%. Extra density in the knee/cliff regions below 3700 mV minimises interpolation error.

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
    BATTERY_POWER_STATE_UNKNOWN     = 0,
    BATTERY_POWER_STATE_ACTIVE      = 1,
    BATTERY_POWER_STATE_IDLE        = 2,  // Inactivity timeout (30s)
    BATTERY_POWER_STATE_SLEEP       = 3,  // Deep inactivity timeout (120s)
    BATTERY_POWER_STATE_CRITICAL    = 4,  // Voltage below critical threshold
    BATTERY_POWER_STATE_CHARGING    = 5,  // Charger connected, charging
    BATTERY_POWER_STATE_DISCHARGING = 6,  // On battery, charger not connected
    BATTERY_POWER_STATE_CHARGED     = 7,  // Charger connected, charge complete
};
```

### `battery_power_manager_init`

Initialize the power state monitor. Sets the initial state to ACTIVE. Called automatically by `battery_sdk_init()`.

**Returns:** `BATTERY_STATUS_OK`.

### `battery_power_manager_get_state`

Get the current power state based on voltage thresholds with hysteresis.

| Parameter | Direction | Description |
|-----------|-----------|-------------|
| `state` | out | Current power state |

The power manager reads the current voltage via `battery_voltage_get_mv()` and applies a hysteresis state machine:

| Transition | Condition |
|------------|-----------|
| ACTIVE → CRITICAL | Voltage drops below 2100 mV |
| CRITICAL → ACTIVE | Voltage rises above 2200 mV |

The 100 mV dead band between enter (2100 mV) and exit (2200 mV) thresholds prevents oscillation when voltage hovers near the boundary.

**Graceful degradation:** If the voltage read fails, the last known state is returned with `BATTERY_STATUS_OK` (the failure is not propagated).

**Inactivity timers:** ACTIVE → IDLE after 30s, IDLE → SLEEP after 120s. Reset via `battery_power_manager_report_activity()`.

**Charger integration:** When `CONFIG_BATTERY_CHARGER_TP4056=y`, reads CHRG/STDBY GPIO pins to detect CHARGING/CHARGED states. Charger state overrides inactivity logic. CRITICAL → CHARGING recovery when charger connected at low voltage.

**Priority order:** CRITICAL > charger state > inactivity > default (ACTIVE or DISCHARGING).

**Returns:** `BATTERY_STATUS_OK`, `BATTERY_STATUS_INVALID_ARG` (NULL pointer), or `BATTERY_STATUS_NOT_INITIALIZED`.

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
    uint8_t  telemetry_version;     // BATTERY_TELEMETRY_VERSION (2)
    uint32_t timestamp_ms;          // Uptime in ms
    int32_t  voltage_mv;            // Filtered voltage in mV
    int32_t  temperature_c_x100;    // Temperature in 0.01 C
    uint16_t soc_pct_x100;          // SoC in 0.01%
    uint8_t  power_state;           // battery_power_state enum
    uint32_t status_flags;          // Error flag bits
    uint32_t cycle_count;           // Charge cycle count (v2, NVS-persisted)
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

## battery_transport.h — Telemetry Transport

```c
#include <battery_sdk/battery_transport.h>

int battery_transport_init(void);
int battery_transport_send(const struct battery_telemetry_packet *packet);
int battery_transport_deinit(void);
int battery_transport_is_connected(bool *connected_out);
```

Available when `CONFIG_BATTERY_TRANSPORT=y`. The active backend is selected at compile time via Kconfig.

### `battery_transport_init`

Initialize the transport subsystem. Calls the active backend's init function. For BLE, this enables the Bluetooth stack, registers the GATT service, and starts connectable advertising.

**Returns:** `BATTERY_STATUS_OK`, `BATTERY_STATUS_IO`, or `BATTERY_STATUS_UNSUPPORTED` (no backend compiled in).

### `battery_transport_send`

Serialize and send a telemetry packet. Packs the packet into a wire buffer (v1: 20 bytes, v2: 24 bytes, v3: 32 bytes) and forwards it to the active backend.

| Parameter | Direction | Description |
|-----------|-----------|-------------|
| `packet` | in | Telemetry packet to send (must not be NULL) |

**Wire format v1 (20 bytes, little-endian):**

| Offset | Size | Field | Encoding |
|--------|------|-------|----------|
| 0 | 1 | telemetry_version | uint8 |
| 1 | 4 | timestamp_ms | uint32 LE |
| 5 | 4 | voltage_mv | int32 LE |
| 9 | 4 | temperature_c_x100 | int32 LE |
| 13 | 2 | soc_pct_x100 | uint16 LE |
| 15 | 1 | power_state | uint8 |
| 16 | 4 | status_flags | uint32 LE |

**Wire format v2 (24 bytes, extends v1):**

| Offset | Size | Field | Encoding |
|--------|------|-------|----------|
| 20 | 4 | cycle_count | uint32 LE |

**Wire format v3 (32 bytes, extends v2):**

| Offset | Size | Field | Encoding |
|--------|------|-------|----------|
| 24 | 4 | current_ma_x100 | int32 LE |
| 28 | 4 | coulomb_mah_x100 | int32 LE |

v3 is the current default when `CONFIG_BATTERY_CURRENT_SENSE=y` (`BATTERY_TELEMETRY_VERSION=3`). The decoder accepts v1, v2, and v3 for backward compatibility.

**BLE behavior:** When no client is subscribed (CCCD not enabled), the send silently succeeds (drop policy). The wire buffer is always updated for the Read characteristic regardless of subscription state.

**Returns:** `BATTERY_STATUS_OK`, `BATTERY_STATUS_INVALID_ARG` (NULL packet), `BATTERY_STATUS_UNSUPPORTED`, or `BATTERY_STATUS_IO`.

### `battery_transport_deinit`

Shut down the transport subsystem. For BLE, stops advertising.

**Returns:** `BATTERY_STATUS_OK` or `BATTERY_STATUS_UNSUPPORTED`.

### `battery_transport_is_connected`

Query whether a remote client is connected.

| Parameter | Direction | Description |
|-----------|-----------|-------------|
| `connected_out` | out | true if a BLE client is connected |

**Returns:** `BATTERY_STATUS_OK`, `BATTERY_STATUS_INVALID_ARG` (NULL pointer), or `BATTERY_STATUS_UNSUPPORTED`.

### BLE Configuration (Kconfig)

| Config | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_BATTERY_TRANSPORT` | bool | n | Enable transport subsystem |
| `CONFIG_BATTERY_TRANSPORT_BLE` | bool | — | BLE GATT backend |
| `CONFIG_BATTERY_BLE_DEVICE_NAME` | string | "iBattery" | Advertised device name |
| `CONFIG_BATTERY_BLE_ADV_INTERVAL_MS` | int | 1000 | Advertising interval (20-10240 ms) |

### BLE Service Details

| Property | Value |
|----------|-------|
| Service UUID | `12340001-5678-9ABC-DEF0-123456789ABC` |
| Characteristic UUID | `12340002-5678-9ABC-DEF0-123456789ABC` |
| Properties | Read + Notify |
| Permissions | Read (no authentication) |
| Max connections | 1 (configurable via `CONFIG_BT_MAX_CONN`) |

---

## battery_cycle_counter.h — Charge Cycle Counter

```c
#include <battery_sdk/battery_cycle_counter.h>

int battery_cycle_counter_init(void);
int battery_cycle_counter_update(uint8_t power_state);
int battery_cycle_counter_get(uint32_t *count_out);
```

### `battery_cycle_counter_init`

Initialize the cycle counter. Loads the persisted cycle count from NVS flash storage (starts at 0 if no stored value exists). Called automatically by `battery_sdk_init()`.

**Returns:** `BATTERY_STATUS_OK` on success.

### `battery_cycle_counter_update`

Notify the cycle counter of a power state change. Call this every telemetry loop. The counter internally tracks the previous state and increments when a CHARGING → CHARGED transition is detected. The new count is persisted to NVS flash.

| Parameter | Direction | Description |
|-----------|-----------|-------------|
| `power_state` | in | Current power state (enum `battery_power_state` value) |

**Returns:** `BATTERY_STATUS_OK` on success.

### `battery_cycle_counter_get`

Get the current charge cycle count.

| Parameter | Direction | Description |
|-----------|-----------|-------------|
| `count_out` | out | Number of completed charge cycles |

**Returns:** `BATTERY_STATUS_OK`, `BATTERY_STATUS_INVALID_ARG` (NULL pointer).

---

## battery_hal_current.h — Current Measurement

```c
#include <battery_sdk/battery_hal_current.h>

int battery_hal_current_init(void);
int battery_hal_current_read_ma_x100(int32_t *current_ma_x100_out);
```

Available when `CONFIG_BATTERY_CURRENT_SENSE=y`. Reads the INA219 current sensor via Zephyr sensor API.

### `battery_hal_current_init`

Initialize the current sensor HAL. Binds to the INA219 devicetree node and validates the device is ready. Called automatically by `battery_sdk_init()` when current sensing is enabled.

**Returns:** `BATTERY_STATUS_OK`, or `BATTERY_STATUS_IO` if the device is not found or not ready.

### `battery_hal_current_read_ma_x100`

Read the current flowing through the INA219 shunt resistor.

| Parameter | Direction | Description |
|-----------|-----------|-------------|
| `current_ma_x100_out` | out | Current in 0.01 mA units (positive = discharge, negative = charge) |

**Returns:** `BATTERY_STATUS_OK`, `BATTERY_STATUS_INVALID_ARG` (NULL pointer), `BATTERY_STATUS_NOT_INITIALIZED`, or `BATTERY_STATUS_IO`.

---

## battery_coulomb.h — Coulomb Counter

```c
#include <battery_sdk/battery_coulomb.h>

int battery_coulomb_init(void);
int battery_coulomb_update(int32_t current_ma_x100, uint32_t dt_ms);
int battery_coulomb_get_mah_x100(int32_t *mah_x100_out);
int battery_coulomb_reset(int32_t mah_x100);
```

Tracks accumulated charge via trapezoidal integration of current measurements. Requires `CONFIG_BATTERY_CURRENT_SENSE=y`.

### `battery_coulomb_init`

Initialize the coulomb counter. Loads persisted accumulator value from NVS flash (starts at 0 if no stored value). Called automatically by `battery_sdk_init()`.

**Returns:** `BATTERY_STATUS_OK` on success.

### `battery_coulomb_update`

Feed a new current measurement and time delta. The counter integrates using the trapezoidal rule: `(I_prev + I_curr) / 2 * dt`.

| Parameter | Direction | Description |
|-----------|-----------|-------------|
| `current_ma_x100` | in | Current in 0.01 mA units (from `battery_hal_current_read_ma_x100`) |
| `dt_ms` | in | Time since last update in milliseconds |

**Returns:** `BATTERY_STATUS_OK`, or `BATTERY_STATUS_NOT_INITIALIZED`.

### `battery_coulomb_get_mah_x100`

Get the accumulated charge since last reset.

| Parameter | Direction | Description |
|-----------|-----------|-------------|
| `mah_x100_out` | out | Accumulated charge in 0.01 mAh units |

**Returns:** `BATTERY_STATUS_OK`, `BATTERY_STATUS_INVALID_ARG` (NULL pointer).

### `battery_coulomb_reset`

Reset the coulomb counter accumulator to a known value. Used by the voltage-anchored SoC estimator to re-sync at voltage endpoints.

| Parameter | Direction | Description |
|-----------|-----------|-------------|
| `mah_x100` | in | New accumulator value in 0.01 mAh units |

**Returns:** `BATTERY_STATUS_OK`, or `BATTERY_STATUS_NOT_INITIALIZED`.

---

## Usage Example

```c
#include <battery_sdk/battery_sdk.h>
#include <battery_sdk/battery_status.h>
#include <battery_sdk/battery_telemetry.h>
#include <battery_sdk/battery_transport.h>  /* if CONFIG_BATTERY_TRANSPORT */

int main(void)
{
    struct battery_telemetry_packet pkt;

    int rc = battery_sdk_init();  /* initializes all subsystems incl. transport */
    if (rc != BATTERY_STATUS_OK) {
        /* handle init error */
    }

    while (1) {
        battery_telemetry_collect(&pkt);

        /* pkt.voltage_mv      — battery voltage in mV    */
        /* pkt.soc_pct_x100    — SoC in 0.01% units       */
        /* pkt.status_flags    — 0 if all readings OK      */

        /* Send via BLE (if transport enabled) */
        battery_transport_send(&pkt);

        sleep(2);
    }
}
```
