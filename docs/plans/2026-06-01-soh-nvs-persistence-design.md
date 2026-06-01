# Design: NVS persistence for State of Health (SoH)

**Date:** 2026-06-01
**Status:** Approved (brainstorming complete)
**Tier:** FREE (core SDK / on-device; no fleet-SaaS or commercial-pack territory)
**Builds on:** Phase 8d SoH (v0.11.0), NVS HAL, cycle-counter persistence pattern

## Problem

`battery_soh.c` learns usable capacity from full→empty excursions but keeps
`g_learned_x100` in static RAM only. `battery_soh_init()` resets it to rated
capacity on every boot, so a power-cycle discards all learned health and SoH
snaps back to 100% until the cell is fully cycled again. Goal: persist the
learned capacity across reboots, best-effort, with zero new RAM and no change
to existing return contracts.

## Decisions

- **Persist format — learned + rated guard.** Two NVS keys hold the learned
  capacity and the rated capacity it was learned against. On boot, the learned
  value is restored only if the stored rated matches the current
  `CONFIG_BATTERY_CAPACITY_MAH`; otherwise first-boot semantics apply
  (`learned = rated`). Cheap insurance against reflashing a different battery
  profile and feeding a stale/garbage SoH.
- **Build gate — broaden to `CYCLE_COUNTER || SOC_SOH`.** Today the real NVS
  source compiles only when `CONFIG_BATTERY_CYCLE_COUNTER=y`; a SoH build
  without the cycle counter silently gets the stub. Broaden the gate so SoH
  builds get real NVS. Side effect: in a SoH build the coulomb counter's
  dormant NVS restore also activates — benign, since the restored mAh only
  seeds SoC in the mid-range window before the first anchor edge recalibrates
  it (and is a bonus: SoC continuity across reboot). Coulomb-only builds (no
  SoH, no cycle counter) are unchanged.
- **Persistence ownership — SoH module owns it** (Approach 1), mirroring
  `battery_cycle_counter.c`. Rejected: estimator-layer persistence (spreads SoH
  internals, widens init signature) and a unified versioned state blob (YAGNI,
  refactors three working modules).

## §1 Architecture & data

- **NVS keys** (`src/hal/battery_hal_nvs.h`):
  `BATTERY_NVS_KEY_SOH_LEARNED = 3`, `BATTERY_NVS_KEY_SOH_RATED = 4`.
- **`battery_soh.c`** includes `../hal/battery_hal_nvs.h`. State unchanged
  (`g_rated_x100`, `g_learned_x100`, `g_armed`, `g_initialized`).
- **`battery_soh_init(rated)`**: set `g_rated = rated`, `g_learned = rated`
  (default). `battery_hal_nvs_init()`; if OK, read `SOH_RATED` + `SOH_LEARNED`.
  If both present **and** stored rated == current rated → `g_learned =
  stored_learned`. Else leave at rated and best-effort write the current rated
  (+ learned) so future boots match. Never fail init on an NVS error — degrade
  to RAM-only, mirroring the cycle counter.
- **Build gate**: in both `CMakeLists.txt` and `app/CMakeLists.txt`, compile
  `battery_hal_nvs_zephyr.c` when `CONFIG_BATTERY_CYCLE_COUNTER OR
  CONFIG_BATTERY_SOC_SOH`, else `battery_hal_nvs_stub.c`.

## §2 Write path & error handling

- **Valid excursion** (`battery_soh_observe_empty_anchor`): after the EMA
  update, best-effort `battery_hal_nvs_write_u32(SOH_LEARNED, learned)`.
  Rejected/implausible excursions don't write (learned unchanged). Excursions
  are rare (one per full discharge), so no rate-limiting is needed — same
  rationale as the cycle counter.
- **`battery_soh_reset()`**: `g_learned = g_rated`, then best-effort write
  `SOH_LEARNED = rated` so the reset survives reboot.
- **`SOH_RATED`** is written only when absent or different from the current
  config (first boot / profile change), not on every excursion.
- **Error handling**: every NVS call is `(void)`-cast best-effort. `g_learned`
  is always valid in RAM regardless of NVS outcome. Read miss → first-boot
  defaults; write failure → silently RAM-only this session. No path returns a
  new error or changes existing return contracts. `learned_x100` is always
  positive and ≤120 % of rated, so the `int32 → uint32` round-trip through the
  `u32` API is lossless.

## §3 Testing (TDD, host)

- **Upgrade `tests/mocks/mock_nvs.c`** to key-aware (small fixed key→{value,
  present} array). Existing single-value control helpers map to a default key
  so `test_coulomb` / `test_cycle_counter` stay green. Add
  `mock_nvs_simulate_reboot()` — clears nothing in the stored map (flash
  survives) but lets a test re-`init` the module (RAM does not survive).
- **New failing tests first** — `tests/test_soh_persistence.c` (Unity), wired
  into `tests/CMakeLists.txt`:
  1. Learn a faded capacity → reboot → SoH restores learned, not 100 %.
  2. Stored rated ≠ current rated → restored value discarded, SoH = 100 %.
  3. `reset()` → reboot → stays 100 % (reset persisted).
  4. NVS read-miss (first boot) → learned = rated, no crash.
  5. NVS write failure → in-session learning still works, not persisted.
  6. Regression: coulomb + cycle-counter suites still pass on the key-aware mock.
- **Then implement** to green.
- **On-device** (needs the wired NUCLEO): flash `build-stm32-ble-cur` with
  `CONFIG_BATTERY_SOC_SOH=y`, drive a full→empty excursion, power-cycle,
  confirm SoH restores and coulomb-restore is benign. Capture evidence under
  `docs/captures/`.

## Constraints honored

- **0 new static RAM** (reuses existing globals).
- Flash cost ≈ cycle-counter delta (a handful of NVS calls).
- No heap, integer-only, ~120 B RAM budget preserved.

## Out of scope

- Partial-excursion learning (separate roadmap item).
- Unified/versioned multi-module state blob (YAGNI).
- Persisting `g_armed` across reboot (a mid-excursion reboot intentionally
  abandons that excursion).
