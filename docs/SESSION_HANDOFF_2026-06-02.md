# Session Handoff — 2026-06-02

> Current handoff. Supersedes `SESSION_HANDOFF_2026-05-29.md` (and earlier).
> For authoritative current status, this file + `CLAUDE.md` "Current State" win.

## 2026-06-04 UPDATE — the "one remaining HW test" is DONE ✅

The faded-SoH-over-BLE→Grafana end-to-end gap (see TL;DR / §"Next HW test") is
now **closed and hardware-validated**, plus NVS persistence in the same run:

- Rig: PPK2 (Source Meter) on the **cap-free 10 k/10 k** divider + INA219 + load,
  per `docs/BENCH_PREP_SOH_BLE_E2E.md`. Build: `nucleo_l476rg_ble_extadc.conf`
  with a **`CONFIG_BATTERY_CAPACITY_MAH=10`** override so the excursion finishes
  in minutes (build dir `build-stm32-extadc-q10`).
- Drove a full→empty excursion: PPK2 3300 mV armed the full anchor (firmware
  reads ~9% low, so 3300→~2990 ≥ 2950), ~33 mA load drew it down, PPK2 swept to
  2200 mV (→ read ≤2000) fired the **empty anchor → SoH learned 73.10%**.
- **BLE → gateway → InfluxDB:** `soh_pct=73.1` **verified by direct InfluxDB
  query** (bucket `telemetry`, device `iBattery`) — which transitively proves the
  BLE path, since a fresh value in the DB arrived over BLE from the device.
  Gateway run from **iTerm**.
- **Grafana** "State of Health" gauge (73.1%) + trend (100→73 step) **observed on
  the live dashboard** (operator screenshots; Grafana queries the same verified
  InfluxDB).
- **NVS persistence:** reset over SWD → board booted reading **SOH=73.10% from
  flash** on the first telemetry line (not 100%) — verified in the serial capture.
- Evidence: `docs/captures/2026-06-04-soh-fast-excursion.log` (the
  `SOH CHANGED 100.00% -> 73.10%` event + post-reset restore are in it; this is
  the serial stream I read directly).
- Cross-checks: PPK2 vs INA219 current agreed <1% throughout.

> Provenance: firmware-learn, NVS-restore, and the InfluxDB value were verified
> by direct tooling (serial + `influx query`); the Grafana render was observed
> via operator screenshots. The BLE link itself was not sniffed directly — it's
> inferred from the fresh InfluxDB value (gateway's only data source is BLE).

**Board left with the q10 (10 mAh-rated) extADC image + NVS holding learned
73.10%.** To return to normal, reflash the standard app (VREFINT, 220 mAh) and
clear NVS — same as the post-v0.13.0 restore.

Remaining candidates now: external-ADC ~6–10% calibration trim, partial-excursion
learning, promo blog post (the SoH parameter-estimation story now has a complete
BLE→Grafana demo + screenshots).

## TL;DR

iBattery SDK is at **v0.13.0**, on `main`, clean and in sync. The big arc since
the last handoff: shipped **SoH NVS persistence (v0.12.0)** and **external-ADC
voltage sense (v0.13.0)**, drove the **first real hardware-validated SoH
voltage excursion** (PPK2 rig), then ran a **multi-agent health review** and
fixed everything it surfaced — most importantly a **broken Zephyr-module
distribution path**. All logic + builds are verified; the core SoH+persistence
chain is proven on hardware. No urgent work. The one HW test still worth doing
is a faded-SoH value over **BLE → Grafana** end-to-end.

## Repo / release state

- Branch `main`, **in sync with origin**, clean tree, no stale local branches.
- **v0.13.0**: tag pushed, **PlatformIO published**. GitHub *release page* NOT
  created (a `gh` workflow-scope quirk) — to finish it: `gh auth refresh -h
  github.com -s workflow` then `gh release create v0.13.0 ...`, or draft from
  the tag in the browser. (Distribution already works: tag + PlatformIO.)
- **v0.12.0** fully released (tag + GitHub release + PlatformIO).
- Recent merged PRs: #3 (v0.12.0 SoH persistence), #4 (v0.13.0 external-ADC),
  #5 (doc test-counts), #6 (health-review: module-path fix + hygiene), #7
  (gateway tests 77→113), #8 (CI drift guard + printk), #9 (module-path build
  smoke test + CI).

## Tests / coverage (all green)

- **23 C host suites** (Unity) + **113 Python gateway tests** (pytest).
- Firmware builds + links via BOTH the app path and the Zephyr-module path.
- CI (`.github/workflows/`): `ci.yml` (host C + gateway + `build-config-sync`
  drift guard), `firmware.yml` (ESP32-C3 app builds + a module-path build),
  `docs.yml`.

