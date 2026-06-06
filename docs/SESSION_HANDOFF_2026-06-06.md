# Session Handoff — 2026-06-06

> Current handoff. Supersedes `SESSION_HANDOFF_2026-06-02.md` (and earlier).
> For authoritative current status, this file + `CLAUDE.md` "Current State" win.

## TL;DR

iBattery SDK is **released at v0.14.0** (tag + GitHub release [Latest] + PlatformIO),
on `main`, clean and in sync. Since the last handoff the project completed a big
**"become a standard" arc** plus the first **FREE-tier lifecycle feature**:

1. **Faded-SoH BLE→Grafana E2E** hardware-validated (learned 73.10%, persisted across reset).
2. **Standard Zephyr `fuel_gauge` driver** (read-only) + a **custom SoH property** the
   standard API lacks — hardware read-back validated. Released in **v0.14.0**.
3. **Positioning + usage docs**: `POSITIONING.md`, `USE_CASES.md` (how-tos + edge cases),
   a phased product vision in `ROADMAP.md`.
4. **Promo**: two articles published (dev.to + LinkedIn) — SoH story and the fuel_gauge story.
5. **Runtime-to-empty** ("time remaining") — built end-to-end (estimator → wire v5 →
   `fuel_gauge` `RUNTIME_TO_EMPTY` → gateway → Grafana). **Merged to `main` (PR #25), UNRELEASED.**

**No urgent work.** The one thing pending is the **hardware e2e for runtime-to-empty**,
then a **v0.15.0 release**.

## Repo / release state

- Branch `main`, **in sync with origin**, clean tree, no stale local branches, 0 open PRs.
- **v0.14.0**: `library.json` = 0.14.0; tag `v0.14.0`; **GitHub release created + marked Latest**
  (also backfilled the previously-missing v0.13.0 release page); **PlatformIO published (accepted)**.
- **On `main`, unreleased (1 squash commit past the v0.14.0 tag):** runtime-to-empty (PR #25).
- Recent PRs this arc: #11–#17 (faded-SoH e2e, CI `select ADC` fix, doc sync), #18 (README/positioning),
  #19 (fuel_gauge driver), #20 (fuel_gauge HW read-back validation), #21 (usage/edge-case docs),
  #22 (phased roadmap vision), #23 (dev.to+LinkedIn drafts), #24 (v0.14.0 release), #25 (runtime-to-empty).

## Tests / coverage (all green)

- **25 C host suites** (Unity) + **122 Python gateway tests** (pytest).
- CI: `ci.yml` (host C + gateway + build-config drift guard), `firmware.yml` (3 ESP32-C3 app
  configs + TWO module-path build smokes: the SoH consumer and the fuel_gauge consumer), `docs.yml`.

## What is PROVEN on hardware (don't re-test — redundant)

- **Faded SoH → BLE → InfluxDB → Grafana + NVS persistence** (NUCLEO-L476RG + PPK2/INA219 rig):
  learned 73.10%, `soh_pct=73.1` confirmed in InfluxDB, on the Grafana gauge, and restored from
  NVS after reset. `docs/captures/2026-06-04-soh-fast-excursion.log`.
- **Zephyr `fuel_gauge` driver read-back** on NUCLEO-L476RG: every property correct vs native
  telemetry (current sign-flip, REMAINING tracks coulomb, SoH custom prop, `set_property`→`-ENOSYS`).
  `docs/captures/2026-06-04-fuel-gauge-runtime-validation.log`. Opt-in self-check:
  `CONFIG_BATTERY_FUEL_GAUGE_SELFCHECK`.

## Next HW test worth doing (the only real gap)

**Runtime-to-empty e2e on the rig.** Build `nucleo_l476rg_ble_extadc.conf` +
`-DCONFIG_BATTERY_RUNTIME_TO_EMPTY=y` (the option needs `BATTERY_SOC_COULOMB`, which the extadc
conf provides). Drive the PPK2/INA219 rig (see `docs/BENCH_PREP_SOH_BLE_E2E.md` — cap-free
10k/10k divider): confirm serial `RTE=<min>` under load, `RTE=n/a` when idle/charging, then
`runtime_to_empty_min` over BLE → InfluxDB → the Grafana "Time to Empty" panel. Run the gateway
from **iTerm** (BLE). Then cut **v0.15.0**.

## Open follow-ups (low priority unless noted)

1. **v0.15.0 release** — bundle runtime-to-empty (bump `library.json` → 0.15.0, tag, GitHub
   release, `pio pkg publish`). Do after the e2e.
2. **OWNER DECISION pending:** confirm/re-tag the **FREE/PAID tiers** in `ROADMAP.md`
   ("Surfaced from usage gaps" table + the phased-vision section — all marked "proposals pending
   owner confirmation"). Monetization is a human call.
3. Remaining FREE Iteration-1/2 items (per ROADMAP phased vision): swap-aware SoH reset,
   power-source flag, on-device LOW/warning state. Iteration-3 (per-battery profiles, fleet
   analytics) is the proposed-COMMERCIAL layer. **Advanced SoH (partial-excursion/multi-cell) is
   PARKED.**
4. External-ADC divider reads ~6–10% low (calibration/trim; does NOT affect SoH).
5. `app/prj.conf` BT defaults → assigned-but-inactive Kconfig warnings on no-BLE boards.
6. ESP32-C3 + INA219 needs soldered connections for current/SoH on that board.

## Environment / gotchas (bite-sized)

- **Build PATH**: homebrew tools MUST precede the NCS toolchain (cmake/openocd live there):
  `export PATH="/opt/homebrew/bin:/usr/bin:/bin:/opt/nordic/ncs/toolchains/e5f4758bcf/bin:$PATH"`.
  STM32 builds need `-DZEPHYR_EXTRA_MODULES=/opt/nordic/ncs/v3.2.2/modules/hal/stm32`.
- **ESP32-C3** builds from the vanilla `~/zephyr-esp32` workspace (not NCS).
- **BLE on macOS**: `bleak` (scan/stream/run) must run from **iTerm**, not Claude Code (CoreBluetooth
  TCC). Serial / InfluxDB / Grafana work from anywhere.
- **GitHub**: push/PR/publish as **`aliaksandr-liapin`**; `gh` reverts to `aliapin-maker` each session
  → `gh auth switch -h github.com -u aliaksandr-liapin` before any gh PR/merge/release.
- **PlatformIO**: logged in as `aliaksandr-liapin`; `pio pkg publish` packs the working tree (sync
  README/docs first; versions are immutable).
- **Cloud stack**: Rancher Desktop (not Docker.app); `cd cloud && docker compose up -d` (InfluxDB
  :8086 ibattery/ibattery-dev-token, bucket `telemetry`; Grafana :3000 admin/admin).
- **Wire format**: now v1–v5. v5 = 38 bytes (`runtime_to_empty_min` uint32 LE @34, `UINT32_MAX`=n/a).
  When adding a version: bump serialize sizes AND transport sizes AND all BLE MTU confs (now 41/45)
  AND the gateway decoder AND both Grafana dashboards.
- **Git workflow** (mandatory): every feature on its own branch from latest `main`; after a PR
  merges, end on latest `main` with the merged branch gone.
- **Process**: this project uses the superpowers workflow (brainstorm → design → plan →
  subagent-driven TDD with spec + quality reviews → finishing-a-development-branch).

## Quick verify in a new session

```bash
git -C /Users/aliapin/Downloads/project/ibattery-sdk status   # main, clean, in sync
export PATH="/opt/homebrew/bin:/usr/bin:/bin:/opt/nordic/ncs/toolchains/e5f4758bcf/bin:$PATH"
cd tests && rm -rf build && cmake -B build . && cmake --build build && ctest --test-dir build   # 25/25
cd ../gateway && pip install -e ".[dev]" && python -m pytest -q                                  # 122
cd .. && python3 scripts/check_build_sync.py                                                      # in sync
```
