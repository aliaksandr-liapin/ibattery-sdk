# Zephyr `fuel_gauge` API Conformance — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add an opt-in, read-only Zephyr `fuel_gauge` driver that exposes iBattery's existing outputs through the standard `fuel_gauge_get_prop()` API, plus a custom State-of-Health property.

**Architecture:** Split into (a) **pure integer conversion helpers** with no Zephyr dependencies — host-unit-tested with Unity; and (b) a **thin Zephyr driver** (`get_property` + `DEVICE_DT_INST_DEFINE`) that calls `battery_telemetry_collect()` / `battery_soh_get_pct_x100()` and packs the helpers' output into `union fuel_gauge_prop_val` — verified by a build smoke test through the subsystem. Gated by `CONFIG_BATTERY_FUEL_GAUGE_API` (default n).

**Tech Stack:** Zephyr `fuel_gauge` subsystem, devicetree binding, Kconfig, Unity (host tests), existing iBattery public API.

**Design ref:** `docs/plans/2026-06-04-zephyr-fuel-gauge-conformance-design.md` (units verified against `fuel_gauge.h`, NCS v3.2.2 / Zephyr 4.2).

---

## Task 1: Pure conversion helpers (host-tested, TDD)

Pure functions, no Zephyr headers, so they're host-testable. This is where all the unit math lives (the risky part).

**Files:**
- Create: `include/battery_sdk/battery_fuel_gauge_convert.h`
- Create: `src/core_modules/battery_fuel_gauge_convert.c`
- Test: `tests/test_fuel_gauge_convert.c`
- Modify: `tests/CMakeLists.txt` (register the new suite)

**Header (`battery_fuel_gauge_convert.h`):**
```c
#ifndef BATTERY_SDK_BATTERY_FUEL_GAUGE_CONVERT_H
#define BATTERY_SDK_BATTERY_FUEL_GAUGE_CONVERT_H
#include <stdint.h>

/* Battery voltage mV -> Zephyr fuel_gauge µV. */
int32_t battery_fg_mv_to_uv(int32_t mv);

/* Current centi-mA (0.01 mA) -> Zephyr µA, sign-flipped to Zephyr's
 * negative=discharging convention (iBattery reports discharge as positive). */
int32_t battery_fg_ma_x100_to_ua(int32_t ma_x100);

/* Temperature centi-°C (0.01 °C) -> Zephyr 0.1 K (uint16, clamped >=0). */
uint16_t battery_fg_cdegc_to_decikelvin(int32_t c_x100);

/* SoC centi-percent (0.01 %) -> integer percent 0..100 (rounded, clamped). */
uint8_t battery_fg_socx100_to_pct(uint16_t soc_x100);

/* Charge centi-mAh (0.01 mAh) -> Zephyr µAh (uint32, negatives clamp to 0). */
uint32_t battery_fg_mahx100_to_uah(int32_t mah_x100);

/* Full-charge capacity µAh from rated mAh scaled by SoH (soh centi-percent).
 * soh_x100==0 (unknown) -> falls back to rated capacity. */
uint32_t battery_fg_full_charge_uah(int32_t rated_mah, uint16_t soh_x100);

/* Cycle count -> Zephyr "1/100ths" unit. */
uint32_t battery_fg_cycles_to_centi(uint32_t cycles);

#endif
```

