# Runtime-to-Empty Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** On-device "minutes to empty" estimate, exposed via a native API, the Zephyr `fuel_gauge` `RUNTIME_TO_EMPTY` property, and a new wire-v5 BLE telemetry field → gateway → Grafana.

**Architecture:** A pure, host-tested estimator (EMA-smoothed discharge current → `remaining_mAh × 60 ÷ avg_mA` in int64) with idle/charging → NOT_AVAILABLE. Thin integration into the telemetry packet (wire v5), the fuel_gauge driver, and the gateway. Opt-in `CONFIG_BATTERY_RUNTIME_TO_EMPTY` (default n; depends on `BATTERY_SOC_COULOMB`).

**Tech Stack:** Zephyr C (integer-only), Unity host tests, Python gateway (pytest), Grafana JSON.

**Design ref:** `docs/plans/2026-06-05-runtime-to-empty-design.md`.

**Verified facts (use these exact targets):**
- Telemetry version macro: `include/battery_sdk/battery_types.h:11` (`#define BATTERY_TELEMETRY_VERSION 4U` under SoH, else `3U`). Packet struct in same file.
- Serialize sizes: `src/transport/battery_serialize.h:49-53` (V1=20…V4=34, `BUF_SIZE=V4`); size ladder `:91-94`; serialize body `src/transport/battery_serialize.c:96-125`.
- Transport sizes: `include/battery_sdk/battery_transport.h:23-31`; `_Static_assert` at `src/transport/battery_transport.c:18`.
- MTU/ACL confs (5): `app/prj.conf`, `app/boards/esp32c3_devkitm.conf`, `app/boards/nucleo_l476rg_ble.conf`, `app/boards/nucleo_l476rg_ble_current.conf`, `app/boards/nucleo_l476rg_ble_extadc.conf` (`CONFIG_BT_L2CAP_TX_MTU=37`→`41`, `CONFIG_BT_BUF_ACL_TX_SIZE`/`RX_SIZE=41`→`45`).
- `battery_status.h` has OK/ERROR/INVALID_ARG/NOT_INITIALIZED/UNSUPPORTED/IO — **no NOT_AVAILABLE** (add it).
- Host test build: `export PATH="/opt/homebrew/bin:/usr/bin:/bin:/opt/nordic/ncs/toolchains/e5f4758bcf/bin:$PATH"; cd tests && rm -rf build && cmake -B build . && cmake --build build && ctest --test-dir build --output-on-failure`

---

## Task 1: `BATTERY_STATUS_NOT_AVAILABLE` + pure estimator (host-tested, TDD)

**Files:**
- Modify: `include/battery_sdk/battery_status.h` — add `BATTERY_STATUS_NOT_AVAILABLE = -6` to the enum.
- Create: `include/battery_sdk/battery_runtime.h`
- Create: `src/intelligence/battery_runtime.c`
- Test: `tests/test_runtime.c` + register in `tests/CMakeLists.txt` (mirror an existing suite).

**Header API:**
```c
#ifndef BATTERY_SDK_BATTERY_RUNTIME_H
#define BATTERY_SDK_BATTERY_RUNTIME_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Reset EMA state (call at init). */
void battery_runtime_reset(void);
/* Feed a current sample (0.01 mA, positive=discharge) into the EMA. */
void battery_runtime_update(int32_t current_ma_x100);
/* Estimate minutes to empty from remaining charge (0.01 mAh) + the EMA'd current.
 * Returns BATTERY_STATUS_OK and *minutes_out on success;
 * BATTERY_STATUS_NOT_AVAILABLE when idle/charging (EMA current <= idle threshold). */
int battery_runtime_to_empty_min(int32_t remaining_mah_x100, uint32_t *minutes_out);
#ifdef __cplusplus
}
#endif
#endif
```

