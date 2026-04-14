# Advanced SoC: Coulomb Counting Design

**Date:** 2026-04-13
**Status:** Approved
**Phase:** 8a (of 8a/8b/8c Advanced SoC roadmap)

## Overview

Add coulomb counting to the iBattery SDK using an INA219 current sensor over I2C. Coulomb counting tracks charge consumed/added by integrating current over time, solving the voltage-LUT's plateau-region inaccuracy and load-dependent voltage sag errors.

**Strategy:** Coulomb counting as primary SoC tracker, voltage-LUT as calibration anchor at known endpoints (full charge, cutoff). Designed so Kalman filter fusion can replace the estimator layer in Phase 8c without API changes.

**Target platform:** ESP32-C3 DevKitM (first validation), then nRF52840-DK and STM32.

## Hardware

- HiLetgo INA219 breakout board (0.1 ohm shunt, I2C address 0x40)
- Wired in series on battery high side: Battery+ -> VIN+ -> VIN- -> Load
- 4-wire I2C connection to ESP32-C3 (SDA=GPIO6, SCL=GPIO7, VCC=3.3V, GND)

## Architecture

```
INA219 (I2C, Zephyr sensor driver)
  -> Current HAL (battery_hal_current — wraps Zephyr sensor API)
    -> Coulomb Counter (trapezoidal integration, mAh accumulator)
      -> SoC Estimator v2 (coulomb primary + voltage anchor)
        -> Telemetry v3 (adds current_ma, coulomb_mah fields)
```

Matches existing SDK layering: HAL -> intelligence -> telemetry -> transport.

## 1. Current Sensing HAL

### Files

```
include/battery_sdk/battery_hal_current.h    -- public interface
src/hal/battery_hal_current_zephyr.c         -- Zephyr sensor backend
src/hal/battery_hal_current_stub.c           -- stub for builds without current sensing
```

### API

```c
int battery_hal_current_init(void);
int battery_hal_current_read_ma_x100(int32_t *current_ma_x100_out);
```

- Returns `battery_status` codes (OK, ERROR, INVALID_ARG, NOT_INITIALIZED)
- Signed output: positive = discharging, negative = charging
- Units: 0.01 mA (matches SDK x100 convention)
- NULL check on output parameter

### Implementation

Uses Zephyr's built-in INA219 sensor driver (`sensor_sample_fetch` / `sensor_channel_get`), same pattern as die temperature HAL. No custom I2C code.

Conversion from Zephyr `sensor_value` (val1=A, val2=uA) to mA x100:
```c
*current_ma_x100_out = val.val1 * 100000 + val.val2 / 10;
```

Stub returns `BATTERY_STATUS_UNSUPPORTED` for all calls.

### Devicetree Overlay (ESP32-C3)

```dts
&i2c0 {
    status = "okay";
    ina219@40 {
        compatible = "ti,ina219";
        reg = <0x40>;
        shunt-milliohm = <100>;
        lsb-microamp = <100>;
        brng = <0>;           /* 16V range (single-cell LiPo) */
        pg = <1>;             /* +/-80mV shunt range (~800mA max) */
        sadc = <13>;          /* 12-bit + 8x averaging */
        badc = <13>;
    };
};
```

### Kconfig

```kconfig
config BATTERY_CURRENT_SENSE
    bool "Enable current sensing"
    default n
    select SENSOR

config BATTERY_CURRENT_INA219
    bool "INA219 current sensor"
    depends on BATTERY_CURRENT_SENSE && I2C
    default y if BATTERY_CURRENT_SENSE
```

### CMake

```cmake
zephyr_library_sources_ifdef(CONFIG_BATTERY_CURRENT_SENSE
    src/hal/battery_hal_current_zephyr.c)
zephyr_library_sources_ifndef(CONFIG_BATTERY_CURRENT_SENSE
    src/hal/battery_hal_current_stub.c)
```

## 2. Coulomb Counter Module

### Files

```
include/battery_sdk/battery_coulomb.h
src/intelligence/battery_coulomb.c
```

### API

```c
int battery_coulomb_init(void);
int battery_coulomb_update(int32_t current_ma_x100, uint32_t dt_ms);
int battery_coulomb_get_mah_x100(int32_t *mah_x100_out);
int battery_coulomb_reset(int32_t mah_x100);
```

### Integration Method

Trapezoidal rule, integer-only arithmetic:
```
delta_uah = ((prev_current + current) / 2) * dt_ms / 3600
accumulated_mah_x1000 += delta_uah  (sub-mAh precision internally)
```

Internal accumulator is `int64_t` to prevent overflow on multi-day runs.

### Static State (~20 bytes)

```c
static int64_t  accumulated_mah_x1000;   // 0.001 mAh internal precision
static int32_t  prev_current_ma_x100;
static bool     initialized;
```

### NVS Persistence

Saves accumulated value to flash via existing `battery_hal_nvs` layer:
- Default save interval: every 60 seconds
- Also saves on significant change (>1% SoC shift)
- Configurable via `CONFIG_BATTERY_COULOMB_NVS_INTERVAL_S`
- Restores on boot so SoC survives reboot

