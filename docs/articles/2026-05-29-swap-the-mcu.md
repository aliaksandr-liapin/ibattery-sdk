# When Soldering Doesn't Fix It: Swap the MCU

*A hardware-debugging technique that uses your portable firmware codebase as the diagnostic tool. Plus the deeper bug it exposed once I got past the hardware wall.*

---

I spent a week chasing an I2C bus that refused to ACK. By the end of it, I had soldered the breakout, replaced every jumper, swapped chips, and verified every pin assignment three times. The firmware still reported **0 devices found** on every scan.

What finally cracked it wasn't another tweak in the same setup. It was moving the *peripheral* to a different *MCU* — and discovering that the same chip, with the same wires, came up `6/6` immediately on a NUCLEO.

Here's the technique, and the bonus firmware bug it exposed once I got past the hardware wall.

## The symptom

iBattery SDK, Phase 8a: an Adafruit INA219 current sensor wired to a nRF52840-DK over I2C, addressed at `0x40`. From the firmware shell:

```
uart:~$ i2c scan i2c@40003000
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
40:  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
0 devices found on i2c@40003000
```

Six scans in a row, all empty. A logic analyzer earlier in the project had captured the chip ACKing on the wire occasionally — so the chip was alive — but the firmware never saw it.

## What I had already ruled out

By the time soldering came into the picture, I'd already burned through the usual suspect list:

- **Power:** Adafruit "on" LED solid green → VCC + GND wires good
- **Pin assignment:** triple-verified P0.26 = SDA, P0.27 = SCL, VDD on the power header
- **Wires:** replaced both data jumpers with fresh, different-colored wires
- **Chip:** swapped to a second known-good INA219, same result at every address (`0x40`, `0x41`, `0x44`, `0x45` — all `--`)
- **Firmware:** unchanged from a prior release that had captured clean ACKs on a logic analyzer

I soldered the male header pins onto the INA219 PCB to eliminate the press-fit class of failure entirely. Re-scan: still `0/6`.

At that point I'd run out of in-place things to try.

## The pivot: swap the MCU

The SDK was already portable — nRF52840-DK, NUCLEO-L476RG, and ESP32-C3 all built from the same source tree, with per-board overlays and configs. I'd never thought of that portability as a *debugging tool* before, but here we were: same firmware logic, same INA219 driver, same wire-level protocol — just running on different silicon.

So I added a minimal STM32 build config:

```c
// app/boards/nucleo_l476rg.overlay (excerpt)
&i2c1 {
    ina219: ina219@40 {
        compatible = "ti,ina219";
        reg = <0x40>;
        status = "disabled";
        shunt-milliohm = <100>;
        // ...
    };
};
```

```
# app/boards/nucleo_l476rg_i2cscan.conf
CONFIG_I2C=y
CONFIG_SHELL=y
CONFIG_I2C_SHELL=y
CONFIG_BATTERY_CURRENT_SENSE=y
CONFIG_BATTERY_SOC_COULOMB=y
CONFIG_BATTERY_CAPACITY_MAH=220
```

Built and flashed:

```bash
west build -b nucleo_l476rg app -d /tmp/build-stm32-scan --pristine -- \
    -DEXTRA_CONF_FILE=boards/nucleo_l476rg_i2cscan.conf
west flash -d /tmp/build-stm32-scan --runner openocd
```

Moved the exact same wires from the nRF DK to the NUCLEO's Arduino I2C pins (PB8/PB9 = D15/D14). Reset, scanned:

```
uart:~$ i2c scan i2c@40005400
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
40:  40 -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
1 devices found on i2c@40005400
```

**Six out of six scans** ACKed `0x40`. Telemetry started flowing:

```
[v3 t=92117] V=3323 mV T=23.56 C SOC=100.00% PWR=1 CYC=0 flags=0x00000000
I=0.20 mA Q=220.00 mAh
```

Note `flags=0x00000000` — no `CURRENT_ERR`. On the nRF DK this had been stuck at `0x00000020` for two weeks.