**Step 1 — failing test** (`tests/test_fuel_gauge_convert.c`), one assert block per helper:
```c
#include "unity.h"
#include "battery_sdk/battery_fuel_gauge_convert.h"

void setUp(void) {}
void tearDown(void) {}

void test_mv_to_uv(void) {
    TEST_ASSERT_EQUAL_INT32(3300000, battery_fg_mv_to_uv(3300));
}
void test_current_sign_flip_and_scale(void) {
    /* 31.70 mA discharge -> -31700 µA */
    TEST_ASSERT_EQUAL_INT32(-31700, battery_fg_ma_x100_to_ua(3170));
    /* charge (negative iBattery current) -> positive µA */
    TEST_ASSERT_EQUAL_INT32(5000, battery_fg_ma_x100_to_ua(-500));
}
void test_temp_cdegc_to_decikelvin(void) {
    /* 23.56 °C -> (2356 + 27315)/10 = 2967 (296.7 K) */
    TEST_ASSERT_EQUAL_UINT16(2967, battery_fg_cdegc_to_decikelvin(2356));
    /* 0 °C -> 2731 */
    TEST_ASSERT_EQUAL_UINT16(2731, battery_fg_cdegc_to_decikelvin(0));
}
void test_soc_to_pct_round_clamp(void) {
    TEST_ASSERT_EQUAL_UINT8(73, battery_fg_socx100_to_pct(7310));   /* 73.10 -> 73 */
    TEST_ASSERT_EQUAL_UINT8(100, battery_fg_socx100_to_pct(10000));
    TEST_ASSERT_EQUAL_UINT8(100, battery_fg_socx100_to_pct(12000)); /* clamp */
    TEST_ASSERT_EQUAL_UINT8(1, battery_fg_socx100_to_pct(99));      /* 0.99 -> 1 (round) */
}
void test_mah_to_uah_clamp(void) {
    TEST_ASSERT_EQUAL_UINT32(5380, battery_fg_mahx100_to_uah(538)); /* 5.38 mAh -> 5380 µAh */
    TEST_ASSERT_EQUAL_UINT32(0, battery_fg_mahx100_to_uah(-10));
}
void test_full_charge_uah_with_soh(void) {
    /* rated 10 mAh, SoH 73.10% -> 7.31 mAh -> 7310 µAh */
    TEST_ASSERT_EQUAL_UINT32(7310, battery_fg_full_charge_uah(10, 7310));
    /* unknown SoH -> rated 10 mAh -> 10000 µAh */
    TEST_ASSERT_EQUAL_UINT32(10000, battery_fg_full_charge_uah(10, 0));
}
void test_cycles_to_centi(void) {
    TEST_ASSERT_EQUAL_UINT32(500, battery_fg_cycles_to_centi(5));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_mv_to_uv);
    RUN_TEST(test_current_sign_flip_and_scale);
    RUN_TEST(test_temp_cdegc_to_decikelvin);
    RUN_TEST(test_soc_to_pct_round_clamp);
    RUN_TEST(test_mah_to_uah_clamp);
    RUN_TEST(test_full_charge_uah_with_soh);
    RUN_TEST(test_cycles_to_centi);
    return UNITY_END();
}
```

**Step 2 — register suite** in `tests/CMakeLists.txt` (follow the existing `add_battery_test(...)` / `add_executable`+`add_test` pattern used by the other 23 suites — match whatever macro the file already uses; source = `src/core_modules/battery_fuel_gauge_convert.c`).

**Step 3 — run, expect FAIL** (link error / assert fail):
```
cd tests && cmake -B build . && cmake --build build && ctest --test-dir build -R fuel_gauge_convert --output-on-failure
```
Expected: FAIL (undefined refs).

**Step 4 — implement** (`src/core_modules/battery_fuel_gauge_convert.c`):
```c
#include "battery_sdk/battery_fuel_gauge_convert.h"

int32_t battery_fg_mv_to_uv(int32_t mv) { return mv * 1000; }

int32_t battery_fg_ma_x100_to_ua(int32_t ma_x100) { return -(ma_x100 * 10); }

uint16_t battery_fg_cdegc_to_decikelvin(int32_t c_x100) {
    int32_t dk = (c_x100 + 27315) / 10;       /* 0.01°C + 273.15°C, then to 0.1K */
    if (dk < 0) dk = 0;
    if (dk > UINT16_MAX) dk = UINT16_MAX;
    return (uint16_t)dk;
}

uint8_t battery_fg_socx100_to_pct(uint16_t soc_x100) {
    uint32_t pct = ((uint32_t)soc_x100 + 50u) / 100u;
    return (uint8_t)(pct > 100u ? 100u : pct);
}

uint32_t battery_fg_mahx100_to_uah(int32_t mah_x100) {
    if (mah_x100 < 0) return 0;
    return (uint32_t)mah_x100 * 10u;
}

uint32_t battery_fg_full_charge_uah(int32_t rated_mah, uint16_t soh_x100) {
    if (rated_mah < 0) rated_mah = 0;
    if (soh_x100 == 0) return (uint32_t)rated_mah * 1000u;          /* unknown -> rated */
    /* rated_mah(µAh=×1000) × soh% : ×1000 ÷ 10000 == ×1 ÷10 */
    return (uint32_t)(((uint64_t)rated_mah * 1000u * soh_x100) / 10000u);
}

uint32_t battery_fg_cycles_to_centi(uint32_t cycles) { return cycles * 100u; }
```

**Step 5 — run, expect PASS.** **Step 6 — commit** `feat(fuel_gauge): pure unit-conversion helpers (host-tested)`.

