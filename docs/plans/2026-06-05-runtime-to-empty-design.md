# Design — Runtime-to-empty ("time remaining")

**Date:** 2026-06-05
**Status:** Approved (brainstorm) → ready for implementation plan
**Branch:** `feature/runtime-to-empty`

## Goal

An on-device estimate of **minutes until the battery is empty**, exposed three
ways:
1. Native SDK API (`battery_runtime_to_empty_min()`),
2. The Zephyr `fuel_gauge` standard property `FUEL_GAUGE_RUNTIME_TO_EMPTY`
   (completes the read-only driver),
3. A new **wire-v5** BLE telemetry field → gateway → InfluxDB → Grafana.

Opt-in (`CONFIG_BATTERY_RUNTIME_TO_EMPTY`, default n); no change to existing
behavior when disabled.

## Algorithm (integer-only, ~one int32 of new RAM)

- Maintain an **EMA of discharge current** (configurable α, same integer-EMA
  idiom as SoC fusion / SoH). Updated each telemetry cycle with the measured
  current.
- `minutes = remaining_mAh × 60 ÷ avg_discharge_mA`, computed in **int64** so the
  `×100` fixed-point operands cancel without overflow:
  `minutes = (int64)remaining_mah_x100 * 60 / avg_current_ma_x100`.
- Remaining charge comes from the coulomb counter (`battery_coulomb_get_mah_x100`,
  Q-as-remaining). Current from the telemetry/current HAL (positive = discharge).

### Edge cases → "not available"
- Smoothed current **at or below an idle threshold** (`CONFIG_BATTERY_RUNTIME_IDLE_THRESHOLD_MA_X100`,
  **default 0** — i.e. only current ≤ 0 (idle/charging) is "not discharging"; a
  positive default like 1.00 mA was rejected because it would mark a normal
  ~0.1 mA coin-cell draw as idle. Raise it to add a sub-mA noise-floor deadband.)
  → not meaningfully discharging.
- **Charging** (current ≤ 0 in iBattery's positive-discharge convention).
- No current sensor / coulomb path (the Kconfig dependency prevents this build).
- Remaining ≤ 0 → 0 minutes.
- Result that would overflow uint32 minutes → clamp to `UINT32_MAX - 1`
  (kept distinct from the sentinel).

## Exposure + sentinel encoding

| Surface | Encoding |
|---|---|
| Native API `battery_runtime_to_empty_min(uint32_t *out)` | returns `BATTERY_STATUS_OK` + minutes, or `BATTERY_STATUS_NOT_AVAILABLE` when idle/charging |
| `fuel_gauge` `FUEL_GAUGE_RUNTIME_TO_EMPTY` | minutes (uint32), or `-ENOTSUP` when not discharging |
| Wire v5 field `runtime_to_empty_min` (uint32 LE, offset 34) | minutes, or **`UINT32_MAX` = "not available"** |

### Wire v5 (38 bytes)
v5 = v4 (34 bytes) **+ `runtime_to_empty_min` uint32 LE at offset 34**. It is
cumulative: v5 still carries the v4 `soh_pct_x100` field at offset 32 (zero when
SoH is disabled). Version selection becomes:
- **5** when `CONFIG_BATTERY_RUNTIME_TO_EMPTY` is enabled,
- else **4** when `CONFIG_BATTERY_SOC_SOH`,
- else **3** (with current sense) / 2 / 1 as today.

Implications (the v5 checklist, all in the plan):
- serializer: `BATTERY_SERIALIZE_V5_SIZE = 38`, write the field, bump version logic.
- transport: `BATTERY_TRANSPORT_WIRE_SIZE_V5 = 38`, update `BATTERY_TRANSPORT_WIRE_SIZE`
  + `_Static_assert(... >= V5_SIZE)`.
- **BLE MTU**: `CONFIG_BT_L2CAP_TX_MTU` 37→**41** (38-byte payload + 3 ATT header),
  ACL buffers 41→**45**, in every `*.conf` that sets them.
- gateway decoder: accept length 38 → decode `runtime_to_empty_min`.
- both Grafana dashboards: add a **"Time to Empty"** panel.
- `app/src/main.c`: print it (guarded).

## Components (all gated by `CONFIG_BATTERY_RUNTIME_TO_EMPTY`, default n)

| File | Purpose |
|---|---|
| `include/battery_sdk/battery_runtime.h` | public API: `battery_runtime_to_empty_min()`, `battery_runtime_update()` (or fold update into the estimator) |
| `src/intelligence/battery_runtime.c` | EMA state + the estimate; integer-only |
| `app/Kconfig.battery` | `CONFIG_BATTERY_RUNTIME_TO_EMPTY` (depends on `BATTERY_SOC_COULOMB`), `…_EMA_ALPHA_X1000`, `…_IDLE_THRESHOLD_MA_X100` |
| `CMakeLists.txt` + `app/CMakeLists.txt` | compile the source under the gate, both paths (drift-guard-clean) |
| serializer + transport headers/src | wire v5 (see above) |
| `src/fuel_gauge/battery_fuel_gauge_zephyr.c` | implement `FUEL_GAUGE_RUNTIME_TO_EMPTY` |
| gateway decoder + InfluxDB writer + dashboards | v5 field end-to-end |
| `app/src/main.c` | guarded serial print |

## Testing (full coverage — explicit owner ask)

- **Host unit tests** (`tests/test_runtime.c`): EMA smoothing; the int64 division;
  edge cases (idle→NOT_AVAILABLE, charging→NOT_AVAILABLE, remaining=0→0, overflow
  clamp); threshold boundary.
- **Serialize v5** test: round-trip a v5 packet (38 bytes), field at offset 34,
  sentinel value.
- **Gateway tests**: decode a v5 packet (length 38) + the new field; sentinel
  handling.
- **e2e** (hardware): build the v5 + fuel_gauge image, drive the PPK2/INA219 rig,
  confirm `runtime_to_empty` on serial → over BLE → InfluxDB → the Grafana panel,
  and that idle/charging shows "not available". Same playbook as the SoH e2e.

## Out of scope (YAGNI)
- Runtime-to-**full** (charging direction).
- A load-model / multi-rate prediction beyond the single EMA.

## Risks / to-confirm in the plan
- Exact current EMA wiring: where `battery_runtime_update()` is called in the
  collect path, and whether to reuse an existing smoothed current.
- The version-selection code currently branches 3/4; extend to 5 cleanly.
- Enumerate every `*.conf` that sets `CONFIG_BT_L2CAP_TX_MTU`/ACL so none is missed.
- Confirm `BATTERY_STATUS_NOT_AVAILABLE` exists or add it to `battery_status.h`.
