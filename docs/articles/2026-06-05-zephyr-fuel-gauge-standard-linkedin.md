# LinkedIn announcement — Zephyr fuel_gauge driver

> Audience-specific draft to accompany the dev.to article
> (`2026-06-05-zephyr-fuel-gauge-standard.md`). Short, professional, link-out.
> Post from your account; review before publishing. Swap the dev.to link for the
> live URL once the article is published.

---

My open-source battery SDK now speaks Zephyr's standard language. 🔋

I added a **Zephyr `fuel_gauge` driver** to iBattery SDK — so any Zephyr project can read battery voltage, charge, current, capacity and cycles through the *standard* interface it already uses, no SDK-specific code required.

The interesting part: while mapping it, I found the standard `fuel_gauge` API has **no State-of-Health property** — no way to report how *worn out* a battery is. So the driver conforms to the standard for the common properties and **extends it** with an on-device battery-health metric the standard is missing.

- ✅ Reads through the standard `fuel_gauge` API (opt-in, read-only, no extra gauge chip)
- ✅ Adds on-device **State of Health** (capacity fade) — the property the standard lacks
- ✅ Validated on real hardware (NUCLEO-L476RG): every property read back correct vs the native telemetry

Integer-only C, ~120 bytes of RAM, runs on nRF52840 / STM32 / ESP32-C3. Free and open (Apache-2.0).

Full write-up 👉 https://dev.to/aliaksandrliapin/the-zephyr-fuelgauge-api-has-no-battery-health-property-so-my-driver-adds-one-dca
Code 👉 https://github.com/aliaksandr-liapin/ibattery-sdk

#embedded #IoT #ZephyrRTOS #opensource #firmware
