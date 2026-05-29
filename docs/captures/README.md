# Hardware Capture Evidence

Archived serial-telemetry captures and bench evidence used to validate
firmware behavior on real hardware. These are kept in the repo as
gold-standard "evidence of fix" for major releases — each file is a
deterministic record of what came off the device at a particular
firmware version.

| File | What it shows | Related release |
|------|---------------|-----------------|
| `2026-05-29-v0.8.3-q-pinned-bug-evidence.log` | 5-minute loaded telemetry capture (151 samples) on NUCLEO-L476RG with v0.8.3 firmware: `I=2.80 mA` stable under load, `flags=0x00000000`, but **`Q` pinned at `220.00 mAh`** because the SoC estimator's full anchor re-fires every sample on CR2032 (issue [#1](https://github.com/aliaksandr-liapin/ibattery-sdk/issues/1)). | v0.8.3 |
| `2026-05-29-v0.8.4-q-ticks-down-fix-evidence.log` | Same rig, same load, 5-minute capture with v0.8.4 firmware: `Q = 219.98 → 219.75 mAh` (Δ = −0.23 mAh, matches theoretical 0.233 mAh at 2.80 mA load), SoC tracks `99.99% → 99.88%`. Proves both Bug A (Q-as-remaining sign flip) and Bug B (one-shot anchor) fixes work end-to-end on hardware. | v0.8.4 |
| `2026-05-29-v0.10.0-drift-correction.log` | Same rig, FUSION=y, 5-minute autonomous capture. `Q = 219.92 → 219.68 mAh` (Δ = −0.24 mAh ≈ theory), `flags=0x00000000` throughout. Trajectory matches v0.8.4 within rounding — fusion runs cleanly without disturbing the underlying Q integration when voltage-LUT and coulomb already agree. | v0.10.0 |
| `2026-05-29-v0.10.0-load-vs-rest.log` | FUSION=y, user-toggled load (30 s on / 30 s off, 10 transitions in 5 minutes). All 10 transitions detected by the post-analysis (current jumps cleanly between 2.80 mA and 0.10 mA). Demonstrates the adaptive α flipping between `ALPHA_LOAD` and `ALPHA_REST` based on current magnitude. SoC stays flat during rest periods (no integration), ticks down during load periods — the visible Phase 8c value prop on real silicon. | v0.10.0 |
| `2026-05-29-v0.10.0-fusion-off-regression.log` | FUSION=n, 5-minute autonomous capture under steady load. `Q = 219.98 → 219.75 mAh`, SoC = `99.99% → 99.88%`, flash = 73556 B — **byte-for-byte identical** to v0.8.4's `q-ticks-down-fix` evidence to two decimal places. Proves Phase 8c is strictly opt-in with zero impact when disabled. | v0.10.0 |

## How to add new captures

Use the naming convention `YYYY-MM-DD-vX.Y.Z-short-description.log` so the
filename sorts chronologically and is self-describing. Keep the file
plain-text (serial dump or structured table) — easy to grep, diff, and
review without special tooling.

Add a row to the table above documenting what the capture shows and the
release it informed.

## Why archive these in the repo

These captures are small (< 2 KB each), evidence-heavy, and irreplaceable
once the hardware is unwired or the firmware is overwritten. They're the
permanent answer to "did the fix actually work on real silicon?" — much
more valuable than chat transcripts or release-note prose, which describe
what the captures show rather than being the captures themselves.
