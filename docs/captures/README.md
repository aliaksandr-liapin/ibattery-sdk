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
