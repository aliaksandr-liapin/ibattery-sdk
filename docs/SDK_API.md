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
4. Power manager
5. SoC estimator
6. Telemetry

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
| `CONFIG_BATTERY_TEMP_NTC=y` (default) | External 10K NTC thermistor (B=3950) | SAADC AIN1 (P0.03), voltage divider with 10K pullup |
| `CONFIG_BATTERY_TEMP_DIE=y` | nRF52840 on-chip die sensor | TEMP peripheral via Zephyr sensor API (±2 °C) |

Both sensors use the same HAL interface — modules above the HAL are unchanged regardless of which sensor is selected.

**Returns:** `BATTERY_STATUS_OK` or `BATTERY_STATUS_IO` if the sensor is not ready.

### `battery_temperature_get_c_x100`

Read the current temperature.

| Parameter | Direction | Description |
|-----------|-----------|-------------|
| `temperature_c_x100` | out | Temperature in 0.01 C units (e.g., 2350 = 23.50 C) |

**NTC mode** (default): Reads SAADC channel 1, converts ADC millivolts to NTC resistance via voltage divider math, then interpolates through a 16-point resistance-to-temperature lookup table (-40 °C to +125 °C). All integer math.

**Die sensor mode**: Reads the nRF52840 TEMP peripheral (±2 °C accuracy). Note: measures chip temperature, not ambient or battery temperature.

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
    BATTERY_POWER_STATE_UNKNOWN  = 0,
    BATTERY_POWER_STATE_ACTIVE   = 1,
    BATTERY_POWER_STATE_IDLE     = 2,
    BATTERY_POWER_STATE_SLEEP    = 3,
    BATTERY_POWER_STATE_CRITICAL = 4
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

**Note:** IDLE and SLEEP states are defined in the enum but not yet implemented (require Zephyr power management integration).

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

Serialize and send a telemetry packet. Packs the packet into a 20-byte little-endian wire buffer and forwards it to the active backend.

| Parameter | Direction | Description |
|-----------|-----------|-------------|
| `packet` | in | Telemetry packet to send (must not be NULL) |

**Wire format (20 bytes, little-endian):**

| Offset | Size | Field | Encoding |
|--------|------|-------|----------|
| 0 | 1 | telemetry_version | uint8 |
| 1 | 4 | timestamp_ms | uint32 LE |
| 5 | 4 | voltage_mv | int32 LE |
| 9 | 4 | temperature_c_x100 | int32 LE |
| 13 | 2 | soc_pct_x100 | uint16 LE |
| 15 | 1 | power_state | uint8 |
| 16 | 4 | status_flags | uint32 LE |

20 bytes fits within a single BLE ATT default MTU (23 − 3 overhead = 20).

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
