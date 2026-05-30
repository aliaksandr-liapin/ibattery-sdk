# Phase 8d: On-device State of Health (Capacity-Fade Learning) Design

**Status:** Approved design (brainstorm) — not yet implemented
**Date:** 2026-05-29
**Depends on:** Phase 8a coulomb counting (`CONFIG_BATTERY_SOC_COULOMB`)

## Overview

Phases 8a–8c estimate *state* (the current SoC). Phase 8d estimates a
*parameter*: the battery's true usable capacity, which fades 10–20% over
service life. With today's fixed compile-time `CONFIG_BATTERY_CAPACITY_MAH`,
an aged cell reads optimistically — it anchors to 100% when "full" but the
integrator then counts down against a capacity the battery no longer has.

Phase 8d learns the actual capacity from discharge excursions and exposes a
**State of Health (SoH)** value. SoH/capacity-fade is the on-device primitive
behind fleet predictive-maintenance ("which devices need battery
replacement?") — the capability that feeds the ROADMAP's SaaS fleet-monitoring
and "advanced SoC" commercial-license models.

## Problem

The coulomb integrator assumes `rated_capacity`. A healthy cell hits the
empty-voltage anchor right as remaining charge `Q ≈ 0`. An aged cell hits
empty-voltage *early* — while the integrator still reads `Q > 0` — because it
counted down against a capacity the cell no longer holds. That gap is the
fade, and nothing in 8a–8c captures it.

## Solution

### The core insight

The coulomb counter integrates **real measured current** (INA219), so the
charge that flows between a full anchor and an empty anchor *is* the battery's
true usable capacity for that cycle, independent of the rated constant.

With Q-as-remaining semantics:

- At the **full** anchor, `Q` is reset to `rated_capacity`.
- The battery discharges; `Q` ticks down by the integrated current.
- At the **empty** anchor, read `Q` *just before* it resets to 0:

```
measured_capacity = rated_capacity − Q_just_before_empty_reset
```

Healthy cell → `Q_before_empty ≈ 0` → measured ≈ rated. Aged 80% cell → hits
empty-voltage while `Q ≈ 20%` → measured ≈ 0.8·rated. The difference is the
fade.

### Algorithm

On a valid **full→empty** excursion:

```
measured_x100 = rated_x100 − q_before_empty_x100
learned_x100  = learned_x100 + (measured_x100 − learned_x100) * alpha_soh / 1000
soh_pct_x100  = learned_x100 * 10000 / rated_x100
```

- EMA smoothing (`alpha_soh`, x1000, Kconfig-tunable) guards against a single
  anomalous excursion while converging in a few cycles.
- Integer-only; round-to-nearest in the EMA divide to avoid one-sided bias
  (same lesson as 8c).

### Guards

- Update only when a full anchor genuinely preceded the empty anchor — reuse
  the estimator's existing edge-detected `g_full_anchor_active` /
  `g_empty_anchor_active` flags.
- Reject implausible measurements (`< 0.30·rated` or `> 1.20·rated`) as
  partial cycles / noise.
- Discharge-direction only for the MVP.
- Initialize `learned = rated` (SoH 100%) until the first valid excursion.

## Architecture

```
SoC estimator (anchor edge logic, 8a)
   │  on empty-anchor edge, if full-anchored first:
   ▼
battery_soh_observe_excursion(rated_x100, q_before_empty_x100)   (8d, NEW)
   │  EMA update of learned capacity
   ▼
battery_soh_get_pct_x100() / get_learned_capacity_mah_x100()      (public API)
```

- New module `src/intelligence/battery_soh.c` + header
  `include/battery_sdk/battery_soh.h`.
- The estimator calls `battery_soh_observe_excursion()` at the empty-anchor
  edge (it already has `Q` and the rated capacity in scope there).
- A few `int32` file-scope statics (learned capacity + init flag). No heap,
  integer-only — consistent with SDK constraints.

### Public API (the cloud-ready seam)

```c
int battery_soh_get_pct_x100(uint16_t *soh_x100);
int battery_soh_get_learned_capacity_mah_x100(int32_t *cap_x100);
int battery_soh_reset(void);
```

RAM-only for the MVP. Scaling to cloud later is a thin additive layer: add a
`soh_pct_x100` field to a wire **v4** (34 bytes) + gateway decode + InfluxDB
field + a Grafana "State of Health" panel, reusing the v0.8.5 pattern. The
getter makes this a drop-in with no rework. **Not built in the MVP.**

## Kconfig surface

| Option | Type | Default | Notes |
|---|---|---|---|
| `BATTERY_SOC_SOH` | bool | `n` | Master enable; `depends on BATTERY_SOC_COULOMB` |
| `BATTERY_SOC_SOH_ALPHA_X1000` | int | TBD (~500) | EMA blend per valid excursion; `range 0 1000` |
| `BATTERY_SOC_SOH_REJECT_LO_PCT` | int | 30 | Reject measured below this % of rated |
| `BATTERY_SOC_SOH_REJECT_HI_PCT` | int | 120 | Reject measured above this % of rated |

## Testing strategy

### Unit tests — `tests/test_soc_soh.c` (host, Unity)

- Healthy excursion (`q_before_empty ≈ 0`) → SoH → ~100%.
- Aged excursion (`q_before_empty = 20% of rated`) → SoH → ~80%.
- EMA convergence over several consecutive aged excursions.
- Guard: implausible / partial excursions rejected (no update).
- Init: SoH = 100% before the first excursion.
- Round-to-nearest EMA has no accumulating bias over many updates.

### Hardware validation

Cycle a LiPo full→empty and confirm SoH tracks the measured capacity.
**Honest caveat:** convergence needs deep discharge cycles, so it is slow on
a CR2032 (months); a cycled LiPo demonstrates it in reasonable time.

## Risks and deferred work

### Risks

- **Slow convergence** — inherent to full-cycle learning; documented, not a
  bug. Future Approach 2 (partial-excursion ΔQ/ΔSoC) speeds it up.
- **Anchor dependence** — SoH only updates when both anchors fire cleanly; a
  device that never reaches empty never learns. Acceptable for an MVP.

### Deferred (not 8d MVP scope)

- Cloud telemetry (wire v4 + gateway + Grafana SoH panel).
- Non-volatile persistence (Zephyr NVS/settings) for offline-edge devices.
- Partial-excursion learning (Approach 2).
- Charge-direction learning for rechargeable cells.
- Current-sensor offset auto-calibration (the other 8d candidate) — separate.

## Release scope (MVP)

On-device SoH learning core: new module + API + opt-in Kconfig + host unit
tests, full-excursion + EMA (Approach 1). RAM-only. Designed so the cloud
layer drops in later without rework.

## References

- `src/intelligence/battery_soc_estimator.c` — anchor edge logic (8a) where
  the observe hook attaches.
- `src/intelligence/battery_coulomb.c` — Q-as-remaining integrator.
- `docs/plans/2026-05-29-phase-8c-fusion-design.md` — prior parameter/taste
  precedent (complementary filter over Kalman).
- `docs/ROADMAP.md` — monetization models (SaaS fleet monitoring, commercial
  license) this capability underpins.
