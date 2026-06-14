# Roadmap & Business Strategy

## Current State (v0.13.0 — Phase 8 + State of Health end-to-end; SoH persists across reboot (NVS) and its learning chain is hardware-validated via a real PPK2 voltage excursion; opt-in external-ADC voltage sense; all on NUCLEO-L476RG)

v0.10.1 closed the deferred BLE end-to-end loop: v3 telemetry now flows
firmware → BLE → gateway → InfluxDB → Grafana on real hardware (X-NUCLEO-IDB05A2
shield). Fixed three latent BLE bugs (MTU too small for v3, gateway name-matching
unreliable on macOS, no re-advertise after disconnect) — see RELEASE_NOTES and
`docs/captures/2026-05-29-v0.10.1-ble-on-nucleo-e2e.log`.

ibattery-sdk is a lightweight, portable C SDK for battery intelligence on MCUs.

**What exists today:**
- Targets nRF52840 + CR2032 coin cells and LiPo single-cell (3.7V)
- Voltage reading (SAADC) → SoC estimation (LUT interpolation) → telemetry collection
- Real die temperature sensor via nRF52840 TEMP peripheral (±2 °C) or external 10K NTC thermistor (B=3950) on SAADC AIN1
- Temperature-compensated SoC for LiPo cells (`CONFIG_BATTERY_CHEMISTRY` Kconfig)
- Voltage-threshold power state machine with 100 mV hysteresis
- Full battery state machine: IDLE/SLEEP inactivity timers + TP4056 CHARGING/DISCHARGING/CHARGED
- LiPo single-cell (3.7 V nominal) discharge curve LUT
- BLE telemetry transport with custom GATT service + notification characteristic
- Wire format v1 (20B), v2 (24B with `cycle_count`), and v3 (32B with `current_ma` + `coulomb_mah`) — backward compatible
- Charge cycle counter with NVS flash persistence (CHARGING → CHARGED transitions)
- Compile-time transport backend and chemistry selection via Kconfig
- ~120 bytes static RAM (core + coulomb counter), integer-only math, no heap allocation
- HAL abstraction layer — core logic is platform-independent portable C
- LiPo 500mAh powering nRF52840-DK via TP4056 HW-373 (USB-C) — real battery power verified
- TP4056 charging confirmed (voltage rise on USB-C connect); CHRG/STDBY GPIO signals simulated with jumper wires (LED pad soldering pending)
- Python BLE gateway (bleak) → InfluxDB 2.x → Grafana dashboard (Docker Compose)
- `ibattery-gateway` CLI: scan, stream, run, analytics (health, anomalies, rul, cycles)
- Cloud analytics: battery health scoring, real-time + historical anomaly detection, RUL estimation, cycle analysis
- 25 C test suites (Unity) + 122 Python tests (pytest), zero hardware required to run
- Zephyr RTOS integration with clean layered architecture
- Production-quality codebase: no layer violations, consistent conventions, full documentation

---

## Product Positioning

**Category:** Battery monitoring middleware library for embedded developers building battery-powered products.

**Strengths:**
- Clean architecture, genuinely production-quality code
- HAL abstraction is well-executed — porting is realistic, not theoretical
- Tiny footprint makes it viable for real constrained devices (coin cells, wearables)
- Test infrastructure is solid and runs without hardware

**Gaps to address before monetizing:**
- ~~CR2032-only is too niche~~ — ✅ LiPo support added (v0.2.0+), dual-chemistry Kconfig
- ~~No charging detection~~ — ✅ TP4056 charger driver + NVS cycle counter (v0.4.1 + v0.5.1)
- Single platform (nRF52840) limits reach — STM32 port would be highest-impact addition

**Competitive landscape:**
- Nordic's nrf_fuel_gauge library
- TI BQ series fuel gauge drivers
- Zephyr's native fuel gauge API

**Differentiation options (pick one to lead with):**
1. *Simplicity* — easier to integrate than the alternatives
2. *Portability* — works across chip vendors and RTOSes
3. *Intelligence* — better SoC accuracy through advanced algorithms

---

## Growth Strategy

### Path A: Open-Source SDK with Commercial Extensions (Open Core)

**Free tier (open-source, Apache-2.0):**
- Current functionality: voltage, SoC, telemetry, basic power states
- Community builds trust, adoption, GitHub stars, contributors
- Target audience: hobby developers, students, startups prototyping