## The conclusion this isolation enabled

I now had two chips, multiple fresh wires, and the firmware all working correctly on STM32. That ruled out the chip, the wires, and the firmware in one experiment. The only remaining variable was the **specific nRF52840-DK unit**.

The defect was local to that DK's P0.26 / P0.27 GPIOs — most likely damaged from repeated plug-cycle wear over many sessions. Not a chip issue, not a wiring issue, not a firmware issue. A per-unit hardware defect on a specific MCU board.

I never would have isolated this cleanly without changing the MCU. Every other test I ran *kept the broken hardware in the loop.*

## What "swap the MCU" requires

Two preconditions, and the technique is cheap. Without them, it's not available:

1. **Portable firmware** with a HAL clean enough that you can rebuild for a different MCU without rewriting drivers. In my case the SDK already supported three platforms — same code, different overlay.
2. **A second known-good MCU** with the right peripheral on physically accessible pins. The STM32 NUCLEO's Arduino-compatible I2C1 (PB8/PB9) was already silkscreened.

If you only have one platform target or your driver code is wired to one chip's idioms, this technique isn't available to you — and that itself is an argument for keeping your HAL portable.

## The bonus payload: a bug the swap exposed

This is the part that surprised me. Once the chip was responding cleanly on STM32, I let it run for five minutes under a small load (~1 kΩ resistor across Vin+/Vin-, ~2.80 mA draw).

```
   t (ms)   V (mV)   T (C)      SOC     flags    I (mA)    Q (mAh)
 5506348     3322   23.56  100.00%  00000000      2.80     220.00
 5536382     3322   23.89  100.00%  00000000      2.80     220.00
```

`I` is correctly reading 2.80 mA. But `Q` (the coulomb accumulator, nominal full = 220 mAh) is **pinned at 220.00**. Over five minutes, delta = `0.0000` mAh. SoC stuck at 100%.

Discharge at 2.80 mA for 300 seconds should accumulate 0.233 mAh of charge consumed — easily resolvable with the 0.01 mAh display precision. Something was actively resetting Q.

I had two suspects, and both turned out to be real bugs:

### Bug B — the full anchor was firing every sample

The SoC estimator periodically calibrates the coulomb counter at known voltage anchors (`full` at the top of the curve, `empty` at the bottom). For CR2032, the macro that gates the "this is full" condition on current draw is `0`:

```c
// src/intelligence/battery_soc_estimator.c
#define SOC_ANCHOR_FULL_I_X100  0    // primary cell, no current check

// ...
if (voltage_mv >= SOC_ANCHOR_FULL_MV &&
    (SOC_ANCHOR_FULL_I_X100 == 0 ||
     abs_current < SOC_ANCHOR_FULL_I_X100)) {
    // reset coulomb counter to full capacity
    battery_coulomb_reset(full_mah_x100);
}
```

For CR2032, `SOC_ANCHOR_FULL_I_X100 == 0` short-circuits the `||` to true, collapsing the entire gate to just `V >= 2950 mV`. A fresh CR2032 sits well above 2.95 V under any realistic load and **never exits that condition** during normal use — so the anchor fires on every sample, wiping the integration delta each time.

I fixed this by making the anchor a one-shot edge-detected event:

```c
static bool g_full_anchor_active;
static bool g_empty_anchor_active;

if (in_full_region) {
    if (!g_full_anchor_active) {
        battery_coulomb_reset(full_mah_x100);
        g_full_anchor_active = true;
    }
    g_empty_anchor_active = false;
} else {
    g_full_anchor_active = false;  // re-arm
    g_empty_anchor_active = false;
}
```

The anchor calibrates *once* when entering the region, then lets the integrator do its job until voltage leaves the region.

### Bug A — the integrator and the estimator disagreed on Q's meaning

This one is sneakier. Look at the integrator:

