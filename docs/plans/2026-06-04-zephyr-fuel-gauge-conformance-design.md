# Design — Zephyr `fuel_gauge` API conformance (read-only)

**Date:** 2026-06-04
**Status:** Approved (brainstorm) → ready for implementation plan
**Branch:** `feature/zephyr-fuel-gauge-api`

## Goal

Let any Zephyr application consume iBattery through the **standard
`fuel_gauge` driver API** (`fuel_gauge_get_prop()` / `fuel_gauge_get_props()`),
so adopting iBattery is not a bet on a bespoke API — it slots in behind the
interface Zephyr users already use (like `max17048` / `sbs_gauge`).

Strategic intent: this is the "become a standard" move — interoperability /
adoption. SoH (our differentiator) is **not in the standard API**, so we expose
it as a documented custom property *and* keep it in the native API.

## Scope

**In:** a read-only `fuel_gauge` driver, devicetree-instantiated, opt-in via
Kconfig, mapping iBattery's existing outputs to standard properties + one custom
SoH property. No change to existing behavior when disabled.

**Out (explicitly):** `set_property` (read-only → `-ENOSYS`, since `.set_property` is left unset and the subsystem returns `-ENOSYS`); SBS buffer/string
properties (manufacturer/device name/chemistry); `RUNTIME_TO_EMPTY/_TO_FULL`
(deferred — needs a rate model); charge-control properties. A future iteration
can add these.

## Approach

**Devicetree-instantiated driver** (not a non-DT device): a custom binding
`compatible = "aliaksandr,ibattery-fuel-gauge"` + `DEVICE_DT_INST_DEFINE`, so
consumers do `DEVICE_DT_GET(DT_NODELABEL(...))` exactly like any fuel gauge.
Marginally more code than a non-DT device, but it's the idiomatic, genuinely
"first-class Zephyr fuel-gauge driver" path — which is the whole point.

Rejected alternative: `DEVICE_DEFINE` with a fixed name (no DT). Simpler, but
non-idiomatic — consumers expect `DEVICE_DT_GET`, and a DT node is what makes us
look like a real driver. Not worth the small saving.

## Components (all new, all gated by `CONFIG_BATTERY_FUEL_GAUGE_API`)

| File | Purpose |
|---|---|
| `dts/bindings/fuel-gauge/aliaksandr,ibattery-fuel-gauge.yaml` | DT binding (the `compatible`) |
| `src/fuel_gauge/battery_fuel_gauge_zephyr.c` | implements `struct fuel_gauge_driver_api.get_property`; `DEVICE_DT_INST_DEFINE` |
| `include/battery_sdk/battery_fuel_gauge.h` | public: the custom SoH property id + a doc of which union member carries it |
| `app/Kconfig.battery` (edit) | `CONFIG_BATTERY_FUEL_GAUGE_API` (depends on `BATTERY_SDK`; `select FUEL_GAUGE`; default n) |
| `CMakeLists.txt` + `app/CMakeLists.txt` (edit) | compile the driver when the option is on (both paths; drift-guard-clean) |
| `app/boards/*.overlay` (example) | a sample node so the app build instantiates it (gated) |
| `tests/test_fuel_gauge.c` (host) | property-mapping unit test |
| `tests/module_consumer_fuel_gauge/` or extend smoke | build smoke: `DEVICE_DT_GET` + `fuel_gauge_get_prop` links through the subsystem |

## Property mapping (units VERIFIED against `fuel_gauge.h`, NCS v3.2.2 / Zephyr 4.2)

| Property | Union member | API unit | Source (iBattery) | Conversion |
|---|---|---|---|---|
| `RELATIVE_STATE_OF_CHARGE` | `relative_state_of_charge` | `uint8` %, 0–100 | `soc_pct_x100` | `(soc_pct_x100 + 50) / 100` (round) |
| `ABSOLUTE_STATE_OF_CHARGE` | `absolute_state_of_charge` | `uint8` %, 0–100 | same as relative (no separate design-vs-full split today) | same |
| `VOLTAGE` | `voltage` | **µV**, int | `voltage_mv` | `× 1000` |
| `CURRENT` | `current` | **µA**, int, **neg=discharging** | `current_ma_x100` | `× 10`, **sign-flip** to Zephyr convention |
| `AVG_CURRENT` | `avg_current` | **µA**, int | (same instantaneous value for v1) | `× 10`, sign-flip |
| `TEMPERATURE` | `temperature` | **0.1 K**, uint16 | `temperature_c_x100` | `(temp_c_x100 + 27315) / 10` |
| `CYCLE_COUNT` | `cycle_count` | **1/100ths**, uint32 | `cycle_count` | `× 100` |
| `REMAINING_CAPACITY` | `remaining_capacity` | **µAh**, uint32 | coulomb `mAh_x100` | `× 10` |
| `FULL_CHARGE_CAPACITY` | `full_charge_capacity` | **µAh**, uint32 | **SoH-learned** capacity = `rated_mAh × soh` | `rated_mAh × soh_pct_x100 / 100 × 1000` (clamp; falls back to rated when SoH unknown) |
| `DESIGN_CAPACITY` | `design_cap` | **mAh**, uint16 | `CONFIG_BATTERY_CAPACITY_MAH` | direct |
| `DESIGN_VOLTAGE` | `design_volt` | **mV**, uint16 | chemistry nominal (CR2032 3000 / LiPo 3700) | direct |
| **SoH (custom)** | TBD union member (see below) | % | `soh_pct_x100` | exposed as x100 to preserve precision |