**Paid tier (commercial license):**
- Advanced SoC algorithms (coulomb counting, Kalman filter, hybrid estimators)
- Multi-chemistry support (LiPo, LiFePO4, NiMH)
- Battery health / cycle count / degradation prediction
- Cloud telemetry integration (BLE → gateway → dashboard)
- OTA-updateable battery profiles
- Certified/validated LUTs with lab-measured discharge curves
- Priority support + SLA

Reference: Redis, GitLab, and Zephyr use variations of this model.

### Path B: Battery-as-a-Service Platform

Full stack vision where the SDK is the device-side agent:

1. **Device SDK** (what exists today) — collects telemetry
2. **Transport layer** — BLE, LoRaWAN, or cellular upload
3. **Cloud backend** — ingests telemetry, stores time-series data
4. **Dashboard** — fleet-wide battery health monitoring, alerts, analytics
5. **API** — customers integrate battery insights into their own platforms

Revenue: per-device subscriptions or per-fleet annual licenses.

### Path C: Consulting + Custom Integrations

Use the SDK as a portfolio piece and proof of expertise:
- Custom battery profiling for specific chemistries/form factors
- Integration into client firmware (Nordic, STM32, ESP32, etc.)
- Battery-related certification consulting (IEC 62133, UN 38.3)

Lower scale but immediate revenue with zero infrastructure cost.

---

## Phased Product Vision (lifecycle-driven)

> Articulated 2026-06-04 from the battery lifecycle. **FREE/PAID labels are
> proposals pending owner confirmation** — monetization is a business decision.
> Open-core principle: the on-device lifecycle *core* is FREE to drive adoption
> ("become the standard"); *fleet-scale* intelligence, multi-battery profiling,
> and saved-history analytics are the COMMERCIAL layer; developer-experience
> tooling stays FREE because it lowers adoption friction.
>
> **Sequence intentionally:** ship the FREE core (Iterations 1–2) first to grow
> the user base, *then* build the COMMERCIAL layer (Iteration 3) once there's
> adoption to monetize — consistent with the Monetization timing below.

### Iteration 1 — Primary (one-time) cell lifecycle — *proposed FREE*

The unifying loop: **capture state → track discharge → warn low → estimate
"replace soon" → reset on swap.**
- Today: SoC from voltage ✅, CRITICAL low state ✅.
- Add: softer **LOW/warning** tier (before CRITICAL); **runtime / time-to-empty**
  estimate ("replace soon"); **swap-aware `battery_soh_reset()`** (today manual).
- Note: a primary cell has no charge cycles — "tracking" here is monotonic
  discharge (precise charge counting needs the INA219 current sensor).

### Iteration 2 — Rechargeable cell lifecycle — *proposed FREE*

Iteration 1 **+** charge-cycle count + capacity-fade (SoH) + "capacity too low"
warning.
- Today: cycle counter ✅, SoH learning + NVS persistence ✅.
- Add: **SoH-threshold ("capacity too low") warning**.
- **"Is the battery rechargeable?" is not electrically detectable.** Resolve by
  (a) **configuration** — chemistry is set at build time (recommended); or
  (b) **inference** — a primary cell never charges, so the first observed charge
  event (charger present) marks it rechargeable. Exposed via the **power-source
  flag** item below. Never pure sensing.

### Iteration 3 — Advanced, all battery kinds — *mostly proposed COMMERCIAL*

- **Per-battery profiles / profile store** (capacity, health, stats per installed
  battery) — *proposed COMMERCIAL (advanced pack + cloud)*. ⚠️ Per-physical-cell
  identity is impossible without an **external ID** (NFC/serial) or a user
  "battery #N" input — design for "profile the currently-installed battery +
  snapshot on swap," not "auto-recognize a re-inserted cell."
- **Extended environmental warnings** (temp too high/low + effect on the battery)
  — *split*: basic on-device thresholds + derating = **FREE**; fleet-wide alerting
  = **COMMERCIAL**. (Gateway temp thresholds + LiPo temp-compensated SoC already
  exist.)
- **Saved battery-history analytics** (e.g. how a specific device/board drains a
  cell over time) — *proposed COMMERCIAL (SaaS/analytics)*. Raw telemetry is
  already stored (InfluxDB/Grafana); the analysis & reporting is the product.
- **SDK install wizard / guided fine-tune** (incl. user-provided inputs) —
  *proposed FREE* (developer experience lowers adoption friction → serves the
  "become the standard" goal).

### Two hard technical truths (set expectations)

1. **Rechargeability / power-source is configured or inferred, never sensed** from
   the cell alone.
