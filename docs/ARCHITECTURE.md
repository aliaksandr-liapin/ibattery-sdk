# Architecture

## Design Principles

1. **Layered isolation** — each layer depends only on the layer below it; no upward or lateral dependencies
2. **Platform abstraction** — all hardware access goes through the HAL; core logic is portable C
3. **Integer-only math** — no floating point anywhere in the SDK (safe for MCUs without FPU, deterministic)
4. **Fail-safe collection** — telemetry collects what it can and flags what it can't
5. **Testable in isolation** — core modules compile and test on a host machine without Zephyr or hardware

---

## Layer Diagram

```
+------------------------------------------------------+
|                    Application                        |
|                   (app/src/main.c)                    |
+------------------------------------------------------+
                         |
              battery_sdk_init()
              battery_telemetry_collect()
              battery_transport_send()        [optional]
                         |
+------------------------------------------------------+
|                  Public SDK API                       |
|              (include/battery_sdk/*.h)                |
|                                                       |
|  battery_sdk.h        battery_telemetry.h             |
|  battery_voltage.h    battery_temperature.h           |
|  battery_soc_estimator.h  battery_power_manager.h     |
|  battery_transport.h  battery_status.h                |
|  battery_types.h                                      |
+------------------------------------------------------+
                         |
+------------------------------------------------------+
|                 Domain Modules                        |
|                                                       |
|  core/           core_modules/       intelligence/    |
|    battery_sdk.c   battery_voltage.c   battery_soc_   |
|    battery_        battery_adc.c       estimator.c    |
|    internal.h      battery_voltage_    battery_soc_   |
|                    filter.c            lut.c          |
|                    battery_            battery_ntc_   |
|                    temperature.c       lut.c          |
|                    battery_power_                     |
|                    manager.c                          |
|                                                       |
|                 telemetry/                             |
|                   battery_telemetry.c                  |
|                                                       |
|                 transport/                             |
|                   battery_transport.c                  |
|                   battery_serialize.c/.h               |
|                   ble/                                 |
|                     battery_transport_ble_zephyr.c     |
+------------------------------------------------------+
                         |
+------------------------------------------------------+
|              Hardware Abstraction Layer                |
|                   (src/hal/)                           |
|                                                       |
|  battery_hal.h                  Interface (portable)     |
|  battery_hal_zephyr.c           Platform init + uptime   |
|  battery_hal_adc_zephyr.c       ADC channel setup + read |
|  battery_hal_temp_zephyr.c      Die temperature sensor   |
|  battery_hal_temp_ntc_zephyr.c  NTC thermistor (default) |
+------------------------------------------------------+
                         |
+------------------------------------------------------+
|                Platform Drivers                       |
|      Zephyr ADC API + nRF SAADC + Zephyr Sensor API  |
+------------------------------------------------------+
                         |
+------------------------------------------------------+
|                   Hardware                            |
|   nRF52840 SAADC Ch0 -> VDD rail -> CR2032/LiPo      |
|   nRF52840 SAADC Ch1 -> AIN1 (P0.03) -> NTC divider  |
|   nRF52840 TEMP peripheral (die temperature, opt.)    |
+------------------------------------------------------+
```

---

## Module Responsibilities

### core/battery_sdk.c
- Owns the SDK runtime state (`battery_sdk_runtime_state`)
- `battery_sdk_init()` calls each subsystem init in dependency order
- `battery_sdk_get_uptime_ms()` provides platform-independent timestamp (delegates to HAL)
- Tracks which modules are initialized via boolean flags

### core_modules/battery_voltage.c
- Orchestrates ADC reads through `battery_adc.c`
- Feeds raw readings through `battery_voltage_filter.c`
- Lazy-initializes on first call if not explicitly initialized

### core_modules/battery_voltage_filter.c
- Moving average filter with configurable window size (default 12)
- O(1) update: maintains running sum, circular buffer index
- Memory: `4 + 2*window + 2 + 4 + 4 = 28` bytes for window=12
- No heap allocation, no floating point