## 3. SoC Estimator v2

### Behavior

Extends existing `battery_soc_estimator` — no API change.

1. **Init / first boot:** SoC from voltage LUT (existing behavior)
2. **Each sample (coulomb enabled):** `soc -= (delta_mah / capacity_mah) * 10000`
3. **Anchor points:** Reset coulomb accumulator, re-sync SoC to LUT value
4. **Fallback:** If current sensing unavailable, voltage-LUT only (zero regression)

`battery_soc_estimator_get_pct_x100()` returns coulomb-based SoC when `CONFIG_BATTERY_SOC_COULOMB` is enabled, voltage-LUT otherwise. Existing callers unchanged.

### Anchor Thresholds (compile-time, chemistry-dependent)

| Chemistry | Full Anchor | Empty Anchor |
|-----------|-------------|--------------|
| LiPo | V >= 4180mV AND |I| < 50mA | V <= 3000mV |
| CR2032 | V >= 2950mV | V <= 2000mV |

Full-charge anchor for LiPo requires near-zero current (CV phase complete) to avoid false triggers during active charging.

### Kconfig

```kconfig
config BATTERY_SOC_COULOMB
    bool "Coulomb counting SoC estimation"
    depends on BATTERY_CURRENT_SENSE
    default y if BATTERY_CURRENT_SENSE

config BATTERY_CAPACITY_MAH
    int "Battery capacity in mAh"
    depends on BATTERY_SOC_COULOMB
    default 220 if BATTERY_CHEMISTRY_CR2032
    default 1000 if BATTERY_CHEMISTRY_LIPO
```

## 4. Telemetry v3

### Wire Format (32 bytes)

```
Offset  Size  Type      Field
0       1     uint8     version (3)
1       4     uint32    timestamp_ms
5       4     int32     voltage_mv
9       4     int32     temperature_c_x100
13      2     uint16    soc_pct_x100
15      1     uint8     power_state
16      4     uint32    status_flags
20      4     uint32    cycle_count
24      4     int32     current_ma_x100        // NEW
28      4     int32     coulomb_mah_x100       // NEW
```

Gateway auto-detects format by packet length: 20=v1, 24=v2, 32=v3.

BLE MTU increases from 24 to 32 bytes.

### New Status Flags

```c
#define BATTERY_TELEMETRY_FLAG_CURRENT_ERR  (1U << 5)
#define BATTERY_TELEMETRY_FLAG_COULOMB_ERR  (1U << 6)
```

Best-effort model preserved: failed current read sets flag but doesn't abort packet.

## 5. Test Plan

All host-testable (Unity framework, no hardware):

| Suite | Count | Coverage |
|-------|-------|----------|
| `test_hal_current_stub.c` | ~3 | Stub returns error, init/read contract |
| `test_coulomb.c` | ~12 | Integration math, overflow, reset, sign handling, NVS round-trip |
| `test_soc_coulomb.c` | ~10 | Coulomb SoC tracking, anchor resets, fallback to LUT, capacity config |
| `test_telemetry_v3.c` | ~5 | v3 encode/decode, backward compat with v1/v2 |

Gateway: extend decoder tests for v3 packets (~3 new Python tests).

## 6. Memory Budget

| Component | RAM (bytes) |
|-----------|-------------|
| Coulomb counter state | ~20 |
| Current HAL (device pointer) | ~8 |
| SoC estimator additions | ~12 |
| **Total new** | **~40** |

Existing core: ~80 bytes. New total: ~120 bytes. No heap allocation.

## 7. Roadmap Phases

Advanced SoC is split into three incremental phases:

- **Phase 8a** (this work): Coulomb counting — INA219 HAL, coulomb counter, voltage-anchored SoC
- **Phase 8b**: Voltage-LUT correction mode — coulomb as smoothing layer over existing LUT
- **Phase 8c**: Kalman filter fusion — optimal weighting of voltage + coulomb + temperature

Each phase builds on the previous. Same public API throughout. Phase 8b and 8c are estimator-layer changes only — HAL and telemetry from 8a are reused.

## Files Changed / Added

### New Files

- `include/battery_sdk/battery_hal_current.h`
- `include/battery_sdk/battery_coulomb.h`
- `src/hal/battery_hal_current_zephyr.c`
- `src/hal/battery_hal_current_stub.c`
- `src/intelligence/battery_coulomb.c`
- `app/boards/esp32c3_devkitm.overlay` (extend)
- `tests/test_hal_current_stub.c`
- `tests/test_coulomb.c`
- `tests/test_soc_coulomb.c`
- `tests/test_telemetry_v3.c`

### Modified Files

- `app/Kconfig` — add current sense + coulomb Kconfig options
- `CMakeLists.txt` — add conditional sources
- `src/intelligence/battery_soc_estimator.c` — coulomb SoC path
- `src/telemetry/battery_telemetry.c` — v3 format
- `include/battery_sdk/battery_types.h` — v3 packet struct, new flags
- `gateway/` — v3 decoder + tests
- `docs/ROADMAP.md` — split Advanced SoC into 8a/8b/8c