2. **Per-physical-cell identity requires an external ID** — the SDK can profile
   "the battery currently installed," not fingerprint a specific re-inserted cell.

### Parked

- **Advanced SoH** (partial-excursion learning, multi-cell/per-cell identity) —
  **postponed 2026-06-04**; revisit when fleet/pack demand appears.

---

## Development Roadmap

### Surfaced from real-world usage gaps (2026-06-04)

These came directly from walking through user scenarios (new/used coin cell,
permanent USB, multi-board, battery swap) — see [USE_CASES.md](USE_CASES.md).
**FREE/PAID tags below are proposals pending owner confirmation** (monetization
is a business decision — keep a human in the loop). Guiding principle: basic
on-device *correctness and usability* stays FREE (gating it would hurt adoption);
*fleet-scale* intelligence and alerting is the COMMERCIAL tier.

| Candidate | Why (gap it closes) | Tier (proposed) |
|---|---|---|
| ✅ **Runtime-to-empty estimate** (minutes) on-device — **done, released v0.15.0** | opt-in `CONFIG_BATTERY_RUNTIME_TO_EMPTY`; native API + Zephyr `fuel_gauge` `RUNTIME_TO_EMPTY` + wire v5 + gateway/Grafana. Host/gateway tested **and hardware-validated E2E** (idle-current floor added so the estimate never spikes on load removal) | **FREE** (basic gauge feature) |
| ✅ **Swap-aware SoH** — battery-swap detection + auto-reset — **done, released v0.16.0 (Phase 1)** | opt-in `CONFIG_BATTERY_SWAP_DETECT` (default y for CR2032 only): on boot, an upward SoC jump vs a persisted baseline ⇒ `battery_soh_reset()` + `BATTERY_SWAPPED` flag. Hardware-validated incl. the same-cell-power-cycle false-positive control. **LiPo (charge-vs-swap disambiguation) = Phase 2.** | **FREE** (correctness) |
| **Power-source flag** — expose "on external/permanent power vs battery" | no auto USB-vs-battery detection today; only inferable from charge state | **FREE** (basic) |
| **Configurable low/warning state on-device** (not just CRITICAL) | device has one low threshold (CRITICAL); a "LOW/warning" tier only exists in the gateway | **FREE** (basic) |
| **Fleet alerting / notifications** ("plan a swap", thresholds → push/email) | device emits signals, not messages; turning them into alerts is the integrator's job | **COMMERCIAL** (SaaS/fleet) |
| **Predictive maintenance / fleet RUL dashboards** | per-device cloud RUL exists; fleet-wide "which units to service" is the monetizable layer | **COMMERCIAL** (SaaS/fleet) |
| **Advanced SoH** — partial-excursion learning, multi-cell/per-cell identity | faster/robust health without a full excursion; supports swappable + pack use | **POSTPONED** (parked 2026-06-04) — revisit when fleet/pack demand appears |

> Documentation already shipped for these gaps: `USE_CASES.md` (How-Tos +
> "Developer responsibilities & edge cases", incl. the swap/`battery_soh_reset()`
> rule). The roadmap items above are the *code* follow-ups.

> **Deferred QA — full hardware validation matrix.** Much of the feature
> validation to date is PPK2-emulated or host-only (the firmware sees a voltage,
> which the PPK2 reproduces faithfully — valid for the *logic*). A future pass
> should exercise every feature against **real batteries across all three boards**:
> swap-detect with a real CR2032 swap (IR sag/relaxation, the physical pull-and-
> insert); the SoC LUT against a real discharge curve; an ESP32-C3 BLE→Grafana E2E
> (never done); and nRF current/SoH/RTE once the DK's P0.26/P0.27 I2C defect is
> worked around. Not blocking any release — analog/physical-layer confidence.

### Near-term (1-3 months)

| Priority | Task | Impact |
|----------|------|--------|
| ~~1~~ | ~~Multi-chemistry LUTs — LiPo single-cell (3.7V nominal)~~ | ✅ Done (v0.2.0) |
| ~~2~~ | ~~Real temperature + dynamic power states~~ | ✅ Done (v0.2.0 + v0.2.1) |
| ~~3~~ | ~~Phase 3 — BLE telemetry transport~~ | ✅ Done (v0.3.0) |
| ~~3.5~~ | ~~Phase 4 — Cloud telemetry (BLE gateway + InfluxDB + Grafana)~~ | ✅ Done (v0.4.0) |
| ~~4~~ | ~~Phase 5a — Temperature-compensated SoC + cloud analytics~~ | ✅ Done (v0.5.0) |
| ~~5~~ | ~~Phase 5b — Cycle counter, wire v2, RUL, cycle analysis, Grafana v2~~ | ✅ Done (v0.5.1) |
| ~~6~~ | ~~STM32 HAL port~~ | ✅ Done (v0.6.0) — NUCLEO-L476RG hardware-validated |
| ~~7~~ | ~~ESP32 HAL port~~ | ✅ Done (v0.7.0) — ESP32-C3 DevKitM hardware-validated |
| 8 | Zephyr module registry submission | Discoverability via `west manifest` |