### core_modules/battery_temperature.c
- Reads die temperature through the temperature HAL
- `battery_temperature_init()` calls `battery_hal_temp_init()` to configure the sensor
- `battery_temperature_get_c_x100()` delegates to `battery_hal_temp_read_c_x100()`
- Returns temperature in 0.01 °C units (e.g., 2350 = 23.50 °C)

### core_modules/battery_power_manager.c
- Voltage-threshold state machine with hysteresis for power state detection
- Uses `battery_voltage_get_mv()` to read current voltage (lateral dependency, init-order safe)
- Enter CRITICAL when voltage drops below 2100 mV; exit CRITICAL when voltage rises above 2200 mV
- 100 mV hysteresis dead band prevents oscillation near threshold
- Graceful degradation: returns last known state if voltage read fails
- Static `g_current_state` maintains hysteresis memory across calls (+1 byte RAM)
- IDLE/SLEEP states deferred (require Zephyr power management integration)

### core_modules/battery_adc.c
- Thin wrapper around HAL ADC functions
- Reads raw ADC sample, converts to millivolts via HAL
- Hides HAL details from the voltage module

### intelligence/battery_soc_estimator.c
- Reads current voltage, passes to LUT interpolator
- Returns SoC in 0.01% units (0-10000)

### intelligence/battery_soc_lut.c
- CR2032 9-point voltage-to-SoC lookup table
- LiPo 1S 11-point voltage-to-SoC lookup table
- Linear interpolation between adjacent points
- Integer-only math: `soc = soc_high - (soc_high - soc_low) * (v_high - v) / (v_high - v_low)`
- Clamps at boundaries

### intelligence/battery_ntc_lut.c
- 16-point resistance-to-temperature LUT for 10K NTC (B=3950), covering -40 °C to +125 °C
- Resistance values computed from B-parameter equation: `R(T) = R0 * exp(B * (1/T - 1/T0))`
- `battery_ntc_resistance_from_mv()` — converts voltage divider ADC reading to NTC resistance
- `battery_ntc_lut_interpolate()` — linear interpolation between resistance-temperature entries
- Integer-only math using int64_t for negative temperature handling and uint64_t for overflow safety
- Table sorted descending by resistance (highest R / coldest first)

### telemetry/battery_telemetry.c
- Reads all subsystems in sequence: timestamp, voltage, temperature, SoC, power state
- Best-effort: each read is independent; failures set flag bits, don't abort collection
- Zeroes failed fields so consumers see deterministic values

### transport/battery_transport.c
- Compile-time vtable pattern: selects backend via `CONFIG_BATTERY_TRANSPORT_BLE` or `CONFIG_BATTERY_TRANSPORT_MOCK`
- `battery_transport_send()` serializes packet via `battery_serialize_pack()` then delegates to backend
- Returns `BATTERY_STATUS_UNSUPPORTED` if no backend compiled in
- Null-checks packet pointer before serialization

### transport/battery_serialize.c
- Encodes/decodes `battery_telemetry_packet` to/from 20-byte little-endian wire buffer
- Explicit byte shifts (`put_u16_le`, `put_u32_le`, etc.) — fully portable, no struct packing
- 20 bytes fits within a single BLE ATT default MTU (23 − 3 = 20)
- Wire format: version(1) + timestamp(4) + voltage(4) + temperature(4) + soc(2) + power_state(1) + flags(4)

### transport/ble/battery_transport_ble_zephyr.c
- Custom BLE GATT service with notification characteristic for telemetry
- Service UUID: `12340001-5678-9ABC-DEF0-123456789ABC`
- Characteristic UUID: `12340002-5678-9ABC-DEF0-123456789ABC` (Read + Notify)
- `BT_GATT_SERVICE_DEFINE()` compile-time GATT table
- `bt_enable()` with semaphore-based synchronous init, then connectable advertising
- Drop policy: silently succeeds when no client subscribed (no error on unsubscribed send)
- Connection callbacks manage ref-counted `bt_conn`, resume advertising on disconnect
- Device name configurable via `CONFIG_BATTERY_BLE_DEVICE_NAME` (default "iBattery")
- Resource overhead: ~62 KB flash, ~12 KB RAM (BLE stack)