> Note for executor: confirm the rounding convention the team prefers; tests above encode round-to-nearest for SoC, truncate for capacity. Keep tests as the contract.

---

## Task 2: Kconfig option

**Files:** Modify `app/Kconfig.battery` (the single source of truth, sourced by both `app/Kconfig` and `Kconfig.ibattery`).

Add near the transport/feature options:
```kconfig
config BATTERY_FUEL_GAUGE_API
	bool "Expose iBattery via the Zephyr fuel_gauge driver API (read-only)"
	default n
	select FUEL_GAUGE
	help
	  Build a read-only Zephyr fuel_gauge driver that maps iBattery's
	  outputs (SoC, voltage, current, capacity, cycle count, temperature)
	  to the standard fuel_gauge properties, plus a custom State-of-Health
	  property. Instantiated from a devicetree node with compatible
	  "aliaksandr,ibattery-fuel-gauge". No effect when disabled.
```
**Commit:** `feat(fuel_gauge): add CONFIG_BATTERY_FUEL_GAUGE_API (default n)`.

---

## Task 3: Devicetree binding

**Files:** Create `dts/bindings/fuel-gauge/aliaksandr,ibattery-fuel-gauge.yaml`:
```yaml
description: iBattery SDK virtual fuel gauge (software, read-only)
compatible: "aliaksandr,ibattery-fuel-gauge"
include: [base.yaml]
```
(No I2C/reg — it's a software device over the existing SDK.)
**Commit:** `feat(fuel_gauge): add devicetree binding`.

---

## Task 4: The Zephyr driver + public custom-prop header

**Files:**
- Create: `include/battery_sdk/battery_fuel_gauge.h`
- Create: `src/fuel_gauge/battery_fuel_gauge_zephyr.c`
- Modify: `CMakeLists.txt` (root, module path) and `app/CMakeLists.txt` (app path) — compile the driver when `CONFIG_BATTERY_FUEL_GAUGE_API` is set (mirror how other gated HAL files are added; keep the two source sets in sync for `scripts/check_build_sync.py`).

**`battery_fuel_gauge.h`:**
```c
#ifndef BATTERY_SDK_BATTERY_FUEL_GAUGE_H
#define BATTERY_SDK_BATTERY_FUEL_GAUGE_H
#include <zephyr/drivers/fuel_gauge.h>

/* Custom State-of-Health property (the standard API has none).
 * Value is delivered in val->flags as State-of-Health in centi-percent
 * (e.g. 7310 == 73.10 %). */
#define BATTERY_FUEL_GAUGE_PROP_SOH (FUEL_GAUGE_CUSTOM_BEGIN + 0)
#endif
```

**`battery_fuel_gauge_zephyr.c`** (skeleton — executor fills bodies):
```c
#define DT_DRV_COMPAT aliaksandr_ibattery_fuel_gauge
#include <zephyr/device.h>
#include <zephyr/drivers/fuel_gauge.h>
#include "battery_sdk/battery_fuel_gauge.h"
#include "battery_sdk/battery_fuel_gauge_convert.h"
#include "battery_sdk/battery_telemetry.h"
#include "battery_sdk/battery_soh.h"

static int ibattery_fg_get_prop(const struct device *dev, fuel_gauge_prop_t prop,
                                union fuel_gauge_prop_val *val)
{
    ARG_UNUSED(dev);
    struct battery_telemetry_packet pkt;
    if (battery_telemetry_collect(&pkt) != 0) {
        return -EIO;
    }
    switch (prop) {
    case FUEL_GAUGE_VOLTAGE:           val->voltage = battery_fg_mv_to_uv(pkt.voltage_mv); break;
    case FUEL_GAUGE_CURRENT:
    case FUEL_GAUGE_AVG_CURRENT:       val->current = battery_fg_ma_x100_to_ua(pkt.current_ma_x100); break;
    case FUEL_GAUGE_TEMPERATURE:       val->temperature = battery_fg_cdegc_to_decikelvin(pkt.temperature_c_x100); break;
    case FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE:
    case FUEL_GAUGE_ABSOLUTE_STATE_OF_CHARGE:
        val->relative_state_of_charge = battery_fg_socx100_to_pct(pkt.soc_pct_x100); break;
    case FUEL_GAUGE_REMAINING_CAPACITY: val->remaining_capacity = battery_fg_mahx100_to_uah(pkt.coulomb_mah_x100); break;
    case FUEL_GAUGE_FULL_CHARGE_CAPACITY: {
        uint16_t soh = 0; (void)battery_soh_get_pct_x100(&soh);
        val->full_charge_capacity = battery_fg_full_charge_uah(CONFIG_BATTERY_CAPACITY_MAH, soh); break;
    }
    case FUEL_GAUGE_DESIGN_CAPACITY:   val->design_cap = (uint16_t)CONFIG_BATTERY_CAPACITY_MAH; break;
    case FUEL_GAUGE_CYCLE_COUNT:       val->cycle_count = battery_fg_cycles_to_centi(pkt.cycle_count); break;
    case BATTERY_FUEL_GAUGE_PROP_SOH: {
        uint16_t soh = 0;
        if (battery_soh_get_pct_x100(&soh) != 0) return -ENOTSUP;
        val->flags = soh; break;
    }
    default: return -ENOTSUP;
    }
    return 0;
}

static const struct fuel_gauge_driver_api ibattery_fg_api = {
    .get_property = ibattery_fg_get_prop,
};

static int ibattery_fg_init(const struct device *dev) { ARG_UNUSED(dev); return 0; }

#define IBATTERY_FG_INIT(inst) \
    DEVICE_DT_INST_DEFINE(inst, ibattery_fg_init, NULL, NULL, NULL, \
        POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &ibattery_fg_api);
DT_INST_FOREACH_STATUS_OKAY(IBATTERY_FG_INIT)
```
Executor: verify field names against `battery_types.h`/`battery_telemetry.h` (e.g. `current_ma_x100`, `coulomb_mah_x100`, `cycle_count`) and adjust; confirm init priority is fine relative to `battery_sdk_init`.

**Commit:** `feat(fuel_gauge): read-only Zephyr fuel_gauge driver + custom SoH prop`.

---

## Task 5: Example overlay so the app build instantiates it

**Files:** Create `app/boards/fuel_gauge.overlay` (or add to an existing extadc overlay) with a gated node:
```dts
/ {
    ibattery_fg: ibattery_fuel_gauge {
        compatible = "aliaksandr,ibattery-fuel-gauge";
        status = "okay";
    };
};
```
Document the build: `-DCONFIG_BATTERY_FUEL_GAUGE_API=y -DEXTRA_DTC_OVERLAY_FILE=boards/fuel_gauge.overlay`.
**Commit:** `feat(fuel_gauge): example devicetree overlay`.

---

## Task 6: Build smoke through the subsystem

**Files:** Create `tests/module_consumer_fuel_gauge/` (mirror `tests/module_consumer/`): `prj.conf` with `CONFIG_BATTERY_SDK=y`, `CONFIG_BATTERY_FUEL_GAUGE_API=y`; `src/main.c` that does `DEVICE_DT_GET(DT_NODELABEL(ibattery_fg))` and calls `fuel_gauge_get_prop(dev, FUEL_GAUGE_VOLTAGE, &v)` and `BATTERY_FUEL_GAUGE_PROP_SOH`. Plus overlay defining the node.

**Modify:** `.github/workflows/firmware.yml` — add a step (esp32c3, like the existing "Verify Zephyr-module consumption path") that builds this consumer. Proves it links through the `fuel_gauge` subsystem.
**Commit:** `test(fuel_gauge): module-path build smoke through the subsystem`.

---

## Task 7: Verify everything green (no new logic, just gates)

**Steps:**
1. Host suites: `cd tests && cmake -B build . && cmake --build build && ctest --test-dir build` → all (24) pass.
2. Drift guard: `python3 scripts/check_build_sync.py` → in sync.
3. Local ESP32-C3 build of the fuel_gauge consumer (vanilla workspace) → links + valid image.
4. Default build with the option **off** → unchanged (regression).
**Commit:** none (verification), or doc-count bump if suite count changes.

---

## Task 8: Docs

**Files:** `docs/SDK_API.md` (document the driver + custom SoH prop + units), `README.md` (a "Use it as a standard Zephyr fuel gauge" snippet), `CLAUDE.md` Kconfig table (+ the new option), `docs/RELEASE_NOTES.md`.
**Commit:** `docs: document the Zephyr fuel_gauge driver + custom SoH property`.

---

## Out of scope (future)
`set_property`, SBS buffer/string props, `RUNTIME_TO_EMPTY/_TO_FULL` (needs a rate model), charge-control props, caching the telemetry snapshot across a `get_props` batch.