## What is PROVEN on hardware (don't re-test — redundant)

- **SoH NVS persistence** — real STM32 flash write + restore across reset
  (`docs/captures/2026-06-01-soh-nvs-persistence-e2e.log`).
- **Full voltage-driven SoH excursion** — PPK2 emulated the cell: full anchor
  (3200 mV) → ~31 mA real discharge through the INA219 → empty anchor (2000 mV)
  → SoH learned **83.30%** → survived reset
  (`docs/captures/2026-06-01-soh-voltage-excursion-extadc-e2e.log`).
- **BLE telemetry** v3/v4 → gateway → InfluxDB → Grafana (v0.10.1 / v0.11.0).
- **Module-path build** — a `CONFIG_BATTERY_SOC_SOH=y` consumer compiles
  `battery_soh.c` and links as a Zephyr module (build-proven, no HW needed).

> Note: the recent cleanup/test/CI work changed **no runtime logic** (only a
> `printk` include), so the app firmware is functionally identical to the
> validated v0.13.0 image — re-flashing to re-run the excursion is redundant.

## Next HW test worth doing (the only real gap)

**A faded SoH value end-to-end over BLE → Grafana, in one run.** We've shown
SoH-learning-on-hardware (via serial) and BLE→Grafana (at 100%) *separately* —
never a sub-100% SoH traveling all the way to the dashboard. Closes the last
end-to-end gap and makes a complete demo.
- Rig: the PPK2 external-ADC setup (A2/PA4 10k/10k divider + INA219 + load +
  common ground — see `docs/WIRING.md` "External ADC voltage sense").
- Build: `boards/nucleo_l476rg_ble_extadc.conf` (BLE + SoH + external ADC).
- Drive the excursion as in the v0.13.0 capture; run the **gateway from iTerm**
  (Bluetooth permission — see gotchas) and watch the Grafana "State of Health"
  panel show the learned value.

## Open follow-ups (low priority, all noted)

1. GitHub release *page* for v0.13.0 (workflow scope, above).
2. `app/prj.conf` sets `CONFIG_BT=y` + BT options as the app default →
   assigned-but-inactive Kconfig warnings on no-BLE boards (move BT tuning into
   BLE confs; behavior-entangled, do carefully).
3. Benign `battery-charger-gpio` devicetree alias warning (alias is used +
   works — likely won't-fix).
4. External-ADC divider reads ~6–9% low (resistor tolerance / ADC gain) — a
   calibration/trim item; does NOT affect SoH (charge-based).
5. ESP32-C3 + INA219 needs soldered connections (breadboard I2C NACKs) if you
   want current/SoH validated on that board.
6. mkdocs `docs/contributing.md` vs root `CONTRIBUTING.md` dedup (needs an
   include approach).

## Environment / gotchas (bite-sized)

- **Build PATH**: homebrew git/tools MUST precede the NCS toolchain (see
  `CLAUDE.md` Build Commands). STM32 builds need `-DZEPHYR_EXTRA_MODULES=.../
  hal/stm32`.
- **BLE on macOS**: `bleak` (scan/stream/run) must run from **iTerm**, not
  Claude Code — Claude.app lacks the Bluetooth TCC grant (CoreBluetooth
  SIGABRT). Serial/InfluxDB/Grafana work from anywhere.
- **GitHub account**: push/PR/publish as **`aliaksandr-liapin`**. `gh` reverts
  to `aliapin-maker` (NOT a collaborator) each session → run
  `gh auth switch -h github.com -u aliaksandr-liapin`, else PR-create/merge 404s
  and the browser shows no Merge button. (SSH push works under either.)
- **Module-path drift guard**: `scripts/check_build_sync.py` (in CI) enforces
  that the app path and the Zephyr-module path stay in sync. All
  `CONFIG_BATTERY_*` options live ONLY in `app/Kconfig.battery` (sourced by both
  `app/Kconfig` and `Kconfig.ibattery`); root vs app `CMakeLists.txt` source
  sets must match.
- **Git workflow** (mandatory): every feature on its own branch cut from latest
  `main`; after a PR merges, end on latest `main` with the merged branch gone.

## Quick verify in a new session

```bash
git -C /Users/aliapin/Downloads/project/ibattery-sdk status   # main, clean, in sync
# host C tests (23):
cd tests && cmake -B build . && cmake --build build && ctest --test-dir build
# gateway tests (113):
cd gateway && pip install -e ".[dev]" && python -m pytest
# build/config drift guard:
python3 scripts/check_build_sync.py
```