### hal/battery_hal_adc_zephyr.c
- Configures nRF52840 SAADC via Zephyr ADC API
- Input: `NRFX_ANALOG_INTERNAL_VDD` (measures supply rail, not an external pin)
- Config: 12-bit resolution, 1/6 gain, 0.6V internal reference, 40us acquisition time
- 16x hardware oversampling for noise reduction
- Auto-calibration before each read
- Channel config stored at file scope; `adc_channel_setup()` re-called before every read to work around nRF SAADC driver clobbering per-channel input-mux settings when other channels are read

### hal/battery_hal_temp_zephyr.c (CONFIG_BATTERY_TEMP_DIE)
- Reads nRF52840 on-chip die temperature sensor via Zephyr sensor API
- Uses `DEVICE_DT_GET(DT_NODELABEL(temp))` to obtain the TEMP peripheral device
- `sensor_sample_fetch()` + `sensor_channel_get(SENSOR_CHAN_DIE_TEMP)` for each read
- Converts `sensor_value` to 0.01 °C: `val1 * 100 + val2 / 10000`
- Accuracy: ±2 °C (nRF52840 TEMP peripheral specification)

### hal/battery_hal_temp_ntc_zephyr.c (CONFIG_BATTERY_TEMP_NTC, default)
- Reads external 10K NTC thermistor (B=3950) via nRF52840 SAADC channel 1 (AIN1 / P0.03)
- Circuit: VDD → 10K pullup → ADC pin → NTC → GND
- 4-step pipeline: ADC read → raw to mV → mV to resistance → LUT interpolation
- ADC config: 12-bit, 1/6 gain, 0.6V internal reference, 40µs acquisition, 4x oversampling
- Analog input selected via `NRFX_ANALOG_EXTERNAL_AIN1` from `<helpers/nrfx_analog_common.h>` (nrfx v3.x 0-based enum; the legacy `NRF_SAADC_INPUT_AINx` enum is 1-based and causes an off-by-one pin selection)
- Channel config stored at file scope; `adc_channel_setup()` re-called before every read to restore AIN1 after the VDD channel clobbers the input mux
- Uses `battery_ntc_lut.c` for resistance-to-temperature conversion
- Selected at compile time via `CONFIG_BATTERY_TEMP_NTC=y` in `app/Kconfig`
- Same interface as die sensor HAL — modules above are unchanged

### hal/battery_hal_zephyr.c
- `battery_hal_init()` calls ADC init then temperature init in sequence
- `battery_hal_get_uptime_ms()` wraps Zephyr `k_uptime_get()`

---

## Data Flow: Telemetry Collection

```
battery_telemetry_collect(&pkt)
  |
  +-> battery_sdk_get_uptime_ms() -> pkt.timestamp_ms
  |
  +-> battery_voltage_get_mv()
  |     +-> battery_adc_read_mv()
  |     |     +-> battery_hal_adc_read_raw()    [SAADC sample]
  |     |     +-> battery_hal_adc_raw_to_pin_mv() [raw -> mV]
  |     +-> battery_voltage_filter_update()       [moving avg]
  |     +-> pkt.voltage_mv
  |
  +-> battery_temperature_get_c_x100()
  |     +-> battery_hal_temp_read_c_x100()
  |     |     [NTC: SAADC AIN1 -> mV -> R -> LUT interpolate]
  |     |     [Die: sensor_fetch -> SENSOR_CHAN_DIE_TEMP]
  |     +-> pkt.temperature_c_x100
  |
  +-> battery_soc_estimator_get_pct_x100()
  |     +-> battery_voltage_get_mv()
  |     +-> battery_soc_lut_interpolate() [LUT + lerp]
  |     +-> pkt.soc_pct_x100
  |
  +-> battery_power_manager_get_state()
  |     +-> battery_voltage_get_mv()        [threshold check]
  |     +-> hysteresis state machine         [2100/2200 mV]
  |     +-> pkt.power_state
  |
  +-> status_flags: OR of any per-field error bits

battery_transport_send(&pkt)           [optional, CONFIG_BATTERY_TRANSPORT]
  |
  +-> battery_serialize_pack()          [pack into 20-byte wire buffer]
  +-> g_ops->send(buf, 20)             [BLE notify / mock capture]
```

