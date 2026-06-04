# Positioning: When to Use iBattery SDK

> This is a **positioning guide, not a benchmark.** It describes categories and
> design trade-offs to help you choose the right tool. Other projects' and
> vendors' capabilities are summarized at a high, category level — verify
> current specifics against each vendor's own documentation before relying on
> them. No accuracy/performance claims are made here.

## The one-line pitch

iBattery SDK is a **portable, open-source battery-intelligence library** for
MCUs. It turns a voltage reading (and, optionally, a current sensor) into
State-of-Charge, power state, and structured telemetry — and it **learns a
battery's State-of-Health** (real usable capacity) on-device, in integer-only C
with a ~120-byte static-RAM footprint.

## Lead differentiator: intelligence, not just a reading

Plenty of code can tell you a battery's voltage or a rough "percent full."
iBattery SDK's distinguishing feature is **on-device intelligence**:

- **Coulomb counting** SoC (current-sensor based, voltage-anchored)
- **Voltage + coulomb fusion** (current-adaptive complementary filter)
- **State-of-Health learning** — parameter estimation from a full→empty
  excursion that recovers the cell's *real* usable capacity, so you know not
  just *how charged* a battery is but *how worn out* it is

That last capability is the on-device primitive behind fleet questions like
"which devices actually need a battery replacement?"

## Use it when you…

- want battery intelligence **in software**, with no dedicated fuel-gauge IC on your BOM
- ship across **more than one MCU family** and don't want to re-implement your gauge per platform
- care about **capacity fade / State-of-Health**, not only charge level
- run on **constrained parts** (coin cells, no FPU) where footprint and integer-only math matter
- want the **full pipeline** — on-device → BLE → gateway → time-series DB → dashboard — in one place
- value **open source** (Apache-2.0) you can read, fork, and audit

## Probably not the right fit when you…

- need a **certified / lab-validated** gauge for regulatory sign-off *today* (the bundled discharge curves are reference LUTs, not per-cell lab-characterized — see limitations)
- want the absolute-best accuracy from a **dedicated hardware gauge** with a factory-tuned cell model, and are happy to add that IC and its cost
- aren't on a target the SDK supports yet (today: **Zephyr** on nRF52840 / STM32L4 / ESP32-C3)

## The landscape (categories, not a scoreboard)

| Option | What it is (category) | Relationship to iBattery SDK |
|---|---|---|
| **Dedicated fuel-gauge ICs** (e.g. TI BQ27xxx, ADI/Maxim MAX17xxx families) | Hardware chips with on-chip estimation | Capable, but add a physical part + BOM cost and are vendor-specific. iBattery is pure software — no extra IC required. |
| **Vendor estimation libraries** (e.g. Nordic `nrf_fuel_gauge`) | Model-based estimation tied to one vendor's platform | Polished for that ecosystem. iBattery is multi-vendor, open-source, and adds on-device SoH learning. |
| **Zephyr `fuel_gauge` subsystem** | A standard *driver API* (with drivers for specific gauge ICs) | An interface, not an estimator for arbitrary hardware. iBattery is the algorithm side — **conforming to this API is on our roadmap**, so you could consume iBattery through the standard interface. |
| **Roll-your-own voltage LUT** | A voltage→% table in your own firmware | Simplest path, but jittery and has no aging/health signal. iBattery includes a LUT *plus* coulomb counting, fusion, and SoH. |

## What makes iBattery SDK distinct (verifiable attributes)

- **Intelligence, on-device** — coulomb counting, voltage+coulomb fusion, and SoH learning, not just a voltage LUT.
- **Genuinely portable** — one HAL, three hardware-validated MCU families, integer-only math, no heap, ~120 bytes static RAM (core + coulomb).
- **End-to-end and open** — firmware → BLE → Python gateway → InfluxDB → Grafana, all Apache-2.0, all in one repo.

## Honest limitations

- SoH learning needs a **current sensor (INA219)** and a **full→empty excursion** to converge; it's opt-in.
- Bundled discharge LUTs are **reference curves**, not per-cell lab-validated data (lab-validated profiles are a roadmap item).
- The external-ADC voltage divider currently reads a few percent low (a calibration/trim item). It does **not** affect SoH (charge-based), but it matters if you need accurate absolute voltage.
- Targets **Zephyr** today; other RTOSes would need a HAL port (the HAL is designed for exactly this).

---

*Choosing between these? If you want an open, portable, software-only gauge that
also tells you how a battery is **aging** — that's the gap iBattery SDK fills.*
