# Phase 8b: Voltage Smoothing & SoC Slew Limiter Design

**Date:** 2026-04-14
**Status:** Approved
**Phase:** 8b (of 8a/8b/8c Advanced SoC roadmap)

## Overview

Software-only accuracy improvement for SoC estimation. Adds two
defense-in-depth layers against load-induced voltage sag (e.g.
BLE TX pulling voltage down 500mV during transmit):

1. **Median filter** as alternative to existing moving-average filter
2. **SoC slew limiter** that caps reported SoC change rate

No hardware required. Works with every existing platform.
Composes cleanly with Phase 8a coulomb counting (when INA219
is validated) — both can be enabled simultaneously.

## Problem

Current pipeline: ADC -> moving-average filter (8 samples) -> LUT.

A 500mV BLE TX sag pulls the moving average down by 500/8 = 62 mV.
On the LiPo plateau (3.7-3.8V), 62 mV maps to ~10-20% SoC change.
User sees SoC jitter on every BLE notification.

## Solution

### Layer 1: Median Filter

Replace moving-average behavior (selectable at compile time).

**Files:**
- `src/core_modules/battery_voltage_filter_median.c` (new)
- `src/core_modules/battery_voltage_filter_median.h` (new)

**Algorithm:** Sort a copy of the buffer, return middle element
(or mean of two middle elements for even window). Insertion sort,
integer-only, ~30 comparisons average for window=8.

**Kconfig choice:**
```kconfig
choice BATTERY_VOLTAGE_FILTER_TYPE
    prompt "Voltage filter type"
    default BATTERY_VOLTAGE_FILTER_MEAN
config BATTERY_VOLTAGE_FILTER_MEAN
    bool "Moving average (legacy)"
config BATTERY_VOLTAGE_FILTER_MEDIAN
    bool "Median (rejects load-induced sags)"
endchoice
```

**CMake:** select source file based on which option is set.
Existing `battery_voltage.c` keeps calling
`battery_voltage_filter_update()` — implementation differs.

**Memory:** same footprint as existing filter
(`battery_voltage_filter_t` struct, 16-element buffer, ~41 bytes).

### Layer 2: SoC Slew Limiter

Caps how fast reported SoC can change. Catches LUT plateau cliffs
where small voltage changes amplify into large SoC swings.

**Files:**
- `src/intelligence/battery_soc_estimator.c` (modify)

**Algorithm:**
```c
int32_t delta = new_soc - prev_soc;
int32_t max_delta = (SLEW_RATE_X100_PER_SEC * dt_ms) / 1000;
if (delta > max_delta)  new_soc = prev_soc + max_delta;
if (delta < -max_delta) new_soc = prev_soc - max_delta;
prev_soc = new_soc;
```

Bypassed on:
- First sample after init (return immediately, no clamp)
- Anchor events (full charge / cutoff resets) when 8a is enabled

**Kconfig:**
```kconfig
config BATTERY_SOC_SLEW_LIMIT
    bool "Limit SoC change rate"
    default y
config BATTERY_SOC_SLEW_RATE_PCT_PER_MIN
    int "Max SoC change per minute (percent)"
    default 5
    range 1 100
    depends on BATTERY_SOC_SLEW_LIMIT
```

5%/min = realistic max physical discharge rate for LiPo under
normal IoT load. Higher rates skip the limit; lower rates would
reject real changes.

**State:** 6 bytes (prev_soc_x100 uint16 + prev_timestamp uint32).

## Test Plan

All host-testable, no hardware:

| Suite | Tests | Coverage |
|-------|-------|----------|
| `test_voltage_filter_median.c` | ~10 | Odd/even windows, single outlier rejection, multiple outliers, sorted/reverse input, full circular wrap |
| `test_soc_slew_limit.c` | ~6 | Rate cap up, rate cap down, first-sample bypass, anchor bypass (when 8a enabled), capacity-config interaction, dt=0 edge case |

## Memory Budget

| Component | RAM |
|-----------|-----|
| Median filter | same as mean filter (~41 bytes) — selected at compile time |
| Slew limiter state | ~6 bytes |
| **Net new RAM** | **~6 bytes** |

## Files Changed / Added

### New Files

- `src/core_modules/battery_voltage_filter_median.c`
- `src/core_modules/battery_voltage_filter_median.h`
- `tests/test_voltage_filter_median.c`
- `tests/test_soc_slew_limit.c`

### Modified Files

- `app/Kconfig` — add filter choice and slew-limit options
- `CMakeLists.txt` — conditional source for filter type
- `src/intelligence/battery_soc_estimator.c` — slew-limit pass
- `tests/CMakeLists.txt` — register new test targets
- `docs/ARCHITECTURE.md` — update data-flow diagram
- `docs/SDK_API.md` — note new Kconfig options
- `docs/RELEASE_NOTES.md` — v0.9.0 entry
- `docs/ROADMAP.md` — mark Phase 8b complete, advance 8c

## Composition with Phase 8a

When both `BATTERY_SOC_COULOMB` and `BATTERY_SOC_SLEW_LIMIT` are
enabled:
- Coulomb counting runs as Phase 8a designed
- Slew limiter is applied to the *final* SoC value (after coulomb)
- Anchor events bypass the slew limiter (full reset to LUT value)

This gives users with INA219 hardware the best of both: accurate
charge tracking + smooth reported values.
