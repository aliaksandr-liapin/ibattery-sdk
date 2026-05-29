# Zephyr Discord — #general

**Posted:** 2026-05-29 13:04 PT — Zephyr Project Discord, `#general` channel.

## Channel

Zephyr Project Discord → `#general` (the `#show-and-tell` channel didn't exist on the server).

## Length

~1,620 characters. Discord's standard cap is 2,000 (4,000 with Nitro).

## Final posted body

```
Just shipped a debugging story I want to share because the lesson is Zephyr-specific.

Chased an INA219 that wouldn't ACK on a nRF52840-DK's I2C bus for over a week. Soldered headers, replaced both data jumpers, swapped chips, verified pinmux — `i2c scan i2c@40003000` kept returning 0 devices.

What broke the loop: my SDK already builds for `nrf52840dk/nrf52840`, `nucleo_l476rg`, and `esp32c3_devkitm` from the same source. Added a minimal STM32 i2cscan overlay + extra-conf, `west build -b nucleo_l476rg ... --pristine`, flashed via openocd. Moved the same chip + same wires onto the NUCLEO's Arduino I2C pins (PB8/PB9). 6/6 ACKs at `0x40` immediately. The defect was per-unit GPIO damage on the specific nRF DK — two chips + multiple fresh wires + same firmware all worked on STM32.

Zephyr-shaped lesson: **a multi-board SDK is a hardware diagnostic tool, not just a portability feature.**

Bonus: once the chip responded cleanly, `Q` was pinned at full capacity under a 2.80 mA load. SoC estimator's "full anchor" was firing every sample (CR2032 anchor's `|I|` gate is `0` → short-circuits the `||` → collapses to just `V >= 2950 mV`; fresh CR2032 never exits that). Fixed by making the anchor edge-detected: one-shot on transition in, re-arm on transition out.

Proof, same rig + load:
​``​`
v0.8.3: Q = 220.00 mAh stuck       SoC = 100% stuck
v0.8.4: Q = 219.98 → 219.75 mAh    SoC = 99.99% → 99.88%
        Δ = -0.23 mAh / 300s @ 2.80 mA  (theory: -0.233)
​``​`

Repo: <https://github.com/aliaksandr-liapin/ibattery-sdk>
Writeup: <https://dev.to/aliaksandrliapin/when-soldering-doesnt-fix-it-swap-the-mcu-f58>
```

(Note: the `​``​`` markers above are escaped with a zero-width space so this file doesn't break when viewed as markdown. The actual Discord message uses plain triple-backticks.)

## What rendered correctly

- `**bold**` → bold text on the lesson line
- Inline `` `code` `` → monospace (e.g., `i2c scan i2c@40003000`, `Q`, `0x40`)
- Triple-backtick block around the v0.8.3 / v0.8.4 proof → preserved column alignment
- `<URL>` wrappers → suppressed embed cards, kept the message tight

## Observations after posting

- Discord collapsed double-newline paragraph breaks to single line breaks. Visually less airy than the editor preview but functionally identical. Not worth editing the live message.
- The Zephyr Discord doesn't have a dedicated `#show-and-tell` channel — `#general` is the practical home for project announcements like this.

## Notes on engagement

- Zephyr Discord is small (~3K active members). Realistic expectation: 5-20 reactions, 0-5 conversational replies.
- If conversation starts about the anchor bug specifically, the fix is in `src/intelligence/battery_soc_estimator.c` and the regression test is `tests/test_soc_coulomb_cr2032.c` — useful to mention specifically.
- Don't crosspost to Nordic / ST / Espressif Discords with identical content. If sharing in adjacent communities, write a shorter audience-specific version.