Everything not listed → `-ENOTSUP`.

### Custom SoH property

`fuel_gauge.h` has **no** State-of-Health property (verified). Define:

```c
/* battery_fuel_gauge.h */
#define BATTERY_FUEL_GAUGE_PROP_SOH (FUEL_GAUGE_CUSTOM_BEGIN + 0)
```

The union has no dedicated SoH field, so the driver + consumer agree on a member.
Decision: carry SoH as **`val->flags` (uint32) = soh_pct_x100** (e.g. 7310 = 73.10%),
documented in `battery_fuel_gauge.h`. Rationale: keeps full x100 precision; `flags`
is a generic uint32 slot. (Alternative considered: reuse `relative_state_of_charge`
uint8 — rejected, loses precision and is semantically confusing.)

`FULL_CHARGE_CAPACITY` *also* reflects wear (SoH-scaled), so standard SBS-style
consumers see the fade even without the custom prop — a clean bonus.

## Data flow

App `fuel_gauge_get_prop(dev, prop, &val)` → driver `.get_property` reads
iBattery's **current cached telemetry/state** (same snapshot the telemetry layer
emits — no new sampling, no I/O in the getter) → converts per the table → fills
`val`. Pure function of last-known state; no blocking.

Open implementation detail (resolve in plan): the getter needs access to the
latest computed values. Prefer reusing the existing telemetry/SoC accessors
rather than re-reading hardware in the fuel_gauge path.

## Error handling

- Unsupported / out-of-range prop → `-ENOTSUP`.
- SDK not initialized or no valid reading yet → `-EIO`.
- `set_property` → `-ENOSYS` (read-only; `.set_property` left unset, so the subsystem returns `-ENOSYS`). `get_buffer_property` → not provided.
- Getter never blocks and never does I/O — returns last-known state.

## Testing (TDD)

1. **Host unit test** (`tests/test_fuel_gauge.c`, Unity): feed mock iBattery
   state, assert each mapped property returns the exact converted value + unit
   (esp. the tricky ones: µV, µA sign-flip, 0.1 K temperature, cycle ×100,
   µAh vs mAh capacity split, SoH custom prop value). Assert `-ENOTSUP` for an
   unsupported prop and for `set_property`.
2. **Build smoke** through the subsystem: a tiny consumer that `DEVICE_DT_GET`s
   the node and calls `fuel_gauge_get_prop` — proves it compiles + links via the
   Zephyr `fuel_gauge` API (mirrors `tests/module_consumer`). Wire into
   `firmware.yml` ESP32-C3 (and/or the module-path job).
3. **Drift guard** stays green (option lives in `app/Kconfig.battery`; CMake
   source sets match across app + module paths).
4. **Regression:** with `CONFIG_BATTERY_FUEL_GAUGE_API=n` (default), builds are
   byte-identical to today.

## Opt-in / no-regression

`CONFIG_BATTERY_FUEL_GAUGE_API` default **n**. Off → file not compiled, no DT
node, zero footprint. On → `select FUEL_GAUGE`, driver instantiated from DT.

## Risks / to-confirm in the plan

- Exact members of `struct fuel_gauge_driver_api` for this Zephyr version
  (confirm `.get_property` signature + whether `.get_buffer_property` must be
  present/NULL).
- iBattery current sign convention vs Zephyr's negative=discharging (confirm
  against the coulomb/current HAL; the design assumes a flip).
- How the getter accesses latest state without re-sampling (reuse accessors).
- `DEVICE_DT_INST_DEFINE` init level/priority so the device is ready before app
  consumers.

## Promo payoff (after it lands + is hardware/CI-proven)

"iBattery is now a standard Zephyr `fuel_gauge` driver — and it adds the
State-of-Health the standard API doesn't have." dev.to post + Zephyr DevZone /
Discord. Reinforces the positioning doc's roadmap line (turns it true).
