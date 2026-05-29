# Reddit post — r/embedded

## Title

A debugging technique I'd never tried: when soldering didn't fix my I2C bus, I swapped the MCU

## Subreddit

r/embedded (primary). Could also crosspost to r/Zephyr_OS, but not r/AskElectronics — different audience.

## Flair

"General" or "Tech question" — whichever the sub allows. Avoid "Promotional / Self-Promotion" if there's a flair for that, since the post is methodology-first and the link is at the bottom.

## Body

Spent over a week on an INA219 that wouldn't ACK on the nRF52840-DK's I2C bus. Six scans, six empty results, every time. Logic analyzer captures earlier in the project had shown the chip occasionally ACKing on the wire, but the firmware never saw it.

By the time I'd run out of in-place fixes I'd already:

- Verified VCC + GND (Adafruit "on" LED solid green)
- Triple-checked pin assignment (SDA = P0.26, SCL = P0.27)
- Replaced both data jumpers with fresh wires
- Swapped to a second known-good INA219
- Soldered the male header pins onto the INA219 PCB to eliminate the press-fit class of failure

Still 0/6 scans on the firmware shell.

What finally cracked it: I have a portable codebase that builds for nRF52840, STM32L476, and ESP32-C3 from the same source. I'd never thought of that portability as a *debugging tool* before, but it turns out moving the **peripheral** to a different **MCU** is a really clean way to isolate a per-unit defect.

So I added a minimal STM32 build config, moved the exact same wires and INA219 to a NUCLEO-L476RG's Arduino-compatible I2C pins (PB8/PB9), and re-scanned.

6/6 ACKed `0x40`. Telemetry started flowing immediately. `flags=0x00000000` — no current-sensor error. Two chips + multiple wires + same firmware all working on STM32 = the defect is local to the specific nRF52840-DK unit. Almost certainly damaged P0.26/P0.27 GPIOs from years of plug/unplug wear. The board still boots and runs other peripherals fine, but the edge-sensitivity of I2C exposes it.

Two preconditions before this technique is available:

1. A HAL clean enough that you can rebuild for a different MCU without rewriting drivers
2. A second MCU with the right peripheral on physically accessible pins

If you've got both, swap-the-MCU isolates "chip vs wires vs firmware vs MCU" in a single experiment in a way no other test does — because every other test you can run leaves the broken hardware in the loop.

**The bonus part that surprised me:** once I got the chip responding cleanly on STM32, the coulomb counter still wasn't tracking discharge. I had a clear 2.80 mA load, the integrator was running, but Q stayed pinned at 220 mAh (full capacity) for the entire 5-minute capture window.

Turned out to be two compounding bugs in the SoC estimator that had been hiding each other:

- The "full voltage anchor" calibration was firing on every sample, not just on transition into the full region — for CR2032 the current-magnitude gate was `0`, which short-circuited the `||` and collapsed the entire condition to "voltage above threshold." Fresh CR2032 never exits that condition.
- The integrator was treating positive current (discharge) as adding to Q (Q-as-cumulative-throughput), but the SoC estimator was reading Q as remaining capacity. Even with the anchor bug fixed in isolation, Q would have grown above capacity and SoC math would have clamped at 100% silently forever.

Two bugs, both real, both expensive because they masked each other. Caught one because the SoC clamping at 100% looked like correct anchor behavior; only systematic comparison against the theoretical discharge rate exposed the underlying sign error.

Lesson I'm taking forward: if you've kept your HAL portable, you've already bought yourself the swap-the-MCU technique. Use it before you spend another day in the same broken setup.

Curious whether others here have used this pattern — has anyone else used multi-platform support specifically as a debugging tool, vs. just a portability feature? And for the "anchor fires every sample" class of bug — it feels like the kind of thing static analysis or a property-based test would catch in retrospect, but I can't think of a tool that would have flagged it before I had hardware in hand. Interested in how people structure invariant checks across modules that have to agree on a shared variable's semantics.

---

## Notes for posting

- **No link in the post at all** (updated after r/embedded's Rule 4 auto-flagged the original draft). Keep the technical story complete in the body; if commenters ask "do you have a write-up / repo?", reply to that specific request with the link. A link in a reply to a question is responsive (safe), not promotional (flagged).
- **Don't crosspost to multiple subs the same day.** Spam filters notice patterns. Post to r/embedded first; if that goes well, consider r/Zephyr_OS a day or two later as a separate post (slightly adapted intro, not a literal repost).
- **Engage in the comments for the first 2-3 hours after posting.** Reddit visibility decays fast; early engagement is what gets the post to escape new.
- **If you get challenged on "what's the actual debugging lesson here":** the punchy version is "portable firmware is a diagnostic tool, not just a portability feature." That's the line worth repeating in replies.
- **If a mod asks about self-promo:** the standard rule of thumb is the 9:1 ratio — for every self-link, you should have nine value-add contributions in the sub. If you're not regular on r/embedded, lead with the methodology and treat the link as supporting evidence rather than the point.
- **Karma to expect:** honest read is 50-150 upvotes if the title hooks people, possibly higher if it gets a comment thread going. The SoC anchor bug is the kind of war story that gets engagement.