**Tests (the contract — pick concrete numbers):**
```c
#include "unity.h"
#include "battery_sdk/battery_runtime.h"
#include "battery_sdk/battery_status.h"
void setUp(void){ battery_runtime_reset(); }
void tearDown(void){}

/* Steady 60.00 mA discharge, 60.00 mAh remaining -> 60 minutes.
   minutes = remaining_mAh*60/mA = 60*60/60 = 60 */
void test_basic(void){
    for(int i=0;i<20;i++) battery_runtime_update(6000); /* 60.00 mA */
    uint32_t m=0;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_runtime_to_empty_min(6000, &m)); /* 60.00 mAh */
    TEST_ASSERT_EQUAL_UINT32(60, m);
}
void test_idle_not_available(void){
    for(int i=0;i<20;i++) battery_runtime_update(0);
    uint32_t m=123;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_NOT_AVAILABLE, battery_runtime_to_empty_min(6000,&m));
}
void test_charging_not_available(void){
    for(int i=0;i<20;i++) battery_runtime_update(-5000); /* charging */
    uint32_t m=123;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_NOT_AVAILABLE, battery_runtime_to_empty_min(6000,&m));
}
void test_remaining_zero(void){
    for(int i=0;i<20;i++) battery_runtime_update(6000);
    uint32_t m=123;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_runtime_to_empty_min(0,&m));
    TEST_ASSERT_EQUAL_UINT32(0, m);
}
void test_low_drain_no_overflow(void){
    /* 220 mAh @ 0.10 mA -> 220*60/0.1 = 132000 min, fits uint32, no overflow */
    for(int i=0;i<50;i++) battery_runtime_update(10); /* 0.10 mA */
    uint32_t m=0;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_runtime_to_empty_min(22000,&m));
    TEST_ASSERT_EQUAL_UINT32(132000, m);
}
```
(main() RUN_TEST each.)

**Implementation sketch (`battery_runtime.c`) — integer EMA, int64 division:**
```c
#include "battery_sdk/battery_runtime.h"
#include "battery_sdk/battery_status.h"
#ifndef CONFIG_BATTERY_RUNTIME_EMA_ALPHA_X1000
#define CONFIG_BATTERY_RUNTIME_EMA_ALPHA_X1000 300   /* host-test default */
#endif
#ifndef CONFIG_BATTERY_RUNTIME_IDLE_THRESHOLD_MA_X100
#define CONFIG_BATTERY_RUNTIME_IDLE_THRESHOLD_MA_X100 100 /* 1.00 mA */
#endif
static int32_t g_ema_x100;   /* smoothed current, 0.01 mA */
static int g_primed;
void battery_runtime_reset(void){ g_ema_x100=0; g_primed=0; }
void battery_runtime_update(int32_t cur_x100){
    if(!g_primed){ g_ema_x100=cur_x100; g_primed=1; return; }
    int32_t a=CONFIG_BATTERY_RUNTIME_EMA_ALPHA_X1000;
    g_ema_x100 = (int32_t)(((int64_t)a*cur_x100 + (int64_t)(1000-a)*g_ema_x100 + 500)/1000);
}
int battery_runtime_to_empty_min(int32_t rem_x100, uint32_t *out){
    if(out==NULL) return BATTERY_STATUS_INVALID_ARG;
    if(g_ema_x100 <= CONFIG_BATTERY_RUNTIME_IDLE_THRESHOLD_MA_X100) return BATTERY_STATUS_NOT_AVAILABLE;
    if(rem_x100<=0){ *out=0; return BATTERY_STATUS_OK; }
    int64_t minutes = ((int64_t)rem_x100*60)/g_ema_x100; /* x100 cancel */
    if(minutes > (int64_t)(UINT32_MAX-1)) minutes=UINT32_MAX-1;
    *out=(uint32_t)minutes; return BATTERY_STATUS_OK;
}
```
TDD: write tests → fail → implement → pass → full suite green → commit `feat(runtime): time-to-empty estimator (host-tested)`.

---

## Task 2: Kconfig + CMake wiring

- `app/Kconfig.battery`: `config BATTERY_RUNTIME_TO_EMPTY` (bool, default n, `depends on BATTERY_SOC_COULOMB`), `BATTERY_RUNTIME_EMA_ALPHA_X1000` (int, default 300, range 1 1000), `BATTERY_RUNTIME_IDLE_THRESHOLD_MA_X100` (int, default 100, range 1 100000). 4-space indent.
- `CMakeLists.txt` + `app/CMakeLists.txt`: compile `src/intelligence/battery_runtime.c` under `CONFIG_BATTERY_RUNTIME_TO_EMPTY` in BOTH paths (drift-guard-clean).
- Commit `feat(runtime): Kconfig + build wiring`.

---

## Task 3: Wire v5 — packet field, version macro, serializer (host-tested)

- `battery_types.h`: add `uint32_t runtime_to_empty_min;` as the v5 field (after `soh_pct_x100`, comment "v5 field"); extend the `BATTERY_TELEMETRY_VERSION` `#if` so it's `5U` when `CONFIG_BATTERY_RUNTIME_TO_EMPTY`, else 4/3 as today.
- `battery_serialize.h`: `#define BATTERY_SERIALIZE_V5_SIZE 38`; `BUF_SIZE → V5`; size ladder add `if (version>=5) return V5_SIZE`.
- `battery_serialize.c`: after the `>=4` block, add `if (version>=5 && buf_len>=V5_SIZE) { put_u32_le(buf+34, pkt->runtime_to_empty_min); }`. Mirror the existing helper style.
- Test: extend `tests/test_serialize*.c` (or new `test_serialize_v5`) — round-trip a v5 packet, assert size 38 and the uint32 at offset 34 (incl. `UINT32_MAX` sentinel).
- Commit `feat(runtime): wire v5 serializer + field`.

