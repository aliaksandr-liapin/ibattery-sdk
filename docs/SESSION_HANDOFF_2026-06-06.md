# Session Handoff — 2026-06-06

> Current handoff. Supersedes `SESSION_HANDOFF_2026-06-02.md` (and earlier).
> For authoritative current status, this file + `CLAUDE.md` "Current State" win.

## TL;DR

iBattery SDK is **released at v0.16.0** (tag + GitHub release [Latest] + PlatformIO accepted),
on `main`, clean and in sync. **v0.16.0 = swap-aware SoH Phase 1** — auto-reset learned
health on a power-off battery swap (boot SoC-jump vs NVS baseline → `battery_soh_reset()`
+ `BATTERY_SWAPPED` flag; opt-in, CR2032-only; PR #32), hardware-validated E2E incl. the
same-cell-power-cycle false-positive control; it also bundles the Grafana **"Connected
Device" tile + `$device` selector** (PRs #30/#31). The prior **v0.15.0** shipped
**runtime-to-empty** + an **idle-current-floor fix**, both hardware-validated. The
**v0.14.0** arc was a **"become a standard"** push plus the first **FREE-tier lifecycle
feature**:

1. **Faded-SoH BLE→Grafana E2E** hardware-validated (learned 73.10%, persisted across reset).
2. **Standard Zephyr `fuel_gauge` driver** (read-only) + a **custom SoH property** the
   standard API lacks — hardware read-back validated. Released in **v0.14.0**.
3. **Positioning + usage docs**: `POSITIONING.md`, `USE_CASES.md` (how-tos + edge cases),
   a phased product vision in `ROADMAP.md`.
4. **Promo**: two articles published (dev.to + LinkedIn) — SoH story and the fuel_gauge story.
5. **Runtime-to-empty** ("time remaining") — estimator → wire v5 → `fuel_gauge`
   `RUNTIME_TO_EMPTY` → gateway → Grafana. **Released in v0.15.0 (PRs #25, #27, #28).**

## Done 2026-06-06 (this handoff's session)

- **Runtime-to-empty hardware e2e — DONE.** Validated firmware → wire v5 → BLE → gateway
  → InfluxDB → Grafana on NUCLEO-L476RG (extADC+BLE+INA219 rig): RTE converged to ~407 min
  under a ~30 mA load (ground truth 404), idle → `n/a`/field-omitted with no spike.
  Capture: `docs/captures/2026-06-06-runtime-to-empty-idle-floor.log`.
- **Idle-current-floor fix (PR #27, TDD).** The bench idle-check exposed RTE briefly emitting
  multi-year values (261564 → 1307820 min) as the EMA decayed toward zero after the load was
  removed. Root cause: the idle gate only tripped at current ≤ 0, and low-drain reporting was
  *intended/tested* behavior — so it was a design decision, surfaced to the owner, not silently
  patched. Fix: `CONFIG_BATTERY_RUNTIME_IDLE_THRESHOLD_MA_X100` default 0 → 50 (0.5 mA), which
  bounds the estimate to `Q/floor` (spike dropped 83× to 23004 min, then `n/a`). Tunable; 0 = legacy.
- **v0.15.0 released (PR #28):** runtime-to-empty + idle-floor; tag + GitHub release + PlatformIO.
- **Connected-Device Grafana tile + `$device` selector (PRs #30/#31).** Tag telemetry by the
  board's real BLE name (read via GATT 0x2A00 — macOS leaves the advertised name empty); fixed
  the Grafana string-render; nRF given a distinct name. **First full BLE→Grafana run on the
  nRF52840-DK.** `$device` selector shows one board at a time (no cross-device duplication).
- **Swap-aware SoH Phase 1 (PR #32) — built + hardware-validated + MERGED.** Full superpowers
  workflow (brainstorm→design→plan→subagent-driven TDD w/ spec+quality reviews→finish). On boot,
  an upward SoC jump vs a persisted NVS baseline ⇒ `battery_soh_reset()` + `BATTERY_SWAPPED`
  flag (opt-in, CR2032-only, no wire bump). Bench E2E on the PPK2 rig passed incl. the
  **same-cell-power-cycle false-positive control** (`docs/captures/2026-06-06-swap-aware-soh-e2e.log`).
  Merge was held until that control passed (it auto-resets health). **Validation note:** detection
  logic is HW-proven; the cell was PPK2-emulated — a real-cell + all-boards pass is logged as
  deferred QA in `ROADMAP.md` (not blocking).
- **v0.16.0 released:** `library.json` → 0.16.0, RELEASE_NOTES + ROADMAP + CLAUDE.md updated,
  tag `v0.16.0`, GitHub release [Latest], PlatformIO published.

**No urgent work.** All this session's pending items are complete. Open: **FREE/PAID tiers**
(owner call), **swap-aware Phase 2** (LiPo charge-vs-swap), and the **deferred HW-validation
matrix** (all boards × real batteries).

## Repo / release state

- Branch `main`, **in sync with origin**, clean tree, no stale local branches, 0 open PRs.
- **v0.16.0** (current): `library.json` = 0.16.0; tag `v0.16.0`; **GitHub release [Latest]** +
  **PlatformIO published**. Nothing unreleased on `main`.
- **v0.15.0**: runtime-to-empty + idle-floor; tag + GitHub release + PlatformIO.
- Recent PRs: #24 (v0.14.0 release), #25 (runtime-to-empty), #26 (handoff sync),
  #27 (RTE idle-floor fix, HW-validated), #28 (v0.15.0 release), #29 (handoff sync),
  #30 (connected-device tile + GATT name), #31 (`$device` selector), #32 (swap-aware SoH, HW-validated),
  v0.16.0 release.

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

## Next HW test worth doing

**None pending.** The runtime-to-empty e2e (the prior "only real gap") is DONE — see
"Done 2026-06-06" above. Remaining HW candidates are all low-priority (see Open follow-ups):
external-ADC ~6–10% trim, partial-excursion SoH learning, real-cell LUT validation, nRF I2C remap.

## Open follow-ups (low priority unless noted)

1. ~~**v0.15.0 release**~~ — DONE 2026-06-06 (bundled runtime-to-empty + idle-floor fix; tag +
   GitHub release [Latest] + PlatformIO accepted).
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