### Mid-term (3-6 months)

| Priority | Task | Impact |
|----------|------|--------|
| ~~9~~ | ~~Advanced SoC — temperature compensation~~ | ✅ Done (v0.5.0 — LiPo temp-compensated SoC) |
| ~~10~~ | ~~Charging support — detect charging state, track charge cycles~~ | ✅ Done (v0.4.1 + v0.5.1 — TP4056 GPIO driver + NVS cycle counter) |
| ~~8a~~ | ~~Advanced SoC — Coulomb Counting (v0.8.0 → v0.8.4 — hardware-validated, Q tracks discharge)~~ | ✅ Done (v0.8.4) — Current-sensor SoC with NVS persistence |
| 8b | Advanced SoC — Voltage-LUT Correction (complete in v0.9.0) | Software-only SoC jitter reduction |
| ~~8c~~ | ~~Advanced SoC — Voltage+Coulomb Signal Fusion (v0.10.0 — hardware-validated, drift correction working)~~ | ✅ Done (v0.10.0) — complementary filter with current-adaptive α, opt-in via Kconfig |
| ~~12~~ | ~~PlatformIO library publication~~ | ✅ Done — published to registry.platformio.org |
| ~~13~~ | ~~Documentation site — GitHub Pages with guides and API reference~~ | ✅ Done — aliaksandr-liapin.github.io/ibattery-sdk/ |
| 14 | Reference hardware design — open-source board (nRF52840 + fuel gauge IC + LiPo) | Hardware reference designs drive SDK adoption |

#### Phase 8a: Coulomb Counting SoC (v0.8.0 → v0.8.4)

**Software-complete in v0.8.0; hardware-validated on NUCLEO-L476RG in v0.8.3;
coulomb counter integration bugs fixed in v0.8.4 (Q now tracks discharge).**

- INA219 current sensor HAL (Zephyr sensor API + raw I2C fallback)
- Coulomb counter with trapezoidal integration (int64 accumulator, sub-mAh precision)
- Voltage-anchored SoC estimation (coulomb primary, LUT at endpoints)
- Telemetry v3 wire format (32 bytes: adds current + coulomb fields)
- NVS persistence for reboot survival
- `tools/i2c-analyzer/capture.sh` — sigrok-cli wrapper for I2C bus debugging
- `docs/HARDWARE_TROUBLESHOOTING.md` — 4-phase diagnostic methodology + swap-the-MCU isolation

**Status (v0.8.4):** ✅ Phase 8a fully functional end-to-end on **NUCLEO-L476RG**.
INA219 ACKs 6/6 on I2C1, `flags=0x00000000`, live current measurement, and **Q
tracks discharge proportionally** (5-min capture: Q = 219.98 → 219.75 mAh,
Δ = -0.23 mAh at 2.80 mA load, matches theory ≈ 0.233 mAh; SoC tracks 99.99% →
99.88%).

**Status (v0.8.5):** Cloud-side observability closed.
`gateway/influxdb_writer.py` now persists `current_ma` and `coulomb_mah` to
InfluxDB, and the Grafana dashboard has dedicated **Live Current (mA)** and
**Remaining Charge (mAh)** timeseries panels. Phase 8a's signals are now
visible end-to-end: chip → BLE → gateway → InfluxDB → dashboard.

The nRF52840-DK unit used in v0.8.0–v0.8.2 development (PCA10056 SN 1050258557)
has a per-unit GPIO defect on P0.26/P0.27 — two independent chips and fresh
wires both fail to ACK firmware-side, but the same chips and wires work
immediately on STM32. This is documented as a known per-unit limitation, not a
platform problem. Future patch release may remap nRF I2C to alternate GPIOs.
See `docs/HARDWARE_TROUBLESHOOTING.md` for the full swap-the-MCU isolation
narrative.

#### Phase 8b: Voltage-LUT Correction Mode (complete in v0.9.0)