```c
// src/intelligence/battery_coulomb.c (before)
g_accumulated_mah_x1000 += delta_mah_x1000;
```

Positive current (INA219's discharge convention) → positive delta → Q *increases*. So Q is "cumulative throughput."

Now look at the SoC estimator:

```c
// On full anchor:
battery_coulomb_reset(full_mah_x100);  // Q := capacity

// On read:
soc = mah_x100 * 10000 / capacity_x100;  // soc = Q / capacity
```

The estimator sets Q to capacity at full, reads it back, and computes SoC as `Q / capacity`. That's "Q-as-remaining-capacity" semantics — the opposite of what the integrator implements.

Even with Bug B fixed in isolation, the integrator would have *grown* Q above capacity during discharge, the SoC math would have clamped at 100%, and the system would have looked superficially fine while silently misreporting forever.

The fix is one character:

```c
// src/intelligence/battery_coulomb.c (after)
g_accumulated_mah_x1000 -= delta_mah_x1000;  // Q-as-remaining
```

…plus updates to the unit tests that were enshrining the old semantics, and a new TDD regression test that drives the estimator across 100 samples at the full anchor and asserts Q strictly decreases.

## The proof on hardware

Same rig, same load, same five-minute capture, two firmware versions:

```
v0.8.3 (broken):   Q = 220.00 mAh stuck         SoC = 100.00% stuck
v0.8.4 (fixed):    Q = 219.98 → 219.75 mAh      SoC = 99.99% → 99.88%
                   Δ = -0.23 mAh in 300 s @ 2.80 mA load
                   theoretical:  -0.233 mAh
```

Δ matches theory to two decimal places. The anchor calibration is even visible in the first sample — Q starts at 219.98 (not 220.00), confirming the anchor fired *once* at boot and the integrator immediately began tracking the load.

## What I'd file under "lessons"

**1. Your portable firmware codebase is a diagnostic tool.** If you've spent any effort keeping a clean HAL, you've already bought yourself the swap-the-MCU technique. Use it.

**2. "Analyzer sees X, firmware reads Y" usually means signal integrity, not a software bug.** I spent too long suspecting the firmware before the analyzer captures redirected me to the wire.

**3. "Symptom A consistent with no bug" can hide compounding bugs.** The SoC clamping at 100% looked like correct anchor behavior. It was also masking a sign-flipped integrator. Bugs that hide each other are disproportionately expensive — fix one, the other surfaces immediately.

**4. Per-unit GPIO damage is real.** A DK plugged and unplugged dozens of times can have specific pins go bad in ways that aren't obvious without a clean comparison. The board still boots, runs other peripherals, and the GPIO reads 3.3 V at idle — only the edge-sensitivity of I2C exposes the fault.

**5. TDD regression tests for hardware bugs feel slow until they save you twice.** The new `test_cr2032_full_anchor_fires_once_not_every_sample` test took 20 minutes to write. It documented exactly the bug, failed deterministically before the fix, passed after, and will catch any future regression from someone "simplifying" the anchor logic.

## Wrap-up

Repo: [aliaksandr-liapin/ibattery-sdk](https://github.com/aliaksandr-liapin/ibattery-sdk)

Releases that came out of this debugging arc:

- [v0.8.3](https://github.com/aliaksandr-liapin/ibattery-sdk/releases/tag/v0.8.3) — swap to STM32, validated end-to-end
- [v0.8.4](https://github.com/aliaksandr-liapin/ibattery-sdk/releases/tag/v0.8.4) — the two coulomb bugs fixed
- [v0.8.5](https://github.com/aliaksandr-liapin/ibattery-sdk/releases/tag/v0.8.5) — gateway + Grafana panels surface the working signal

Captures preserved in the repo at `docs/captures/` for anyone who wants the raw evidence. The diagnostic methodology is written up in `docs/HARDWARE_TROUBLESHOOTING.md`.

If you've got a hardware bug that won't budge and your codebase is portable, try the swap. Worst case you've confirmed your firmware works on another platform.
