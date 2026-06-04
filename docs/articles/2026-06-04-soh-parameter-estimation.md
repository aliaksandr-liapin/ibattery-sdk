---
title: "I Made a Battery Admit It Was Only 73% Healthy — On-Device, End to End"
published: false
description: "How a tiny embedded SDK learns a battery's real capacity on-device — integer-only, ~200 bytes of flash — and how I watched a learned 'State of Health' travel from firmware to a live Grafana dashboard."
tags: embedded, iot, opensource, zephyr
cover_image: ""
# cover_image: drop the Grafana State-of-Health screenshot here (gauge at 73.1% + the 100→73 trend)
---

Voltage lies.

Put a battery under load and its terminal voltage sags. Let it rest and the voltage springs back. A naive fuel gauge watching only voltage will happily tell you a worn-out cell is "fine" right up until it falls off a cliff. The number you actually care about — *is this battery still good, or is it time to replace it?* — isn't in the instantaneous voltage at all. It's in the **capacity**: how much charge the cell can still deliver between full and empty.

That quantity fades as a cell ages. Tracking it is called **State of Health (SoH)**, and it's the difference between "the device says 80%" and "the device has 80% of the runtime it had when it was new."

I wanted my open-source battery SDK ([`ibattery-sdk`](https://github.com/aliaksandr-liapin/ibattery-sdk), Apache-2.0) to learn SoH **on the device itself** — no cloud model, no floating-point, on MCUs with kilobytes of RAM. This post is the story of getting that working end to end: from a coulomb integral in firmware to a faded value showing up live on a Grafana dashboard.

## The idea: learn capacity from one full→empty trip

You don't need a PhD-grade model to estimate usable capacity. You need two anchors and an ammeter.

- **Full anchor** — when the cell is at its full-voltage plateau, declare "this is full" and set the coulomb counter to the rated capacity.
- **Discharge** — integrate current over time (coulomb counting). Every milliamp-hour that leaves the cell ticks the counter down.
- **Empty anchor** — when the cell hits its empty-voltage threshold, look at how much charge actually flowed. A *healthy* cell delivers close to its rated capacity before going empty. An *aged* cell hits empty early — it simply has less to give.

From the charge measured between those two anchors, you get the cell's real usable capacity, and `SoH = measured / rated`. The SDK runs it through an integer EMA (so one noisy excursion doesn't whip the estimate around) and a plausibility guard (reject anything outside 30–120% of rated — that's almost certainly a glitch, not a real measurement).

The whole thing is **integer-only, ~200 bytes of flash, and zero new static RAM**. It's opt-in behind a Kconfig flag and rides on the coulomb counter that was already there.

That's the theory. The fun part is proving it on hardware.

## You can't wait six months for a coin cell to age

Here's the catch with validating a capacity-fade feature: capacity fade takes *months*. I was not going to babysit a coin cell through a hundred discharge cycles to get a number on a chart.

So I cheated — honestly. I used a **Nordic PPK2** (Power Profiler Kit II) as a *programmable cell emulator*. In Source Meter mode it's a voltage source you can sweep from ~0.8 V to 5 V while it measures current. That means I can drive the *sensed* battery voltage anywhere I want, independently of the MCU's own supply, and walk it through a full→empty excursion in minutes instead of months.

The rig (all on a breadboard):

- **PPK2** sources the emulated cell voltage.
- A **resistor divider** taps that voltage down onto an ADC pin (the firmware has an external-ADC voltage-sense mode for exactly this).
- An **INA219** current sensor in the load path measures the real current flowing through a load resistor — that's what feeds coulomb counting.
- Everything shares a common ground (the ADC measures relative to it — skip this and you get garbage).

One nice detail: the divider tap sits *before* the INA219 shunt, so the ~microamps the divider draws never pollute the current reading.

## The bring-up gotchas (because there are always gotchas)

**The divider reads low.** My 10 kΩ/10 kΩ divider plus ADC gain came in about 9% under the true voltage. That doesn't matter for SoH — SoH is charge-based, not voltage-based — but it *does* matter for the anchors, which are voltage thresholds. To make the firmware *read* ≥ 2950 mV (the full-anchor threshold), I had to set the PPK2 to ~3300 mV. Measure, don't assume.

**Make the excursion fast.** At the default rated capacity, draining at ~33 mA would take hours. I rebuilt with a small rated-capacity override so a full excursion finishes in a few minutes — same physics, faster clock.

**Cross-check your instruments.** The PPK2's own ammeter read **31.9 mA** while the INA219 reported **31.7 mA** — agreement under 1%. When two independent measurements agree, you trust the chain.

## Watching it happen

With the full anchor armed (`SOH = 100%`), I let it discharge, then swept the PPK2 down toward empty. The serial stream tells the whole story:

```
V=2890 mV  I=22.4 mA  Q=5.39 mAh  SOH=100.00%
V=2623 mV             Q=5.38 mAh  SOH=100.00%
V=2351 mV             Q=5.37 mAh  SOH=100.00%
V=2079 mV  PWR=4      Q=0.01 mAh  SOH=73.10%   ← empty anchor fires
>>> SOH CHANGED 100.00% -> 73.10% <<<
```

There it is. The moment the reading crossed into the empty region, the empty anchor fired, the coulomb counter snapped to zero (nothing left), and the SDK locked in a learned **State of Health of 73.10%** — a plausible, faded value, computed entirely on a Cortex-M4 with integer math.

## From firmware to a live dashboard

A number in a serial log is satisfying. A number on a dashboard is *convincing*.

The SoH value rides out over BLE in the telemetry packet (a dedicated wire-format field), into a Python gateway, into InfluxDB, and onto a Grafana panel. I confirmed it landed in the database directly:

```
_field: soh_pct   _measurement: battery_telemetry   _value: 73.1
```

And on the dashboard, the **State of Health gauge dropped from 100% to 73.1%**, with the trend graph capturing the exact step down. *(Screenshot above.)* That's the whole pipeline — firmware → BLE → gateway → time-series DB → dashboard — carrying a value the device figured out about itself.

## The part that makes it real: it survives a reboot

A learned value you lose on power-cycle is a parlor trick. So the SDK persists the learned capacity to flash (Zephyr NVS), behind a guard that rejects the stored value if the configured rated capacity has changed.

I reset the board and watched the first telemetry line after boot:

```
Battery SDK initialized OK
[v4 t=32]  V=1872 mV  SOH=73.10%   ← restored from flash, not 100%
```

It came up reading **73.10%, straight from flash** — not re-initialized to 100%. Learn it once, keep it forever (or until the next excursion refines it).

## Why this matters

Knowing *which* devices in a deployment are genuinely wearing out — versus just momentarily sagging under load — is the foundation for sensible battery-replacement decisions. Doing that estimation **on-device**, in a couple hundred bytes of integer code, means it works on the cheapest sensor node without a cloud round-trip.

It's all open source and runs today on nRF52840, STM32L4, and ESP32-C3 (Zephyr RTOS):

👉 **[github.com/aliaksandr-liapin/ibattery-sdk](https://github.com/aliaksandr-liapin/ibattery-sdk)**

If you're building battery-powered hardware and want a fuel gauge that tells you the truth about *aging* — not just the moment-to-moment voltage — take it for a spin. Issues and PRs welcome.

---

*Built and validated with a NUCLEO-L476RG, an X-NUCLEO BLE shield, an INA219, and a PPK2 standing in for a cell. The voltage readings are emulated; the coulomb counting, the SoH math, the persistence, and the dashboard are all real.*
