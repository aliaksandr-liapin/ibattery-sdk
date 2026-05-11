# Why I Shipped Coulomb Counting Before the Hardware Worked

*An embedded battery SDK story about ambitious algorithms, stubborn breadboards, and the discipline of shipping anyway.*

---

I spent a weekend implementing coulomb counting for my open-source [battery monitoring SDK](https://github.com/aliaksandr-liapin/ibattery-sdk). The code is in production. The hardware doesn't actually work yet.

That sentence used to make me anxious. Now I think it might be one of the most useful things I've learned about embedded engineering.

## The voltage-lookup-table problem

My SDK has been shipping state-of-charge estimation for months. The approach is simple: read the battery voltage, look it up in a curve like this:

```
4200 mV → 100%
3800 mV → ~50%
3000 mV → 0%   (LiPo cutoff)
```

This works fine for a CR2032 coin cell that discharges in a smooth, predictable curve. For LiPo it's a disaster.

Three reasons:

**1. The plateau region.** Between 3.7V and 3.8V, a LiPo can have anywhere from 30% to 70% charge. The voltage barely moves. A 10mV measurement error becomes a 10% SoC error. You can't accurate-curve your way out of physics.

**2. Load-induced sag.** Every time the device transmits over Bluetooth, the battery voltage drops 200-500 mV for a few milliseconds. The voltage-LUT sees this as "battery suddenly at 60%!" Then the transmit ends, voltage recovers, and the SoC jumps back to 80%. Users see a percentage that flickers like a stock ticker.

**3. Cell aging is invisible.** A 500-cycle-old battery reads the same voltage at the same SoC as a fresh one — but it actually holds 80% of the original capacity. Voltage-LUT can't see this.

The fix everyone agrees on: **coulomb counting.** Measure current going in and out, integrate over time, you know exactly how much charge has moved. It's how your phone, your laptop, your EV all really do it.

## Designing the architecture

I want this SDK to be a real product, not a hack. So before writing any code, I sat down to think about layering.

What I landed on:

```
INA219 chip (current sensor)
    │
    ▼ I²C
┌─────────────────────────────────────┐
│  Current HAL (battery_hal_current)  │  ← swap backends (INA219, fuel gauge, shunt+ADC)
└─────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────┐
│  Coulomb counter                    │  ← trapezoidal integration, NVS persistence
│  (integer-only, int64 accumulator)  │
└─────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────┐
│  SoC estimator v2                   │  ← coulomb primary, voltage anchor at endpoints
└─────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────┐
│  Telemetry v3 (32 bytes)            │  ← BLE notifications to gateway
│  + current_ma + coulomb_mah         │
└─────────────────────────────────────┘
```

Key constraints I imposed on myself:

- **Integer-only math.** I'm targeting nRF52840 (Cortex-M4 with FPU), STM32L4 (with FPU), and ESP32-C3 (no FPU). All math must work without floats so the same code paths run on every platform.
- **Zero heap allocation.** Embedded SDKs that malloc are embedded SDKs that mysteriously crash at 3 AM in a field deployment.
- **Backward compatible.** Existing users with voltage-only setups must continue to work with zero changes. Coulomb counting is opt-in via Kconfig.

## The coulomb counter

Here's the heart of it. Trapezoidal integration with an int64 accumulator in 0.001 mAh units (sub-mAh precision):

```c
int battery_coulomb_update(int32_t current_ma_x100, uint32_t dt_ms)
{
    if (!g_initialized) return BATTERY_STATUS_NOT_INITIALIZED;
    if (g_first_sample) {
        g_prev_current = current_ma_x100;
        g_first_sample = false;
        return BATTERY_STATUS_OK;
    }

    /* Trapezoidal: average of previous and current sample */
    int64_t avg = ((int64_t)g_prev_current + current_ma_x100) / 2;

    /* Convert to x1000 mAh units:
     *   delta = (avg_ma_x100 * dt_ms) / 360000
     * Keep remainder to avoid truncation drift over multi-day runs. */
    int64_t numerator = avg * (int64_t)dt_ms + g_remainder;
    int64_t delta_x1000 = numerator / 360000;
    g_remainder = numerator - delta_x1000 * 360000;

    g_accumulated_mah_x1000 += delta_x1000;
    g_prev_current = current_ma_x100;
    return BATTERY_STATUS_OK;
}
```

The remainder accumulator was a debugging gift. My first version had a 1% drift over 1800 samples — each step lost a fractional unit to integer truncation, and that loss compounded. The fix: carry the leftover into the next iteration. Now drift is mathematically zero.

It survives reboot via NVS (non-volatile storage), saving every 60 seconds or whenever charge changes by more than 1 mAh.

## Voltage anchoring

Coulomb counting drifts. Voltage-LUT is accurate at the endpoints (full charge, cutoff) but lies in the middle.

So I use voltage as a periodic calibration anchor:

```c
/* Anchor at full charge (LiPo): voltage near max AND current is tiny
 * (CV phase complete) */
if (voltage_mv >= 4180 && abs_current_ma < 50) {
    battery_coulomb_reset(capacity_mah);  /* declare 100% */
}

/* Anchor at cutoff: voltage below safe threshold */
if (voltage_mv <= 3000) {
    battery_coulomb_reset(0);  /* declare 0% */
}
```

The "low current at full voltage" check is the trick. During charging, a LiPo sits at 4.2V for hours while current tapers. The cell isn't actually full until current drops to a trickle (the "CV phase" of CC/CV charging). If you anchor purely on voltage, you'll claim 100% an hour too early.

## The voltage smoothing layer

While I had the project open, I added two more accuracy improvements that don't need hardware:

### Median filter (replaces moving average)

The existing moving-average filter dilutes BLE transmit sags but doesn't reject them. A 500 mV dip across 8 samples still pulls the average down 62 mV — enough to swing SoC by 10% on the LiPo plateau.

Median filter throws outliers out completely:

```c
uint16_t median_of(const battery_voltage_filter_t *filter)
{
    uint16_t sorted[BATTERY_VOLTAGE_FILTER_MAX_WINDOW_SIZE];
    size_t n = filter->count;

    /* Insertion sort a copy — n ≤ 16, fast for tiny arrays */
    memcpy(sorted, filter->buffer, n * sizeof(uint16_t));
    for (size_t i = 1; i < n; i++) {
        uint16_t key = sorted[i];
        size_t j = i;
        while (j > 0 && sorted[j-1] > key) {
            sorted[j] = sorted[j-1];
            j--;
        }
        sorted[j] = key;
    }

    return (n % 2 == 1) ? sorted[n/2]
                        : (sorted[n/2-1] + sorted[n/2]) / 2;
}
```

The cost: O(n²) sort, but n is bounded at 16 — worst case 256 comparisons, executed at 0.5 Hz. The MCU spends more cycles blinking the status LED.

### SoC slew limiter

Even with a perfect voltage filter, the LUT has steep regions where a 20 mV change maps to a 5% SoC jump. Reality doesn't work that way: an IoT load can't physically discharge a battery 5% in 2 seconds.

So I cap the rate of change:

```c
static uint16_t apply_slew_limit(uint16_t new_soc)
{
    if (!g_first_call) {
        uint32_t dt_ms = current_uptime - g_prev_uptime;
        int32_t max_delta = (5 * 100 * dt_ms) / 60000;  /* 5%/min */
        int32_t delta = (int32_t)new_soc - (int32_t)g_prev_soc;
        if (delta > max_delta) new_soc = g_prev_soc + max_delta;
        if (delta < -max_delta) new_soc = g_prev_soc - max_delta;
    }
    g_prev_soc = new_soc;
    g_prev_uptime = current_uptime;
    g_first_call = false;
    return new_soc;
}
```

Defense in depth: the median filter catches voltage outliers, the slew limiter catches SoC outliers if anything slips through.

## And then the hardware didn't work

I wired the INA219 to an ESP32-C3 DevKitM. Default I2C pins, default address (0x40). Powered on. Watched the serial log:

```
[INA219] not found at 0x40 (rc=-14) — check wiring
```

OK, classic. Probably a loose breadboard contact. I swapped wires, pressed harder, tried both INA219 boards (I bought a 2-pack). Same result.

I added an I²C scanner. The chip responded to the simple probe — its address showed up. But every register write got NACK'd. Reads through the proper `i2c_write_read` API also failed. Only the bare scan worked.

Switched platforms to nRF52840-DK. Same INA219 board. Same wiring topology, just different pins. Different I²C controller from Nordic instead of Espressif. **Zero devices found on the bus.** The chip was completely invisible.

After two hours of debugging — checking pull-up solder bridges on the DK, swapping wires, power-cycling, trying both INA219 boards — I had to admit: I don't know what's wrong. It could be:

- Cold solder joints on the INA219 header pins
- Breadboard contacts at end-of-life
- A clock-stretching quirk in the Espressif I²C driver
- Static damage to one of the chips
- Something I haven't thought of

**I needed a logic analyzer.** Without one, I'm guessing in the dark. Ordered a $10 [HiLetgo USB 24MHz 8-channel analyzer](https://www.amazon.com/HiLetgo-Analyzer-Channel-Compatible-Officially/dp/B077LSG5P2) — arrives Monday.

## The shipping discipline

Here's where I had a choice. The lazy option: don't release until hardware works. The disciplined option: release the software and document the gap.

I chose to ship.

Reasons:

1. **The software is complete and verified.** 14 host-based unit tests, all passing. 65 gateway tests, all passing. The math, the integration, the wire format, the gateway decoder — all proven. A logic analyzer is going to find a wiring problem, not a code problem.

2. **It composes with the existing voltage-LUT path.** Users without an INA219 see no behavior change. The HAL has a stub backend that returns "unsupported" — the SoC estimator detects this and falls back to voltage-only. Zero regression.

3. **The Known Issues section is honest.** [The release notes](https://github.com/aliaksandr-liapin/ibattery-sdk/blob/main/docs/RELEASE_NOTES.md) document the exact failure mode. A potential adopter knows what they're getting.

4. **Holding releases hostage to one flaky breadboard is bad strategy.** I have working voltage smoothing (v0.9.0) that helps every single user immediately. Bundling it with stalled hardware validation would mean shipping nothing.

I tagged **v0.8.0** for coulomb counting (software complete, hardware pending) and **v0.9.0** for voltage smoothing (fully production-ready). Both are live on [GitHub](https://github.com/aliaksandr-liapin/ibattery-sdk/releases).

## CI as accountability

Before stopping for the day, I added a GitHub Actions workflow that builds the firmware for ESP32-C3 in three configurations — default, median filter, current sensing — on every commit. It also runs the host tests on Ubuntu and macOS, and the Python gateway tests.

Now anyone landing a pull request gets concrete proof the code builds and the tests pass. The badge on the README isn't decoration; it's a contract.

## What's next

Monday: logic analyzer arrives. I'll capture the I²C bus during boot, see exactly which byte gets NACK'd, fix the underlying issue. Ship v0.8.1 as a hardware-validation patch.

Then **Phase 8c**: Kalman filter fusion. Combine voltage and current optimally, weighted by their relative confidence (voltage is noisy under load, coulomb counting drifts over weeks). Same public API as today; just a smarter estimator inside.

This is the path real BMS firmware takes — phones, EVs, medical devices. The fact that an open-source SDK targeting CR2032s and breadboards can run the same algorithm is, honestly, the point. Battery intelligence shouldn't be a moat.

## Takeaways

If you're building embedded products and you're not sure when to ship:

1. **Ship the software, document the hardware gap.** Honesty is more credible than perfection.
2. **Design for graceful degradation.** If the new sensor isn't there, fall back to the old path. No regression.
3. **Get a logic analyzer.** Ten dollars buys you the difference between guessing and knowing.
4. **Test what you can on the host.** I have ~80 unit tests that run on Linux, macOS, and Windows. They caught the integer truncation drift bug long before any breadboard was involved.
5. **Layer your design so you can replace any part.** My HAL means swapping INA219 for a fuel-gauge IC later is a 100-line change, not a rewrite.

The SDK is open-source and Apache 2.0. If you're working on a battery-powered IoT product and want to skip rewriting voltage curves and coulomb integrators from scratch:

**Repo:** https://github.com/aliaksandr-liapin/ibattery-sdk

If you've solved the I²C-on-breadboard-on-Espressif puzzle before, I'd love to hear from you. The logic analyzer arrives Monday but human wisdom is faster.

---

*Next post will cover the Kalman filter fusion (Phase 8c), once Phase 8a is hardware-validated. Subscribe if you like battery math and honest engineering write-ups.*
