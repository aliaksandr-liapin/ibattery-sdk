# Swap-aware SoH auto-reset — Design (Phase 1: primary cells, power-off swap)

Date: 2026-06-06
Status: SHIPPED in v0.16.0 (Phase 1) — host-tested, firmware-build-verified, and hardware E2E PASSED on NUCLEO-L476RG (PPK2 rig, incl. the same-cell false-positive control — `docs/captures/2026-06-06-swap-aware-soh-e2e.log`). Real-cell + all-boards validation logged as deferred QA.
Related: `docs/plans/2026-05-29-phase-8d-soh-design.md`,
         `docs/plans/2026-06-01-soh-nvs-persistence-design.md`,
         `docs/USE_CASES.md` ("Battery swap → you must call battery_soh_reset()")

## Problem

State of Health is learned per cell and persists in flash (NVS). Today a battery
swap has **no detection**: the SDK keeps reporting the *old* cell's health and
EMA-blends the next measurement into it. The only mitigation is the integrator
manually calling `battery_soh_reset()` on a swap — exactly the step people forget.
That manual step is the bug this feature removes.

## Goal

On boot, detect that the cell was swapped while the device was powered off, then
**auto-reset learned SoH to rated** and **raise a status flag**. Opt-in. No
wire-format change (the flag reuses the existing `status_flags` field).

Industry precedent: single-cell fuel gauges (e.g. Maxim/ADI MAX17048) detect
battery insertion and set a **reset-indicator bit** so the host reinitializes the
model for the new cell — auto-reinit *plus* a visible flag, not manual, not silent.
iBattery has no insertion-detect hardware, so it infers the swap from voltage/SoC,
which means detection must be conservative.

## Scope

- **Phase 1 (this design): primary cells (CR2032), power-off → swap → power-on.**
- **Out of scope (deferred):**
  - Rechargeable (LiPo) — needs charge-vs-swap disambiguation (Phase 2). See the
    constraint below: a LiPo charged while off looks identical to a swap under the
    Phase-1 rule, so this feature must not default-on for LiPo.
  - Hot-swap (swap while powered) — future on-demand feature; coin cells are
    effectively never hot-swapped without the device losing power.

## Detection (Approach A — persisted SoC baseline + boot delta)

Compare the first filtered SoC after boot against a persisted "last-seen SoC":

```
swap  ⇔  boot_soc_x100 − last_soc_x100 ≥ SWAP_THRESHOLD   (default 25.00%)
```

SoC (not raw voltage) is the signal because it is already median-filtered,
slew-limited, and chemistry-normalized via the LUT — the most stable, lowest
false-positive option, with a chemistry-independent threshold. For a primary cell
SoC only declines, so a real upward jump is unambiguous; 25% sits well above
rest-relaxation noise.

## Components

1. **New module `battery_swap`** (intelligence layer). Single responsibility:
   maintain the SoC baseline, run the boot check, and call `battery_soh_reset()`
   on a detected swap. Keeps `battery_soh` focused on capacity learning.

2. **New NVS key** `BATTERY_NVS_KEY_LAST_SOC` (u32, stores `soc_pct_x100`) on the
   existing `battery_hal_nvs_*` u32 layer (same layer as the SoH keys).

3. **Throttled baseline persist.** Persist the baseline only when SoC has *dropped*
   by ≥ `PERSIST_DELTA` (default 5%) since the last persisted value — not every
   sample. ~20 writes per full discharge → negligible flash wear. RAM holds the
   current baseline; NVS is best-effort (a failed write never changes a return code,
   matching the SoH-NVS contract).

4. **Boot check** `battery_swap_check_on_boot(uint16_t boot_soc_x100)`, run once on
   the first valid SoC after boot (not in init — init precedes any ADC read):
   - No persisted baseline (first boot ever) → no swap; stamp `boot_soc`.
   - `boot_soc − last_soc ≥ SWAP_THRESHOLD` → **swap**: `battery_soh_reset()`, set
     the swap flag, re-stamp baseline to `boot_soc`.
   - Otherwise → no-op (normal baseline persistence continues).

5. **Status flag** `BATTERY_STATUS_FLAG_BATTERY_SWAPPED` (new bit in `status_flags`).
   Set for the session when a swap is detected; travels in every telemetry packet
   (mirrors the MAX17048 reset-indicator bit). The gateway already decodes
   `status_flags`; surfacing it in Grafana is a later, optional step.

6. **Kconfig**
   - `CONFIG_BATTERY_SWAP_DETECT` — bool; depends on `BATTERY_SOC_SOH`;
     **`default y if BATTERY_CHEMISTRY_CR2032`, else n** (primary-only; LiPo waits
     for Phase 2).
   - `CONFIG_BATTERY_SWAP_SOC_THRESHOLD_PCT_X100` — int, default `2500` (25%).
   - `CONFIG_BATTERY_SWAP_PERSIST_DELTA_PCT_X100` — int, default `500` (5%).

## Critical constraint: primary-only this phase

Detection is "boot SoC jumped up." A **LiPo charged while powered off** would show
exactly that (SoC rose, no running charge state was observed) → false swap →
wrongly reset a valid SoH. Therefore `CONFIG_BATTERY_SWAP_DETECT` defaults on only
for `CR2032`; LiPo stays off until Phase 2 adds charge-vs-swap disambiguation
(using `BATTERY_POWER_STATE_CHARGING`/`CHARGED` history).

## Known limitations (to document in USE_CASES)

- Cannot detect a swap to a *similarly-depleted* used cell (no upward SoC jump).
- Power-off path only; hot-swap deferred.
- Primary-cell only this phase.

## Architecture / data flow

```
SDK collect cycle (per sample)
  SoC estimator → soc_pct_x100
    → battery_swap_update(soc)        // throttled baseline persist (on ≥delta drop)
    → (first sample only) battery_swap_check_on_boot(soc)
         → if swap: battery_soh_reset(); set BATTERY_STATUS_FLAG_BATTERY_SWAPPED
  telemetry assembles status_flags (incl. swap flag) → wire (unchanged versions)
```

No new wire field; no MTU change.

## Testing (TDD, host, Unity; NVS HAL mocked)

- first boot (no baseline) → no swap; baseline stamped.
- boot SoC jump ≥ threshold → swap: `battery_soh_reset()` called, flag set,
  baseline re-stamped to boot value.
- boot SoC within threshold of baseline → no swap.
- boot SoC lower than baseline (normal power-cycle, same cell) → no swap.
- persist throttling: baseline written only when SoC drops ≥ `PERSIST_DELTA`.
- flag set/clear semantics across a swap vs a clean boot.

## Non-goals

- No wire-format bump.
- No charge-vs-swap logic (Phase 2).
- No hot-swap (future on-demand).
