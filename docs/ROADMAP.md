# Roadmap & Business Strategy

## Current State (Phase 2 Complete)

ibattery-sdk is a lightweight, portable C SDK for battery intelligence on MCUs.

**What exists today:**
- Targets nRF52840 + CR2032 coin cells
- Voltage reading (SAADC) → SoC estimation (LUT interpolation) → telemetry collection
- Real die temperature sensor via nRF52840 TEMP peripheral (±2 °C)
- Voltage-threshold power state machine with 100 mV hysteresis
- ~37 bytes static RAM, integer-only math, no heap allocation
- HAL abstraction layer — core logic is platform-independent portable C
- 59 host-based unit tests across 5 suites (Unity), zero hardware required to run
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
- CR2032-only is too niche — LiPo support is table stakes for broader adoption
- No charging detection limits to primary (non-rechargeable) cells
- No BLE/transport example — can't demonstrate end-to-end value
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

**Free tier (open-source, MIT/Apache 2.0):**
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

## Development Roadmap

### Near-term (1-3 months)

| Priority | Task | Impact |
|----------|------|--------|
| 1 | Multi-chemistry LUTs — add LiPo single-cell (3.7V nominal) | Biggest use case by volume |
| 2 | Complete Phase 2 stubs — real temperature + dynamic power states | Half-stubbed SDK doesn't inspire adoption |
| 3 | STM32 HAL port | Huge market, multiplies addressable audience |
| 4 | ESP32 HAL port | Huge community, drives open-source adoption |
| 5 | BLE telemetry example | Shows data flowing off the device end-to-end |
| 6 | Zephyr module registry submission | Discoverability via `west manifest` |

### Mid-term (3-6 months)

| Priority | Task | Impact |
|----------|------|--------|
| 7 | Advanced SoC — coulomb counting or voltage+temperature compensation | Simple LUT insufficient for rechargeable cells |
| 8 | Charging support — detect charging state, track charge cycles | Most battery products are rechargeable |
| 9 | PlatformIO library publication | Major distribution channel for Arduino/ESP32 community |
| 10 | Documentation site — GitHub Pages with guides and API reference | Lowers barrier to adoption |
| 11 | Reference hardware design — open-source board (nRF52840 + fuel gauge IC + LiPo) | Hardware reference designs drive SDK adoption |

### Long-term (6+ months)

| Priority | Task | Impact |
|----------|------|--------|
| 12 | Cloud backend + dashboard (if pursuing Path B) | Platform revenue model |
| 13 | Certification-ready battery profiles with lab-validated data | Enterprise/industrial customers |
| 14 | Partner integrations — Nordic DevZone, AWS IoT, Zephyr ecosystem | Distribution and credibility |

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

1. **License choice** — MIT (maximum adoption) vs Apache 2.0 (patent protection) vs dual license (open core)
2. **Lead differentiation** — simplicity, portability, or intelligence? Pick one and lean into it.
3. **First paid offering** — consulting (immediate) vs commercial license (requires feature gap) vs SaaS (requires infrastructure)
4. **Chemistry priority** — LiPo single-cell is the biggest market; LiFePO4 is growing in IoT/solar
5. **Platform priority** — STM32 (professional market) vs ESP32 (maker community) first

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
