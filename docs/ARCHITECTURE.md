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
                         |
+------------------------------------------------------+
|                  Public SDK API                       |
|              (include/battery_sdk/*.h)                |
|                                                       |
|  battery_sdk.h        battery_telemetry.h             |
|  battery_voltage.h    battery_temperature.h           |
|  battery_soc_estimator.h  battery_power_manager.h     |
|  battery_status.h     battery_types.h                 |
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
|                    battery_            battery_soc_   |
|                    temperature.c       lut.h          |
|                    battery_power_                     |
|                    manager.c                          |
|                                                       |
|                 telemetry/                             |
|                   battery_telemetry.c                  |
+------------------------------------------------------+
                         |
+------------------------------------------------------+
|              Hardware Abstraction Layer                |
|                   (src/hal/)                           |
|                                                       |
|  battery_hal.h            Interface (portable)        |
|  battery_hal_zephyr.c     Platform init + uptime      |
|  battery_hal_adc_zephyr.c ADC channel setup + read    |
+------------------------------------------------------+
                         |
+------------------------------------------------------+
|                Platform Drivers                       |
|           (Zephyr ADC API + nRF SAADC)                |
+------------------------------------------------------+
                         |
+------------------------------------------------------+
|                   Hardware                            |
|        nRF52840 SAADC -> VDD rail -> CR2032           |
+------------------------------------------------------+
```

---

## Module Responsibilities

### core/battery_sdk.c
- Owns the SDK runtime state (`battery_sdk_runtime_state`)
- `battery_sdk_init()` calls each subsystem init in dependency order
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

### core_modules/battery_adc.c
- Thin wrapper around HAL ADC functions
- Reads raw ADC sample, converts to millivolts via HAL
- Hides HAL details from the voltage module

### intelligence/battery_soc_estimator.c
- Reads current voltage, passes to LUT interpolator
- Returns SoC in 0.01% units (0-10000)

### intelligence/battery_soc_lut.c
- CR2032 9-point voltage-to-SoC lookup table
- Linear interpolation between adjacent points
- Integer-only math: `soc = soc_high - (soc_high - soc_low) * (v_high - v) / (v_high - v_low)`
- Clamps at boundaries

### telemetry/battery_telemetry.c
- Reads all subsystems in sequence: timestamp, voltage, temperature, SoC, power state
- Best-effort: each read is independent; failures set flag bits, don't abort collection
- Zeroes failed fields so consumers see deterministic values

### hal/battery_hal_adc_zephyr.c
- Configures nRF52840 SAADC via Zephyr ADC API
- Input: `NRFX_ANALOG_INTERNAL_VDD` (measures supply rail, not an external pin)
- Config: 12-bit resolution, 1/6 gain, 0.6V internal reference, 40us acquisition time
- 16x hardware oversampling for noise reduction
- Auto-calibration before each read

### hal/battery_hal_zephyr.c
- `battery_hal_init()` delegates to ADC init
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
  +-> battery_temperature_get_c_x100() -> pkt.temperature_c_x100
  |
  +-> battery_soc_estimator_get_pct_x100()
  |     +-> battery_voltage_get_mv()
  |     +-> battery_soc_lut_interpolate() [LUT + lerp]
  |     +-> pkt.soc_pct_x100
  |
  +-> battery_power_manager_get_state() -> pkt.power_state
  |
  +-> status_flags: OR of any per-field error bits
```

---

## Initialization Order

`battery_sdk_init()` calls subsystems in this order. If any step fails, initialization stops and the error is returned.

```
1. battery_hal_init()           -> ADC hardware ready
2. battery_voltage_init()       -> ADC wrapper + filter ready
3. battery_temperature_init()   -> Temperature sensor ready
4. battery_power_manager_init() -> Power state monitor ready
5. battery_soc_estimator_init() -> SoC estimator ready
6. battery_telemetry_init()     -> Telemetry collector ready
```

---

## Error Handling Strategy

- **HAL boundary**: Zephyr/nrfx errors (errno, nrfx_err_t) are translated to `battery_status` at the HAL return boundary. No platform errors leak above the HAL.
- **Module level**: Every function validates inputs (NULL checks, range checks) and returns appropriate `battery_status` codes.
- **Telemetry level**: Best-effort — individual failures are captured in `status_flags` bits without aborting the collection.

---

## Memory Budget (Phase 1)

| Component | RAM (bytes) | Notes |
|-----------|-------------|-------|
| Voltage filter | 28 | Window=12, circular buffer |
| ADC sample buffer | 2 | Single int16_t |
| SDK runtime state | 6 | 6 booleans |
| Telemetry packet | 20 | Stack-allocated per call |
| SoC LUT | 0 (const) | 36 bytes in flash |
| **Total static RAM** | **~36** | Excluding Zephyr overhead |

---

## Portability

To port to a new platform:

1. Implement `battery_hal_adc_init()`, `battery_hal_adc_read_raw()`, `battery_hal_adc_raw_to_pin_mv()` for the target ADC
2. Implement `battery_hal_init()` and `battery_hal_get_uptime_ms()` for the target OS
3. Add a new SoC LUT in `battery_soc_lut.c` for the target battery chemistry
4. Everything above the HAL layer compiles unchanged
