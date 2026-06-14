# Post-Worthy Milestones

A running list of milestones, bugs, and design decisions worth writing up
(dev.to + LinkedIn, sometimes r/embedded / r/Zephyr_RTOS / Hacker News). Update it
each release alongside `RELEASE_NOTES.md` / `CHANGELOG`.

**The honest "here's what was hard / what I got wrong" angle performs best.**

**Distribution is room-specific:**
- **LinkedIn** — polished infographics are fine.
- **Reddit / Hacker News** — NO marketing graphics (spam filters + community
  pushback). Plain text + genuine technical screenshots (Grafana, serial/scope
  captures, architecture diagrams). New/low-karma accounts get auto-filtered →
  participate before posting.

## Published
| Story | Angle | Link |
|---|---|---|
| Battery admits it's only 73% healthy | on-device SoH (capacity-fade) parameter estimation, end-to-end to Grafana | dev.to (`docs/articles/2026-06-04-soh-parameter-estimation.md`) |
| When soldering doesn't fix it: swap the MCU | hardware debugging — isolating a per-unit GPIO defect | dev.to (`docs/articles/2026-05-29-swap-the-mcu*.md`) |
| Coulomb counting & the hardware drama | building a coulomb counter + the bring-up pain | `docs/articles/2026-05-11-coulomb-counting-and-hardware-drama.md` |

## Candidates (not yet written) — strongest first
- **"My 'time remaining' feature told users their battery would last 2.5 years."**
  The runtime-to-empty idle-current-floor bug: an EMA decaying through tiny
  currents on load-removal produced multi-year spikes; the fix bounds the estimate
  to `Q/floor`. Caught only on hardware. Great what-I-got-wrong story. *(venue: dev.to / r/embedded)*
- **"Teaching a fuel gauge to notice you changed the battery."** Swap-aware SoH:
  no dedicated swap signal, so infer from a boot SoC jump vs an NVS baseline;
  the false-positive control (same-cell power-cycle must NOT wipe health); and the
  on-hardware finding that boot SoC re-anchors from voltage. *(dev.to)*
- **"The standard Zephyr `fuel_gauge` API has no battery-health property — so I added one."**
  Conforming to the standard interface while extending it (custom SoH prop). Honest
  note: I almost claimed the API had SoH; reading the header first prevented a false
  claim. *(dev.to / r/Zephyr_RTOS)*
- **"Fusing voltage and coulomb without going full Kalman."** Why a complementary
  filter with current-adaptive α beat a Kalman filter for this noise model. *(dev.to / HN)*
- **"Three latent bugs because I only ever tested over serial."** The v0.10.1
  BLE-bring-up: MTU too small, macOS service-UUID matching, re-advertise-after-disconnect.
  Lesson: a transport you don't exercise hides bugs. *(r/embedded)*
- **"Why my dashboard showed the wrong board on macOS."** CoreBluetooth leaves the
  advertised GAP name empty → read the GATT Device Name (0x2A00) after connecting.
  Niche but real cross-platform BLE gotcha. *(short post / r/embedded)*

## Reminder
When a release ships a new MCU target, a gnarly bug, or a non-obvious design
decision, add a one-liner here with the angle and the best venue.
