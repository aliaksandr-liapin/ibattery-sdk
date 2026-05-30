# Session Handoff — State of Health shipped end-to-end (v0.11.1)

**Date:** 2026-05-29
**Last release:** **v0.11.1** (tagged, GitHub released, PlatformIO published)
**Status:** All work wrapped. Repo + remotes + registry + memory in sync. Working tree clean. **Ready for any next direction.**

This is the cold-start brief for the next chat. Paste the one-liner at the bottom into the new window.

---

## Big picture (one paragraph)

ibattery-sdk is an open-core embedded battery-intelligence SDK (Zephyr C) with a full pipeline: firmware → BLE → Python gateway → InfluxDB → Grafana. Phases 8a–8c (coulomb counting, voltage smoothing, voltage+coulomb fusion) shipped earlier. **This session** validated BLE-on-NUCLEO end-to-end (v0.10.1/0.10.2), then built **Phase 8d State of Health** — on-device capacity-fade learning *and* its cloud path — and shipped it as **v0.11.0**, plus a Grafana dashboard redesign and a v0.11.1 docs fix. SoH now travels firmware→BLE→gateway→InfluxDB→Grafana via a new **wire v4**. Everything was built the disciplined way (brainstorm → design → plan → subagent-driven TDD with spec/quality/final reviews → hardware E2E). 21 host + 77 gateway tests pass.

---

## Open-core strategy (READ THIS — governs all future work)

Goal: **$10M** intermediate, **$100M** long-term. Model = open-core.

- **Tag every roadmap/feature/release item FREE or COMMERCIAL.**
- **Flag commercial territory UPFRONT** — before any work on the paid/private side, say so and confirm.
- **Boundary = the wire protocol.** Device-side SDK + reference tooling = FREE/public. Fleet-scale-in-the-cloud = COMMERCIAL/private.
- **FREE:** the whole on-device SDK (incl. on-device SoH), wire format, reference gateway + dashboards, NVS persistence, partial-excursion learning, more ports.
- **COMMERCIAL (not started — flag before touching):** Fleet SaaS (per-device/mo recurring — the $-engine), advanced/certified algorithm packs, enterprise (compliance/on-prem/SLA). These live in a *future private repo*.
- Detailed strategy is in private memory (`strategy_open_core.md`), not the public repo. Licensing/pricing/entity = legal/financial, needs counsel.

---

## What shipped this session

| Tag | What |
|---|---|
| v0.10.1 | First v3 telemetry over BLE on real hardware. Fixed 3 BLE bugs: MTU 27→35 (v3 never fit before), gateway service-UUID matching (macOS name unreliable), re-advertise after disconnect. |
| v0.10.2 | Docs/packaging fix (registry README). |
| **v0.11.0** | **Phase 8d State of Health, end-to-end.** On-device `battery_soh` (opt-in `CONFIG_BATTERY_SOC_SOH`, integer-only, ~200 B flash, 0 new RAM) + **wire v4** (34 B, `soh_pct_x100` at offset 32, version 4 only when SoH on) → gateway → InfluxDB → Grafana "State of Health" panel. + redesigned dashboard (status tiles → gauges → trends). Hardware-validated E2E. |
| v0.11.1 | Docs fix — absolute README image URL so the dashboard screenshot renders on the PlatformIO registry. |

Design/plan docs: `docs/plans/2026-05-29-phase-8d-soh-{design,plan}.md` + `2026-05-29-soh-cloud-{design,plan}.md`. Evidence: `docs/captures/2026-05-29-v4-soh-cloud-e2e.log`.

---

## State verification (run first in the next session, ~30 s)

```bash
cd /Users/aliapin/Downloads/project/ibattery-sdk
git status                                   # expect: clean
git log --oneline origin/main..HEAD          # expect: empty (in sync)
python3 -c "import json;print(json.load(open('library.json'))['version'])"   # expect 0.11.1
gh release list --repo aliaksandr-liapin/ibattery-sdk --limit 3              # expect v0.11.1 first
git tag | grep v0.11                          # expect v0.11.0, v0.11.1
```

## Build / test (most-used)