- Coulomb counting as smoothing layer over existing voltage-LUT
- Reduces SoC jitter from voltage sag during BLE TX
- No new hardware — software-only improvement
- Delivered: median voltage filter (Kconfig-selectable) + SoC slew-rate limiter

#### Phase 8c: Voltage + Coulomb Signal Fusion (v0.10.0)

**Software-complete and hardware-validated end-to-end on NUCLEO-L476RG.**

- Complementary filter with current-adaptive blend coefficient (α)
- α small under load (voltage unreliable due to IR drop), larger at rest (LUT accurate)
- Targets mid-discharge drift in the coulomb integrator — the failure
  mode that affects every long-running battery device that never reaches
  voltage anchors
- Integer-only math, ~45 lines, +56 bytes flash, **0 new RAM**
- Composes cleanly with Phase 8a anchors (still fire as one-shot
  calibration events) and Phase 8b slew limiter (still applies after
  fusion to smooth the displayed SoC)
- **Strictly opt-in** via `CONFIG_BATTERY_SOC_FUSION` (default n) —
  byte-for-byte identical to v0.8.4 when disabled
- Hardware-validated via 3 captures in `docs/captures/`:
  drift-correction baseline (Δ Q matches theory), load-vs-rest
  demonstration (10/10 toggles detected, adaptive α verified),
  fusion-off regression (byte-identical to v0.8.4)
- 19 host tests pass (was 17; +10 unit tests + 6 integration tests)
- Kept the "Kalman filter" label internally during design but settled
  on the complementary-filter approach: simpler, easier to reason
  about, smaller, and the Kalman framing's advantage doesn't apply
  cleanly to the bias-dominated noise model here

What was deferred (not 8c scope):
- Battery aging / capacity learning — ✅ Phase 8d MVP implemented (State of
  Health via full→empty excursion learning; opt-in `CONFIG_BATTERY_SOC_SOH`,
  branch `feature/phase-8d-soh`). Underpins the fleet/SaaS monetization path.
- Coulomb counter drift correction (INA219 ±1%) — still a future 8d candidate
  (current-offset auto-calibration), not in the SoH MVP.
- Smooth α interpolation — YAGNI, step function matches physical knee
- Runtime α tuning via API — compile-time is enough

#### Phase 8d: State of Health (capacity-fade learning) — MVP