---

## Initialization Order

`battery_sdk_init()` calls subsystems in this order. If any step fails, initialization stops and the error is returned.

```
1. battery_hal_init()           -> ADC + temperature sensor ready
2. battery_voltage_init()       -> ADC wrapper + filter ready
3. battery_temperature_init()   -> Temperature sensor ready
4. battery_power_manager_init() -> Power state monitor ready
5. battery_soc_estimator_init() -> SoC estimator ready
6. battery_telemetry_init()     -> Telemetry collector ready
7. battery_transport_init()     -> BLE transport ready (if CONFIG_BATTERY_TRANSPORT)
```

---

## Error Handling Strategy

- **HAL boundary**: Zephyr/nrfx errors (errno, nrfx_err_t) are translated to `battery_status` at the HAL return boundary. No platform errors leak above the HAL.
- **Module level**: Every function validates inputs (NULL checks, range checks) and returns appropriate `battery_status` codes.
- **Telemetry level**: Best-effort — individual failures are captured in `status_flags` bits without aborting the collection.

---

## Memory Budget

| Component | RAM (bytes) | Notes |
|-----------|-------------|-------|
| Voltage filter | 28 | Window=12, circular buffer |
| ADC sample buffer (VDD) | 2 | Single int16_t |
| ADC sample buffer (NTC) | 2 | Single int16_t (NTC mode only) |
| Power state | 1 | Hysteresis memory (g_current_state) |
| SDK runtime state | 7 | 7 booleans |
| Telemetry packet | 20 | Stack-allocated per call |
| Transport wire buffer | 20 | BLE cached value (if CONFIG_BATTERY_TRANSPORT) |
| SoC LUT (CR2032) | 0 (const) | 36 bytes in flash |
| SoC LUT (LiPo) | 0 (const) | 44 bytes in flash |
| NTC LUT (10K B3950) | 0 (const) | 128 bytes in flash (16 entries × 8 bytes) |
| **Total static RAM** | **~39** | NTC mode; excluding Zephyr/BLE stack overhead |
| **BLE stack overhead** | **~12 KB** | Additional when CONFIG_BATTERY_TRANSPORT_BLE (Zephyr BLE stack) |

---

## Portability

To port to a new platform:

1. Implement `battery_hal_adc_init()`, `battery_hal_adc_read_raw()`, `battery_hal_adc_raw_to_pin_mv()` for the target ADC
2. Implement `battery_hal_temp_init()` and `battery_hal_temp_read_c_x100()` for the target temperature sensor
3. Implement `battery_hal_init()` and `battery_hal_get_uptime_ms()` for the target OS
4. Add a new SoC LUT in `battery_soc_lut.c` for the target battery chemistry
5. Everything above the HAL layer compiles unchanged

**nRF-specific note:** Use `NRFX_ANALOG_EXTERNAL_AINx` from `<helpers/nrfx_analog_common.h>` for external analog inputs — the legacy `NRF_SAADC_INPUT_AINx` enum from `<hal/nrf_saadc.h>` is 1-based and causes an off-by-one pin selection with the Zephyr ADC driver. Both ADC HAL drivers must also re-call `adc_channel_setup()` before every read because the nRF SAADC driver does not preserve per-channel input-mux settings.
