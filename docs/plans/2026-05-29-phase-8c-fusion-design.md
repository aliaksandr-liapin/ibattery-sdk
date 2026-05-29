# Phase 8c: Voltage + Coulomb Signal Fusion Design

**Date:** 2026-05-29
**Status:** Approved (brainstorming session 2026-05-29)
**Phase:** 8c (of 8a/8b/8c Advanced SoC roadmap)
**Target release:** v0.10.0

## Overview

Continuously fuses the voltage-LUT SoC signal with the coulomb-counter SoC signal into a single estimate that's more accurate than either alone — specifically targeting **mid-discharge drift** in the coulomb integrator.

Today's "coulomb primary, voltage anchor at endpoints" pattern (Phase 8a, finalized in v0.8.4) corrects coulomb-counter integration error only when voltage hits the full or empty thresholds. For a typical battery-powered IoT device — a CR2032-powered BLE sensor running for months without ever reaching either endpoint — that means *no correction signal for the entire deployment*. Integration error accumulates unchecked and becomes the cumulative SoC error users see at month 3.

Phase 8c adds **continuous correction**: every sample, voltage information pulls the SoC estimate proportional to how much we should trust voltage in the current operating conditions (mainly: load magnitude). A complementary filter with current-adaptive blend coefficient.

## Problem

After v0.8.4, the SoC estimator computes:

```
soc = Q / capacity,  where Q is the coulomb accumulator (mAh remaining)
```

`Q` is updated each sample by integrating measured current. Two correction events exist:

1. **Full anchor** — when `V ≥ V_full` AND `|I| < I_full` (one-shot, edge-detected): `Q := capacity`
2. **Empty anchor** — when `V ≤ V_empty` (one-shot, edge-detected): `Q := 0`

