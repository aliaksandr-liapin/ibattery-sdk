---
title: "The Zephyr fuel_gauge API Has No 'Battery Health' Property — So My Driver Adds One"
published: false
description: "Making an open-source battery SDK speak Zephyr's standard fuel-gauge interface — what mapped cleanly, the property the standard is missing, and how I validated every reading back on real hardware."
tags: embedded, iot, opensource, zephyr
cover_image: "https://raw.githubusercontent.com/aliaksandr-liapin/ibattery-sdk/main/docs/images/fuel-gauge-readback.png"
# Cover generated from the real validation capture (docs/captures/2026-06-04-fuel-gauge-runtime-validation.log) → docs/images/fuel-gauge-readback.png
---

There's a moment in every open-source project where you stop asking "is my thing good?" and start asking "is my thing *the obvious choice*?" The difference is usually **interoperability**: does it plug into what people already use, or does adopting it mean betting on your bespoke API?

My battery SDK ([`ibattery-sdk`](https://github.com/aliaksandr-liapin/ibattery-sdk), Apache-2.0) had a bespoke API. It does some genuinely useful things on tiny MCUs — estimates State of Charge, and even **learns a battery's State of Health** (how worn out it is) on-device, in integer-only C. (That last part is [its own story](https://dev.to/aliaksandrliapin/i-made-a-battery-admit-it-was-only-73-healthy-on-device-end-to-end-1882).) But to *use* any of it, you had to learn my headers.

So I taught it to speak the language Zephyr developers already use: the **`fuel_gauge` driver API**. This is the story of doing that — including the part where I discovered the standard is missing the one property I care about most, and the part where verifying against the actual source code stopped me from publishing something false.

## What "speaking the standard" means

Zephyr has a `fuel_gauge` subsystem: a uniform driver interface with a `fuel_gauge_get_prop()` call and drivers for real gauge chips (`max17048`, `sbs_gauge`, …). If your code is written against it, you can swap the gauge underneath without touching your app:

```c
const struct device *fg = DEVICE_DT_GET_ANY(aliaksandr_ibattery_fuel_gauge);
union fuel_gauge_prop_val v;

fuel_gauge_get_prop(fg, FUEL_GAUGE_VOLTAGE, &v);                    /* µV */
fuel_gauge_get_prop(fg, FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE, &v);   /* %  */
```

The goal: make iBattery *be one of those drivers*, so adopting it isn't a bet — it's just "the implementation behind the API you already use." I scoped it deliberately as **read-only** (a software view over the SDK's existing outputs, no `set_property`), instantiated from a devicetree node like any real gauge, and **opt-in** (`CONFIG_BATTERY_FUEL_GAUGE_API`, off by default — zero impact if you don't want it).

## Most of it mapped cleanly

The SDK already computes everything the common properties want; the work was mostly **unit conversion**, and the units are the fiddly part. From the header (verified, not remembered):

| Zephyr property | Unit | iBattery source |
|---|---|---|
| `VOLTAGE` | µV | mV × 1000 |
| `CURRENT` / `AVG_CURRENT` | µA, **negative = discharging** | mA × 1000, **sign-flipped** |
| `TEMPERATURE` | 0.1 K | °C × 10 + 2731.5 |
| `RELATIVE_STATE_OF_CHARGE` | % (0–100) | SoC, rounded |
| `REMAINING_CAPACITY` | µAh | coulomb counter |
| `FULL_CHARGE_CAPACITY` | µAh | **learned (SoH-adjusted) capacity** |
| `DESIGN_CAPACITY` | mAh | rated capacity |
| `CYCLE_COUNT` | 1/100ths | cycle count |

Two things worth calling out. First, the **current sign convention is flipped** from mine (Zephyr says discharge is negative; I report it positive) — exactly the kind of detail that's invisible until it's wrong on real hardware. Second, a happy accident: the spec literally says `FULL_CHARGE_CAPACITY` "might change … to determine wear" — so when I map it to the *SoH-learned* capacity, a standard SBS-style consumer sees the battery's aging **for free**, through a standard property.

## The property the standard forgot

Here's the twist. I went to map my headline feature — **State of Health** — to its standard property… and there isn't one. I grepped the actual `fuel_gauge.h` in two Zephyr versions:

```
$ grep -i health include/zephyr/drivers/fuel_gauge.h
$        # (nothing)
```

The standard fuel-gauge API has `RELATIVE_STATE_OF_CHARGE`, `ABSOLUTE_STATE_OF_CHARGE`, capacities, cycle count… but **no State-of-Health property at all.**

This matters to me personally, because minutes earlier I had written — in a positioning doc — that "the Zephyr fuel_gauge API even has a State-of-Health property that maps onto our feature." That was **wrong**. I'd written it from memory. Reading the header before building is the only reason it didn't ship. (Lesson, stated plainly: *don't trust your memory of an API surface — open the file.*)

But a missing standard property isn't a dead end — the API has an escape hatch (`FUEL_GAUGE_CUSTOM_BEGIN`) for exactly this. So iBattery **extends** the standard with a documented custom property:

```c
/* State of Health: value in val.flags as centi-percent (7310 == 73.10 %) */
#define BATTERY_FUEL_GAUGE_PROP_SOH (FUEL_GAUGE_CUSTOM_BEGIN + 0)
```

So the positioning flipped from a (false) "we match the standard" to a (true and *better*) **"we conform to the standard for the common properties, and add the battery-health metric the standard is missing."**

## How I kept it honest: separate the math from the glue

A Zephyr driver is hard to unit-test (it needs the device model + devicetree). So I split it:

- **Pure conversion helpers** — plain C, no Zephyr includes (`mV→µV`, the current sign-flip, `°C→0.1 K`, the SoH-scaled capacity, …). These are **host-unit-tested** with the same fixed-point edge cases the rest of the SDK uses.
- **A thin driver** that just calls those helpers and packs the result into the union. It's verified by a **build-smoke test in CI**: a tiny consumer that `DEVICE_DT_GET`s the node and calls `fuel_gauge_get_prop`, built on ESP32-C3 through the real subsystem.

All the risky arithmetic is covered by fast host tests; the platform glue is proven to link.

## The payoff: read it back on real hardware

Build-passing isn't the same as *correct*. So I added an opt-in self-check that, each cycle, reads every property back through `fuel_gauge_get_prop()` and prints it next to the SDK's native telemetry — then ran it on a NUCLEO-L476RG with a real current sensor and a programmable supply standing in for the cell:

```
native:  V=3133mV  I=32.60mA  SOC=1.28%  Q=219.85mAh  SOH=100.00%
[FG]:    V=3137000uV  I=-32600uA  SOC=1%  REM=219850uAh  FULL=220000uAh
         DESIGN=220mAh  SOH(flags)=10000  set_rc=-88
```

Every line checks out:
- `V` in µV = mV × 1000 ✓
- `I = -32600 µA` — scaled **and sign-flipped** (discharge negative) ✓
- `REM = 219850 µAh = 219.85 mAh` — tracks the coulomb counter exactly ✓
- `SOH(flags) = 10000` = 100.00 % ✓
- `set_rc = -88` (`-ENOSYS`) — the read-only contract, confirmed ✓

A battery learning it's worn out, surfaced through the *standard* interface, validated by reading it straight back. That's the whole loop.

## Why this matters

Conforming to a standard interface is unglamorous and high-leverage. It means a Zephyr developer can reach for iBattery the same way they reach for any fuel-gauge driver — and get something the dedicated gauge chips often don't bother with on cheap hardware: an **on-device battery-health** number, in a couple hundred bytes of integer code.

It's free and open, runs on nRF52840 / STM32L4 / ESP32-C3, and it's read-only today (reading is the 90% case; writing can come later):

👉 **[github.com/aliaksandr-liapin/ibattery-sdk](https://github.com/aliaksandr-liapin/ibattery-sdk)**

If you're building battery-powered hardware on Zephyr, enable `CONFIG_BATTERY_FUEL_GAUGE_API`, drop in the devicetree node, and read your battery — including how *aged* it is — through the standard API. Issues and PRs welcome.

---

*Read-only driver, validated by reading every property back on a NUCLEO-L476RG + INA219 + PPK2 rig. The voltage is bench-emulated; the conversions, the SoH, and the read-back are real.*