---

## Task 4: Transport sizes + MTU/ACL confs

- `battery_transport.h`: `WIRE_SIZE_V5 38`; `BATTERY_TRANSPORT_WIRE_SIZE → V5`.
- `battery_transport.c:18`: `_Static_assert(... >= BATTERY_SERIALIZE_V5_SIZE, ...)`.
- All 5 conf files: `CONFIG_BT_L2CAP_TX_MTU=41`, `CONFIG_BT_BUF_ACL_TX_SIZE=45`, `CONFIG_BT_BUF_ACL_RX_SIZE=45`. Update the `# v4 is 34 bytes…` comments to v5/38.
- Commit `feat(runtime): transport size + BLE MTU for wire v5`.

---

## Task 5: Telemetry integration

- In the collect path (`src/telemetry/battery_telemetry.c` / SoC estimator), where current is read each cycle: call `battery_runtime_update(current_ma_x100)` and, when `CONFIG_BATTERY_RUNTIME_TO_EMPTY`, set `packet->runtime_to_empty_min` from `battery_runtime_to_empty_min(coulomb_remaining, …)` — using `UINT32_MAX` on `NOT_AVAILABLE`. Initialize via `battery_runtime_reset()` in `battery_sdk_init`.
- Commit `feat(runtime): populate runtime_to_empty in telemetry (v5)`.

---

## Task 6: fuel_gauge driver

- `src/fuel_gauge/battery_fuel_gauge_zephyr.c`: add `case FUEL_GAUGE_RUNTIME_TO_EMPTY:` → if `CONFIG_BATTERY_RUNTIME_TO_EMPTY` and value available, `val->runtime_to_empty = minutes; break;` else `return -ENOTSUP;`. Guard with `#if defined(CONFIG_BATTERY_RUNTIME_TO_EMPTY)`.
- Commit `feat(runtime): expose RUNTIME_TO_EMPTY via fuel_gauge`.

---

## Task 7: Gateway v5 decode (pytest)

- Gateway packet decoder: accept length **38** → version 5 → unpack `runtime_to_empty_min` (uint32 LE @ 34); `UINT32_MAX` → `None`/"n/a". Add InfluxDB field `runtime_to_empty_min`.
- Tests: a v5 (38-byte) decode test + sentinel handling. Run `cd gateway && python -m pytest`.
- Commit `feat(gateway): decode wire v5 runtime_to_empty`.

---

## Task 8: Grafana "Time to Empty" panel

- Add a stat/timeseries panel to BOTH `cloud/grafana/provisioning/dashboards/battery.json` and `gateway/grafana/ibattery-dashboard.json` querying `runtime_to_empty_min` (display minutes/hours; show "—" for null).
- Commit `feat(grafana): Time to Empty panel`.

---

## Task 9: Serial print (guarded)

- `app/src/main.c`: under `#if IS_ENABLED(CONFIG_BATTERY_RUNTIME_TO_EMPTY)`, append ` RTE=<min>min` (or `RTE=n/a`) to the telemetry line.
- Commit `feat(runtime): serial print`.

---

## Task 10: Verify + docs

- Host suites green; `python3 scripts/check_build_sync.py` in sync; default-off build unchanged; an `nucleo_l476rg_ble_extadc.conf + CONFIG_BATTERY_RUNTIME_TO_EMPTY=y` build links.
- Docs: `SDK_API.md` (the API + fuel_gauge property now supported + wire v5 table), `README.md` wire-format list, `CLAUDE.md` Kconfig table + Wire Format (add v5), `RELEASE_NOTES.md` (Unreleased), `USE_CASES.md` (time-remaining now exists — update the edge-case note), `docs/ROADMAP.md` (mark the runtime-to-empty item done).
- Commit `docs: runtime-to-empty`.

---

## e2e (hardware, after merge-ready)
Build `nucleo_l476rg_ble_extadc.conf` + `CONFIG_BATTERY_RUNTIME_TO_EMPTY=y`, PPK2/INA219 rig: confirm `RTE` on serial under load, `UINT32_MAX`→n/a when idle, then over BLE → InfluxDB (`runtime_to_empty_min`) → the Grafana panel. Capture to `docs/captures/`.

## Out of scope
Runtime-to-full; multi-rate load models.