First parameter-estimation feature (8a–8c were state estimation). Learns true
usable capacity from full→empty discharge excursions and reports SoH. Opt-in,
integer-only, RAM-only MVP; cloud telemetry + NVS persistence deferred. SoH is
the on-device primitive behind fleet predictive-maintenance ("which devices
need battery replacement?") — the capability that feeds the SaaS fleet-
monitoring and "advanced SoC" commercial-license models. Design + plan:
`docs/plans/2026-05-29-phase-8d-soh-{design,plan}.md`.

### Long-term (6+ months)

| Priority | Task | Impact |
|----------|------|--------|
| ~~15~~ | ~~Cloud backend + dashboard~~ | ✅ Done (v0.4.0 + v0.5.1 — InfluxDB + 11-panel Grafana + analytics CLI) |
| 16 | Certification-ready battery profiles with lab-validated data | Enterprise/industrial customers |
| 17 | Partner integrations — Nordic DevZone, AWS IoT, Zephyr ecosystem | Distribution and credibility |

---

## Monetization Models

| Model | Revenue | Effort | Timing |
|-------|---------|--------|--------|
| Consulting — custom integrations | $100-250/hr | Low | Now |
| Paid support tiers — SLA, priority fixes | $500-5K/yr per customer | Low | After 50+ GitHub users |
| Commercial license — advanced features | $2K-20K/yr per product line | Medium | After multi-chemistry + advanced SoC |
| SaaS dashboard — fleet monitoring | $0.50-2/device/month | High | 6-12 months out |
| Hardware reference design — kits/license | $50-200/kit | Medium | After 2+ platform ports |
| Training/workshops — embedded battery courses | $500-2K per session | Low | Anytime |

---

## Key Decision Points

1. **License choice** — ✅ Apache 2.0 (patent protection + permissive, compatible with open-core model)
2. **Lead differentiation** — simplicity, portability, or intelligence? Pick one and lean into it.
3. **First paid offering** — consulting (immediate) vs commercial license (requires feature gap) vs SaaS (requires infrastructure)
4. **Chemistry priority** — LiPo single-cell is the biggest market; LiFePO4 is growing in IoT/solar
5. **Platform priority** — STM32 (professional market) vs ESP32 (maker community) first

---

## Brand & Visibility (the SDK is also a public proof asset)

iBattery isn't only code — it's a public demonstration that the maintainer ships
real, production-grade systems end to end. GitHub visitors, dev.to readers, and
technical reviewers will read and judge it, so **presentation must read well to a
competent reviewer who isn't a Zephyr specialist**, not just to embedded devs.
(Keep this track focused on embedded — the AI/Cloud portfolio is a separate brand.)

### Action items
- [ ] **README rewrite (high ROI):** tighten the top — one-line value prop, who
      it's for, a copy-paste quick-start, and a short **"vs. alternatives"** section
      (vs. Zephyr's `fuel_gauge` API, which has **no battery-health property**; vs.
      rolling your own; vs. a cloud-side BMS). The comparison content already exists
      in `POSITIONING.md` — surface/link it from the README. Add a clear
      **contribution CTA** (issues/PRs welcome).
- [ ] **Runnable example + 2-minute quick-start:** the README has a Quick Start;
      ensure a *runnable* minimal example. `tests/module_consumer/` is a minimal
      module-consuming app that can be promoted/linked as that example.
- [ ] **License consistency:** all repo text is **Apache-2.0** (LICENSE, package
      metadata, README badge, CONTRIBUTING, POSITIONING). The only contradictions
      are (a) an **external graphic** (GitHub social-preview card / promo image)
      that says MIT — fix it to Apache-2.0 where it lives; (b) one loose "MIT/Apache
      2.0" phrasing in this file — tightened below. The legally-operative LICENSE is
      Apache-2.0 (patent protection + permissive; see "Key Decision Points").
- [ ] **(Optional) `good-first-issue` labels** to invite contributors.
- [ ] **Maintain a "post-worthy milestones" list** — kept at `POST_WORTHY.md` in
      the repo root (intentionally *not* on the public docs site — it's internal
      comms planning).

### Distribution is room-specific (important)
- **LinkedIn:** polished infographics work well.
- **Reddit (r/embedded, r/Zephyr_RTOS) & Hacker News:** **no marketing graphics** —
  they trigger spam filters and community pushback. Use plain text + genuine
  technical screenshots (Grafana, serial/scope captures, architecture diagrams).
  New/low-karma Reddit accounts get auto-filtered → participate before posting.

### Credibility signals to grow and protect
Consistent commit cadence (the contribution graph is real social proof); real
adoption (PlatformIO downloads, stars, **no stale issues**); visible quality
(tests, docs, CI badges, runnable examples).

### Clean-room
Nothing from the maintainer's employer or any proprietary/customer source enters
this public repo. Ever.

---

## Glossary

| Abbreviation | Meaning |
|-------------|---------|
| ADC | Analog-to-Digital Converter — hardware peripheral that converts analog voltage to a digital value |
| BLE | Bluetooth Low Energy — short-range wireless protocol common in IoT and wearables |
| FPU | Floating Point Unit — hardware for fast floating-point math (absent on some low-cost MCUs) |
| HAL | Hardware Abstraction Layer — interface isolating portable code from platform-specific drivers |
| IoT | Internet of Things — network of connected embedded devices |
| LiFePO4 | Lithium Iron Phosphate — rechargeable battery chemistry, 3.2 V nominal, long cycle life |
| LiPo | Lithium Polymer — rechargeable battery chemistry, 3.7 V nominal, common in consumer electronics |
| LoRaWAN | Long Range Wide Area Network — low-power, long-range wireless protocol for IoT |
| LUT | Lookup Table — precomputed array used here to map voltage to state-of-charge |
| MCU | Microcontroller Unit — small computer on a single chip (CPU, memory, peripherals) |
| NiMH | Nickel-Metal Hydride — rechargeable battery chemistry, 1.2 V nominal |
| NTC | Negative Temperature Coefficient thermistor — resistor whose resistance decreases with temperature |
| OTA | Over-The-Air — wireless firmware or data update mechanism |
| RTOS | Real-Time Operating System — OS with deterministic timing guarantees (e.g., Zephyr, FreeRTOS) |
| SAADC | Successive Approximation Analog-to-Digital Converter — ADC type used in nRF52840 |
| SDK | Software Development Kit — library plus tools for building applications |
| SLA | Service Level Agreement — contract guaranteeing response/fix times for support |
| SoC | State of Charge — remaining battery capacity as a percentage (0–100%) |
| VDD | Voltage Drain Drain — positive supply rail of the MCU |