```bash
export PATH="/opt/homebrew/bin:/usr/bin:/bin:/opt/nordic/ncs/toolchains/e5f4758bcf/bin:$PATH"
export ZEPHYR_BASE="/opt/nordic/ncs/v3.2.2/zephyr"
export ZEPHYR_SDK_INSTALL_DIR="/opt/nordic/ncs/toolchains/e5f4758bcf/opt/zephyr-sdk"

# Host tests (21 suites)
cd tests && cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure
# Gateway tests (77)
cd gateway && /Users/aliapin/.pyenv/versions/3.11.14/bin/python3 -m pytest -q
# Firmware: BLE + INA219 + SoH (the v4 build)
west build -b nucleo_l476rg app -d /tmp/build-soh --pristine -- \
  -DSHIELD=x_nucleo_idb05a1 -DEXTRA_CONF_FILE=boards/nucleo_l476rg_ble_current.conf \
  -DCONFIG_BATTERY_SOC_SOH=y -DZEPHYR_EXTRA_MODULES="/opt/nordic/ncs/v3.2.2/modules/hal/stm32"
west flash -d /tmp/build-soh --runner openocd     # NUCLEO serial: /dev/cu.usbmodem1203
```

---

## What's available to work on (menu, tagged)

| Item | Tier | Effort | Notes |
|---|---|---|---|
| **NVS persistence for SoH** | **FREE** | ~half-day | Highest leverage. SoH is RAM-only today (relearns 100% on reboot). NVS infra exists (coulomb/cycle use it: keys 1/2, add key 3). Plan sketch in chat history. → likely v0.12.0. |
| Partial-excursion learning | FREE | 1–2 hr + build | Faster SoH convergence (learn from partial cycles, not just full→empty). |
| Promo blog post | FREE | 1–2 hr | "State of Health end-to-end on a coin cell." Funnel. |
| nRF I2C remap experiment | FREE | ~30 min | Recover the defective nRF DK as backup BLE platform. Low value. |
| **Fleet SaaS** | **COMMERCIAL ⚠️** | large | First commercial-territory step → private repo. **Flag + confirm before starting.** |

---

## Gotchas / lessons (learned the hard way this session)

- **BLE testing on macOS:** run `ibattery-gateway scan/stream/run` from **iTerm**, NOT Claude Code — Claude.app lacks the Bluetooth TCC grant (bare pyenv python SIGABRTs). Non-BLE checks (serial, InfluxDB, Grafana) work from anywhere.
- **Wire version = TWO size constants.** `BATTERY_SERIALIZE_V*_SIZE` (serializer) AND `BATTERY_TRANSPORT_WIRE_SIZE_V*` (transport, sizes `ble_send` bound + cache buffer). Bumping one without the other silently rejects packets (`Transport send failed: -2`). A `_Static_assert` now guards it. Host tests don't catch it — only the live BLE path does.
- **Adding a wire version touches:** serialize sizes + transport sizes + all BLE MTU confs (now 37/41) + gateway decoder + influxdb_writer + BOTH dashboards.
- **Two Grafana dashboards:** the LIVE provisioned one is `cloud/grafana/provisioning/dashboards/battery.json`; the README-export is `gateway/grafana/ibattery-dashboard.json`. Edit the right one.
- **Grafana 13 provisioning:** `docker compose restart` after editing a provisioned dashboard may drop it (persisted volume). Validate JSON, then import via API or `down -v && up`.
- **PlatformIO:** `pio pkg pack` snapshots the working tree → finish ALL doc edits (esp. README, which is the only doc in the package — `docs/` is excluded) BEFORE packing. README images need **absolute** raw-GitHub URLs to render on the registry. `gh auth switch -u aliaksandr-liapin` before `gh release create` (switch back after). PlatformIO versions are immutable.
- **Container runtime is Rancher Desktop** (not Docker.app) — `docker` CLI works; don't `open -a Docker`.

---

## When you pick this up

Paste into the new chat:

> *"ibattery-sdk v0.11.1 — Phase 8d State of Health shipped end-to-end (on-device learning + wire v4 + gateway + InfluxDB + Grafana), released to GitHub + PlatformIO. Repo/remotes/memory in sync. Open-core: tag work FREE vs COMMERCIAL, flag commercial territory upfront ($10M→$100M goal). See `docs/SESSION_HANDOFF_2026-05-29.md`. No urgent work; the top FREE pickup is NVS persistence for SoH."*
