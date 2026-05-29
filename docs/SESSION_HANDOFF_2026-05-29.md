# Session Handoff — Phase 8 Series Complete

> **UPDATE (later, 2026-05-29): v0.10.1 shipped.** A follow-up session closed
> the BLE-on-NUCLEO E2E loop (was listed below as "blocked on shield"). v3
> telemetry now flows firmware → BLE → gateway → InfluxDB → Grafana on real
> hardware, with real current/coulomb. Fixed three latent BLE bugs (MTU 27→35,
> gateway service-UUID matching, re-advertise after disconnect). Tagged
> `v0.10.1`, GitHub released, PlatformIO published. Evidence:
> `docs/captures/2026-05-29-v0.10.1-ble-on-nucleo-e2e.log`. The menu item below
> is struck through; everything else here is still accurate history.

**Date:** 2026-05-29
**Last release:** v0.10.1 (tagged, GitHub released, PlatformIO published) — followed v0.10.0 the same day
**Status:** All work wrapped. Repo + remotes + registry + memory all synchronized. **Ready for any next direction.**

This document is the cold-start brief for the next chat session. Open the next chat with the one-line summary at the bottom, and that session will know enough to pick up cleanly without re-deriving context.

---

## The big picture (one paragraph)

The ibattery-sdk's Phase 8 advanced-SoC roadmap is fully closed end-to-end. Phase 8a (coulomb counting) is hardware-validated on NUCLEO-L476RG after the swap-the-MCU diagnostic revealed a per-unit GPIO defect on the original nRF52840-DK. Phase 8b (voltage smoothing) was already shipped in v0.9.0. Phase 8c (voltage+coulomb signal fusion via complementary filter with current-adaptive α) shipped today as v0.10.0, opt-in via `CONFIG_BATTERY_SOC_FUSION` (default off). The PlatformIO registry shows v0.10.0 as the latest installable version. 19 host tests + 67 gateway tests pass. 0 open GitHub issues. Five hardware capture logs preserved in `docs/captures/` as evidence-of-fix for the v0.8.x bug-fix arc and the v0.10.0 fusion validation.

---

## What's done

### Releases shipped today (2026-05-29, in order)

| Tag | What it did |
|---|---|
| `v0.8.3` | Phase 8a hardware-validated on NUCLEO-L476RG (swap-the-MCU breakthrough — original nRF DK has per-unit GPIO defect on P0.26/P0.27) |
| `v0.8.4` | Coulomb counter bug fix: Bug A (Q-as-remaining sign flip) + Bug B (one-shot anchor edge-detection). Closed issue #1. TDD regression test `tests/test_soc_coulomb_cr2032.c` added. |
| `v0.8.5` | Gateway persists `current_ma` and `coulomb_mah` to InfluxDB; Grafana dashboard gains Live Current + Remaining Charge panels. Closed issue #2. |
| `v0.9.1` | PlatformIO registry consolidation — `library.json` bumped so external users pulling via PIO get the union of v0.9.0 (Phase 8b) + the v0.8.x hardware-validation arc. |
| `v0.10.0` | Phase 8c voltage+coulomb signal fusion. Complementary filter with current-adaptive α. Opt-in via Kconfig. +56 B flash, 0 new RAM. Hardware-validated with 3 captures. |

### What's persisted in the repo

- **Source code**: `src/intelligence/battery_soc_fusion.c` + `include/battery_sdk/battery_soc_fusion.h` (new, Phase 8c)
- **Tests**: `tests/test_soc_fusion.c` (10 unit tests) + `tests/test_soc_estimator_fusion.c` (6 integration tests) + `tests/test_soc_coulomb_cr2032.c` (v0.8.4 regression)
- **Hardware capture evidence** in `docs/captures/`:
  - `2026-05-29-v0.8.3-q-pinned-bug-evidence.log` — Q stuck at 220 mAh (the bug)
  - `2026-05-29-v0.8.4-q-ticks-down-fix-evidence.log` — Q ticks 219.98 → 219.75 mAh (the fix)
  - `2026-05-29-v0.10.0-drift-correction.log` — FUSION=y baseline, ΔQ matches theory
  - `2026-05-29-v0.10.0-load-vs-rest.log` — 10/10 toggles cleanly detected, adaptive α verified
  - `2026-05-29-v0.10.0-fusion-off-regression.log` — FUSION=n byte-identical to v0.8.4
- **Design docs** in `docs/plans/`:
  - `2026-05-29-phase-8c-fusion-design.md` (the approved design from brainstorming)
  - `2026-05-29-phase-8c-fusion-plan.md` (the TDD implementation plan)