For a deployment that stays in the middle of the discharge curve indefinitely, neither anchor ever fires. The only signal driving `Q` is the integrator. Integration is unbiased in theory but in practice accumulates error from:
- INA219 calibration drift (~±1% per spec)
- Sample-interval jitter (the trapezoidal rule assumes constant dt)
- Quantization rounding (the `g_remainder` tracking helps but doesn't eliminate)

Over weeks of operation, that drift becomes user-visible SoC error.

## Solution

### The fusion math

Complementary filter with current-adaptive α:

```
α = α_rest          if |I| < I_thresh         (battery at rest, voltage is informative)
  = α_load          if |I| ≥ I_thresh         (battery under load, voltage sags from IR drop)

soc_fused = α · soc_v_lut + (1 - α) · soc_c_coulomb
```

`α` is the per-sample pull toward voltage. Higher α = faster drift correction, slower under-load tracking. The current-magnitude split captures the physical reality that voltage-LUT accuracy degrades sharply when current is non-trivial (terminal voltage = OCV − I × R_internal).

### Integer-math implementation

α represented in **x1000 units** (`0`–`1000`) for 0.1% blend resolution:

```c
uint16_t soc_fused_x100 = (uint16_t)(
    ((uint32_t)alpha_x1000 * soc_v_x100
   + (uint32_t)(1000 - alpha_x1000) * soc_c_x100)
   / 1000
);
```

Overflow analysis: max product is `1000 × 10000 = 10,000,000`. Sum of two such products: `20,000,000`. Fits in `uint32_t` (max `4,294,967,295`) with 214× headroom.

Precision: per-sample rounding error ≤ 1 x100 unit (= 0.01% SoC). No bias. Zero RAM cost — operation is memoryless, takes inputs from existing state.

### α selection

```c
uint16_t alpha_x1000 = (abs_current_x100 < SOC_FUSION_I_THRESH_X100)
                     ? SOC_FUSION_ALPHA_REST
                     : SOC_FUSION_ALPHA_LOAD;
```

Step function, not smooth interpolation — one comparison + one branch, matches the physical knee in IR-drop sensitivity. Smooth interpolation is YAGNI for v0.10.0.

## Architecture

Phase 8c slots into the existing call chain as a new stage between Phase 8a (coulomb counting) and Phase 8b (slew limiter):

```
voltage_mv ─► LUT ─► soc_v_x100 ──┐
                                   ├──► fusion ──► slew ──► soc_out
current_ma ─► coulomb ─► soc_c ───┘  (NEW)        (8b)
                  ▲
            anchor (8a)
```

**Key shape decisions:**

- **Fusion is opt-in via Kconfig.** `CONFIG_BATTERY_SOC_FUSION=n` by default. Existing users get v0.8.4 behavior byte-for-byte.
- **Anchors stay.** The one-shot edge-detected full/empty anchors still fire at endpoints. They're high-confidence calibration events; fusion is continuous correction. Both useful.
- **Slew applies after fusion.** Fusion produces the multi-signal estimate; slew is the final smoother on display SoC. Same call-graph order as today.
- **Module boundary:** `src/intelligence/battery_soc_fusion.{c,h}` for the math, `src/intelligence/battery_soc_estimator.c` adds a single new `#if` block.

### Composition with anchors and slew

| Scenario | Behavior |
|----------|----------|
| Anchor fires (one-shot) | `Q := capacity` or `0`. Voltage also near endpoint → `soc_v ≈ soc_c` → fusion is no-op when both signals agree. |
| First sample after init | Coulomb seeded from LUT: `soc_c := lut_soc`. Fusion: `blend(lut_soc, lut_soc) = lut_soc` — no-op by construction. No special "first sample" guard needed. |
| Voltage read fails | Return error (no LUT, no fusion possible). Same as v0.8.4. |
| Current read fails | Fall back to LUT-only with slew. Fusion needs `\|I\|` for α selection — can't run without it. |
| Coulomb returns error | Same fallback — LUT-only. |
| `FUSION=n` | Estimator behaves exactly as v0.8.4 — the fusion branch is not compiled in. |

## Kconfig surface

```kconfig
config BATTERY_SOC_FUSION
    bool "Phase 8c: voltage + coulomb signal fusion"
    depends on BATTERY_SOC_COULOMB
    default n
    help
      Continuously fuses voltage-LUT SoC with coulomb-counter SoC to
      correct mid-discharge drift in the coulomb integrator. Trust
      shifts toward voltage when the battery is at rest (current below
      threshold) and toward coulomb under load (voltage sag is real).

      Composes with the v0.8.4 anchor calibration (which still fires
      at endpoints) and the Phase 8b slew limiter (which still applies
      after fusion).

      Defaults to off. Enable per build to opt in.

if BATTERY_SOC_FUSION

config BATTERY_SOC_FUSION_ALPHA_REST_X1000
    int "Fusion alpha (x1000) when battery is at rest"
    default 50 if !BATTERY_CHEMISTRY_LIPO       # CR2032: 5.0% per sample
    default 30 if BATTERY_CHEMISTRY_LIPO        # LiPo:   3.0% per sample
    range 0 1000

config BATTERY_SOC_FUSION_ALPHA_LOAD_X1000
    int "Fusion alpha (x1000) under load"
    default 5                                   # 0.5% per sample, both chemistries
    range 0 1000

config BATTERY_SOC_FUSION_I_THRESH_X100
    int "Current threshold (x100 mA) classifying rest vs load"
    default 200 if !BATTERY_CHEMISTRY_LIPO      # CR2032: 2.00 mA
    default 5000 if BATTERY_CHEMISTRY_LIPO      # LiPo:  50.00 mA
    range 0 1000000

endif
```

### Default rationale

**CR2032 (α_rest = 5%, α_load = 0.5%, I_thresh = 2 mA):**
- 2 mA threshold = boundary where CR2032 internal resistance (10–50 Ω) starts producing measurable terminal sag (≥20 mV)
- 5% α at rest = ~95% convergence in ~60 samples (2 minutes at 2 s intervals) — fast enough to correct visible drift, slow enough to ignore single-sample voltage noise
- 0.5% α under load = voltage barely contributes, coulomb in charge

**LiPo (α_rest = 3%, α_load = 0.5%, I_thresh = 50 mA):**
- Lower α_rest because LiPo OCV-vs-SoC curve is flatter in the middle plateau (3.6–3.8 V covers 20–80% SoC), so voltage is less informative even at rest
- 50 mA threshold reuses existing `SOC_ANCHOR_FULL_I_X100` for LiPo — anchor logic and fusion classification share one boundary

## Testing strategy

TDD-first. Three test scopes:

### Unit tests — `tests/test_soc_fusion.c` (~10 tests)

Pure unit tests for `battery_soc_fusion.c`:
- `test_alpha_0_returns_pure_coulomb` — α=0 ⇒ fused = soc_c
- `test_alpha_1000_returns_pure_voltage` — α=1000 ⇒ fused = soc_v
- `test_alpha_500_returns_exact_midpoint` — symmetric blend
- `test_alpha_50_pulls_5pct_toward_voltage` — concrete numeric check
- `test_no_overflow_at_max_values` — α=1000, soc_v=10000, soc_c=10000 ⇒ fused=10000 with no u32 overflow
- `test_select_alpha_rest_when_below_threshold`
- `test_select_alpha_load_when_above_threshold`
- `test_select_alpha_at_exact_threshold` (boundary)
- `test_select_alpha_handles_negative_current` (verify signed-current absolution)

### Integration tests — `tests/test_soc_estimator_fusion.c` (~6 tests)

Compiled with `CONFIG_BATTERY_SOC_FUSION=y`:
- `test_fusion_corrects_drift_at_rest` — Q biased to 50%, voltage shows 90%, expect monotone convergence toward 90% over ~60 samples
- `test_fusion_passive_under_load` — Q at 50%, voltage shows 30% (load-induced sag), expect SoC to stay near 50% (the coulomb truth)
- `test_fusion_noop_on_first_sample` — Fresh init, expect `fused == lut_soc` on first call
- `test_anchor_still_fires_with_fusion_enabled` — verify v0.8.4 anchor behavior preserved
- `test_anchor_does_not_refire_with_fusion_enabled` — confirms Bug B fix still holds
- `test_fusion_falls_back_to_lut_on_current_error` — graceful degradation

### Backward-compat — existing `tests/test_soc_coulomb.c` (unchanged)

Runs with `FUSION=n`. All 7 tests must continue to pass byte-for-byte. This is the regression guard.

**Total expected count after Phase 8c lands: 17 → ~33 host tests.**

### Hardware validation captures

Same rig as v0.8.4 (NUCLEO-L476RG + Adafruit INA219 + ~1 kΩ load). Three captures, saved to `docs/captures/`:

1. **`drift-correction.log`** — biased coulomb counter, voltage pulls it back over 5 minutes. Demonstrates the value prop.
2. **`load-vs-rest.log`** — alternate load on/off every 30 s for 5 min. Demonstrates adaptive α visibly.
3. **`fusion-off-regression.log`** — `FUSION=n` recompile, captures match v0.8.4 evidence byte-for-byte. Backward compat check.

## Risks and deferred work

### Risks for v0.10.0

| Risk | Severity | Mitigation |
|------|----------|------------|
| α defaults are wrong for some devices | Medium | All values are Kconfigs; field tuning is one config change + rebuild |
| Voltage LUT errors compound under cold/old cells | Medium | Phase 8a's existing `CONFIG_BATTERY_SOC_TEMP_COMP` covers cold; fusion respects whatever LUT path returns |
| Anchor+fusion at empty boundary briefly reports 0% during a load spike | Low | Pre-existing v0.8.4 issue; Phase 8c doesn't make it worse; slew limiter smooths display |
| Code complexity in estimator | Low | Module boundary keeps math in `battery_soc_fusion.c`; estimator just adds one `#if` block |

### Non-risks (proven in design review)

- **RAM:** 0 new bytes
- **Code size:** ~50 lines of fusion code, comparable to Phase 8b
- **CPU:** single multiply + compare per sample, negligible
- **Backward compatibility:** opt-in via Kconfig, byte-for-byte identical when disabled
- **Feedback loops between anchors and fusion:** none; fusion is memoryless

### Deferred to future phases

| Item | Likely future home |
|------|-------------------|
| Capacity-aging learning | Phase 8d if demand emerges |
| Coulomb counter calibration drift | Phase 8d |
| Additional voltage anchors beyond full/empty | Future patch if needed |
| Smooth α interpolation instead of step | Future patch if field data demands |
| Runtime α tuning via API | Probably never (compile-time is enough) |

## Release scope (v0.10.0)

**Version:** v0.10.0 — minor bump (new Kconfig surface, new module, but no API breakage).

**In scope:**
- Source: `battery_soc_fusion.{c,h}` (new), `battery_soc_estimator.c` (modified), `app/Kconfig` (+4 entries), `CMakeLists.txt` (+1 source)
- Tests: `test_soc_fusion.c` + `test_soc_estimator_fusion.c` (new), CMakeLists targets (new). 17 → ~33 host tests.
- Docs: `RELEASE_NOTES.md` (v0.10.0 entry), `ROADMAP.md` (Phase 8c struck through), `SDK_API.md` (new fusion module + Kconfigs), `docs/captures/` (3 new logs), `CLAUDE.md`, `README.md`, `docs/index.md`, `library.json` (0.9.1 → 0.10.0)
- Distribution: GitHub release v0.10.0 + PlatformIO publish

**Not in scope:**
- Promotional rollout (separate decision after the release stabilizes)
- nRF I2C remap experiment (independent task, would be v0.10.1 if it lands)
- Phase 8d capacity aging (separate phase)
- BLE-on-NUCLEO E2E (blocked on shield arrival)

### Implementation order (preview)

```
1.  Add Kconfig surface — confirm config plumbing
2.  Create battery_soc_fusion.h with the public function signature
3.  TDD red: test_soc_fusion.c, all tests fail
4.  Implement battery_soc_fusion.c, tests pass
5.  Add CMakeLists target, register test
6.  Run host suite — 17 + 10 = 27 tests pass
7.  TDD red: test_soc_estimator_fusion.c, integration tests fail
8.  Modify battery_soc_estimator.c, integration tests pass — 27 + 6 = 33 tests
9.  Build STM32 i2cscan firmware with FUSION=y
10. Flash NUCLEO, run 3 hardware validation captures
11. Update docs (RELEASE_NOTES, ROADMAP, SDK_API, CLAUDE.md, README, index.md)
12. Bump library.json to 0.10.0
13. Commit, tag v0.10.0, push, GH release
14. (Separate decision) PlatformIO publish
```

Estimated effort: ~3–4 hours for code + host tests (steps 1–9), ~30 min for hardware (10–11), ~30 min for release packaging (12–14). Half a day of focused work.

## References

- v0.8.4 release notes — coulomb counter fix and the bugs Phase 8c builds on
- `src/intelligence/battery_soc_estimator.c` — existing pattern Phase 8c extends
- `docs/plans/2026-04-13-advanced-soc-coulomb-counting-design.md` — Phase 8a design
- `docs/plans/2026-04-14-voltage-smoothing-design.md` — Phase 8b design (slew limiter that fusion composes with)
- `docs/captures/2026-05-29-v0.8.4-q-ticks-down-fix-evidence.log` — baseline behavior; Phase 8c hardware captures will be the diff against this
