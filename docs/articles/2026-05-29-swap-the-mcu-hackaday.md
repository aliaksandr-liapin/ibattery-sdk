# Hackaday tip line submission

## Address

`tips@hackaday.com` (verified 2026-05-29 against https://hackaday.com/submit-a-tip/ — they also offer a web form on that page with the same destination, either works)

## Subject

Swap the MCU: a hardware debugging technique + the firmware bug it exposed

## Body

Hi Hackaday team,

I'd like to flag a debugging story from an open-source embedded SDK project that might make a good short writeup. Two angles, both concrete and self-contained.

**The hook:** an Adafruit INA219 current sensor refused to ACK on a nRF52840-DK's I2C bus for over a week. Standard isolation steps — soldered the breakout, replaced both data jumpers with fresh wires, swapped to a second known-good chip, triple-verified the pinmux — all kept showing `0 devices found` on the firmware shell.

What finally cracked it: the SDK was already portable across nRF52840, STM32L476, and ESP32-C3 from the same source tree. I'd never thought of that portability as a *debugging tool* before — but moving the same chip + same wires onto an STM32 NUCLEO-L476RG produced 6/6 clean ACKs immediately, with no firmware changes. That cleanly isolated the defect as per-unit GPIO damage on the specific nRF DK, almost certainly from years of plug/unplug wear on the I2C pins.

**The bonus story:** once the chip was responding cleanly on the NUCLEO, the coulomb counter still wasn't tracking discharge. Five-minute capture under a 2.80 mA load showed `Q = 220.00 mAh stuck` even though the integrator was running. Turned out to be two compounding bugs in the SoC estimator that had been hiding each other for four release versions:

- The CR2032 "full voltage anchor" calibration was firing on every sample (the current-magnitude gate macro was `0`, which short-circuited the `||` and collapsed the entire condition to `V >= 2950 mV` — a fresh CR2032 never exits that)
- The integrator was treating positive current as adding to Q, but the SoC estimator was reading Q as remaining capacity — opposite semantics. Even with the anchor bug fixed in isolation, SoC would have clamped at 100% silently forever

After fixing both: `Q = 219.98 → 219.75 mAh` over five minutes (-0.23 mAh delta, matches the theoretical -0.233 mAh discharge to two decimal places).

**Why this might be worth covering:**
- The swap-the-MCU technique is broadly applicable to anyone with a portable firmware HAL — it's a memorable debugging move that doesn't get named
- The compounding-bugs-hiding-each-other story is a clean illustration of why "looks fine in production" can mask real defects
- All the evidence is preserved in-repo: serial captures, code diffs, TDD regression test, and the released firmware that actually closes the bug

**Links:**
- Long-form writeup: https://dev.to/aliaksandrliapin/when-soldering-doesnt-fix-it-swap-the-mcu-f58
- Repo: https://github.com/aliaksandr-liapin/ibattery-sdk
- Captures (raw evidence files): https://github.com/aliaksandr-liapin/ibattery-sdk/tree/main/docs/captures
- The diagnostic methodology doc: https://github.com/aliaksandr-liapin/ibattery-sdk/blob/main/docs/HARDWARE_TROUBLESHOOTING.md
- Releases v0.8.3 / v0.8.4 / v0.8.5 if release-note narrative is useful

Project is Apache 2.0, all code + evidence freely reproducible.

Happy to chat or send additional materials if any of this is interesting.

Best,
Aliaksandr Liapin
[your email]
[optional: your dev.to / GitHub profile]

---

## Notes on tip-line submissions

- **Send from a real email address**, not a forwarder. Hackaday writers reply when interested.
- **Don't follow up if you don't hear back.** Hackaday gets thousands of tips. If a writer picks it up, you'll know within a week or two. If not, it wasn't a fit — that's fine.
- **If they reply with questions**, answer concretely with links to specific commits, files, or telemetry captures. Their writers prefer evidence-driven sources.
- **Don't expect coverage.** Hackaday picks up maybe 1-2% of tips. The value of the tip is partly in case they cover it, partly in being on their radar for future stories from the same project.
- **Don't pitch the same story to other tip lines simultaneously.** If Hackaday picks it up, they'll want it to be their story. If you've also pitched to Hackster, Electromaker, etc., flag that in the email. Honesty matters more than coverage.
- **If they cover it, don't ask for edits or framing changes** unless something is factually wrong. Let the writer have their voice.