- **Promotional drafts** in `docs/articles/`:
  - `2026-05-29-swap-the-mcu.md` (dev.to article — published)
  - `2026-05-29-swap-the-mcu-discord.md` (Discord post — posted to Zephyr #general)
  - `2026-05-29-swap-the-mcu-hackaday.md` (Hackaday tip email — sent)
  - `2026-05-29-swap-the-mcu-reddit.md` (Reddit draft — parked due to spam-filter Rule 4)
- **Updated docs**: `RELEASE_NOTES.md`, `ROADMAP.md`, `SDK_API.md`, `HARDWARE_TROUBLESHOOTING.md` (swap-the-MCU section), `CLAUDE.md`, `README.md`, `docs/index.md`, `library.json` — all current to v0.10.0

### Promotion done

- **dev.to:** "When Soldering Doesn't Fix It: Swap the MCU" — published https://dev.to/aliaksandrliapin/when-soldering-doesnt-fix-it-swap-the-mcu-f58
- **Zephyr Discord `#general`:** posted (trimmed swap-the-MCU story)
- **Hackaday tip line** (tips@hackaday.com): emailed, awaiting potential coverage
- **Reddit r/embedded:** drafted, auto-flagged by Rule 4, parked

v0.10.0 itself has no promotional coverage yet — could be a future blog angle if energy is there. Suggested angle: "Fusing voltage and coulomb without going full Kalman" (or similar — the design doc has plenty of material).

---

## State verification commands

These let a new session confirm everything from this handoff in ~30 seconds.

```bash
cd /Users/aliapin/Downloads/project/ibattery-sdk

# Repo state
git status                                          # expect: working tree clean
git log --oneline origin/main..HEAD                  # expect: empty (in sync)
git tag | grep "v0\.\(8\|9\|10\)" | sort -V         # expect: v0.8.0 ... v0.10.0

# Open issues
gh issue list --repo aliaksandr-liapin/ibattery-sdk --state open      # expect: empty

# Recent releases
gh release list --repo aliaksandr-liapin/ibattery-sdk --limit 5       # expect: v0.10.0 first

# library.json points at v0.10.0
python3 -c "import json; print(json.load(open('library.json'))['version'])"   # expect: 0.10.0
```

---

## Build / test commands (most-used)

```bash
# PATH setup — homebrew git FIRST, then Nordic toolchain
export PATH="/opt/homebrew/bin:/usr/bin:/bin:/opt/nordic/ncs/toolchains/e5f4758bcf/bin:$PATH"
export ZEPHYR_BASE="/opt/nordic/ncs/v3.2.2/zephyr"
export ZEPHYR_SDK_INSTALL_DIR="/opt/nordic/ncs/toolchains/e5f4758bcf/opt/zephyr-sdk"

# Host tests (no hardware needed)
cd tests/build && ctest --output-on-failure         # expect: 19 pass

# Gateway tests
cd gateway && /Users/aliapin/.pyenv/versions/3.11.14/bin/python3 -m pytest    # expect: 67 pass

# STM32 firmware (the validated platform)
west build -b nucleo_l476rg app -d /tmp/build-stm32 --pristine -- \
    -DEXTRA_CONF_FILE=boards/nucleo_l476rg_i2cscan.conf \
    -DZEPHYR_EXTRA_MODULES="/opt/nordic/ncs/v3.2.2/modules/hal/stm32" \
    -DCONFIG_BATTERY_SOC_FUSION=y      # or omit for FUSION=n
west flash -d /tmp/build-stm32 --runner openocd

# NUCLEO serial port (when connected)
# /dev/cu.usbmodem1203
```

---

## What's available to work on (menu, non-blocking)

Ranked roughly by leverage. None of these are urgent.

| Item | Effort | Why it might matter |
|---|---|---|
| **Phase 8d brainstorm** — capacity-aging learning + coulomb drift correction | 1–2 hr design + days of implementation | Natural next big-picture step. The "Phase 8 series" is closed; Phase 8d would be a new chapter focused on parameter estimation (vs. state estimation). Demand-driven — only do if there's a real use case. |
| **nRF I2C remap experiment** | ~30 min | Closes v0.8.5's "known limitation" on the broken nRF52840-DK PCA10056 SN 1050258557. Definitive yes/no on whether the GPIO damage is isolated to P0.26/P0.27 or wider. If it works, the nRF DK becomes a backup validation platform. |
| ~~**BLE-on-NUCLEO E2E loop**~~ | ✅ Done in v0.10.1 | Shield arrived; v3 telemetry validated firmware → BLE → gateway → InfluxDB → Grafana. Fixed three latent BLE bugs. See RELEASE_NOTES v0.10.1 + `docs/captures/2026-05-29-v0.10.1-ble-on-nucleo-e2e.log`. |
| **v0.10.0 promotional follow-up** | 1–2 hr | dev.to blog post on Phase 8c. Angle: "Fusing voltage and coulomb without going full Kalman" or "Why current-adaptive α is honest physics." |
| **Polish / Minor findings deferred from code reviews** | <30 min | Things like the `\|I\|` notation in Kconfig help text, the loose `range 0 1000000` on `I_THRESH_X100`, the "Phase 8c:" prefix on the prompt aging poorly. All in the v0.10.0 review threads. |
| **Hackaday coverage check** | 5 min/day | Watch inbox for ~2 weeks; if no response, the tip didn't land — that's fine. |
| **Q-not-ticking spawned task follow-up** (from earlier today) | already closed | Spawned task investigated the v0.8.3 bug-finding; resulted in issues #1 + #2 which are both closed. No further action. |

---

## Known limitations (carry-over)

- **nRF52840-DK PCA10056 SN 1050258557** — per-unit GPIO defect on P0.26/P0.27. NUCLEO-L476RG is the validated Phase 8 platform for this DK. Documented in `docs/HARDWARE_TROUBLESHOOTING.md`.
- **ESP32-C3 INA219** — Zephyr driver unstable at boot; raw I2C fallback works but breadboard contacts cause NACKs. Documented in `src/hal/battery_hal_current_zephyr.c`.
- **BLE on NUCLEO** — requires `x_nucleo_idb05a1` shield (not yet on bench).
- **Phase 8c α tuning** — defaults are starting points; per-device tuning may be needed. All 3 tunables are Kconfigs.

---

## Environmental gotchas (learned the hard way today)

- **`west build` requires all three env vars**: PATH (homebrew git BEFORE Nordic toolchain), ZEPHYR_BASE, ZEPHYR_SDK_INSTALL_DIR. The Task 1 verification snippet in `docs/plans/2026-05-29-phase-8c-fusion-plan.md` initially missed ZEPHYR_SDK_INSTALL_DIR — fixed in execution.
- **`gh release create` needs the right account.** Repo is under `aliaksandr-liapin`; the multi-account `gh` CLI default may be on `aliapin-maker`. Use `gh auth switch -u aliaksandr-liapin` before `gh release create`. The `aliapin-maker` token lacks `workflow` scope which is required for release creation.
- **PlatformIO does NOT auto-publish from git tags.** Each version requires manual `pio pkg publish /tmp/ibattery-sdk-VERSION.tar.gz --no-interactive`. The v0.8.x tags between v0.7.0 and v0.9.0 were never published to PIO because of this.
- **Reddit r/embedded Rule 4** auto-flags long-form posts with external links — even if methodology-first and link-at-the-bottom. The safe pattern: post knowledge-share text only, drop links into reply comments if someone asks.
- **`pio pkg pack` writes to a specific output dir** controlled by `-o`; the tarball name is auto-generated from `library.json`. `tar tzf` to inspect contents before `pio pkg publish`.

---

## File map of where to find things

| Looking for | Path |
|---|---|
| Per-release narrative + evidence | `docs/RELEASE_NOTES.md` |
| Roadmap status by phase | `docs/ROADMAP.md` |
| Public C API reference | `docs/SDK_API.md` |
| Hardware debug methodology + swap-the-MCU technique | `docs/HARDWARE_TROUBLESHOOTING.md` |
| Hardware capture logs (gold-standard evidence) | `docs/captures/` |
| Design docs for each phase | `docs/plans/` |
| Promotional drafts (dev.to, Discord, Hackaday, Reddit) | `docs/articles/` |
| Project status overview | `README.md` and `docs/index.md` |
| AI/Claude context | `CLAUDE.md` |
| PlatformIO metadata | `library.json` |
| Auto-memory for AI sessions | `~/.claude/projects/.../memory/` (project_state.md is the main file) |

---

## When you (or the new chat) pick this up

Paste this opener into the new chat to get the next session up to speed in one message:

> *"ibattery-sdk v0.10.1 shipped — Phase 8 series complete AND the BLE-on-NUCLEO E2E loop is now validated end-to-end (firmware → BLE → gateway → InfluxDB → Grafana on real hardware, three latent BLE bugs fixed). Tagged, GitHub released, PlatformIO published. Repo + remotes + memory all synchronized. See `docs/SESSION_HANDOFF_2026-05-29.md` for the full handoff. No urgent work pending; pick from the available-options menu in that doc."*

That gives a fresh chat:
- The state it can rely on without re-deriving (v0.10.0, Phase 8 done, no urgent items)
- A pointer to this doc for the menu of next moves
- A clear "I don't need to do anything specific" framing, so the new chat doesn't invent work

### Recommended first action for the next session

**Whatever the next session wants to start with, run the state-verification commands above first.** It's 30 seconds and establishes that nothing has rotted since this handoff was written. Then read `docs/RELEASE_NOTES.md` top entry + the "What's available to work on" menu in this doc.

---

## Footnote

Today (2026-05-29) shipped 5 releases. That's an unusual pace. If subsequent sessions feel like they're moving slower, that's normal — today was the convergence of months of accumulated v0.8.x debt + a working hardware rig + a productive design session for v0.10.0. Don't try to match the cadence; match the discipline (TDD, hardware evidence, doc + ship + memory refresh per release).
