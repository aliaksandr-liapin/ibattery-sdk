# Reddit Karma Comments — Day 2
# Find matching questions, paste the reply. Adapt slightly to the specific post.

---

## r/embedded — "How to structure embedded C project?"

One thing that's helped me a lot: strict layered architecture with a Hardware Abstraction Layer. All platform-specific code (ADC, GPIO, timers) goes behind a clean interface. Core logic only calls the HAL — never includes vendor headers.

Benefits: your business logic compiles and runs on a host machine (macOS/Linux) for unit testing without hardware. You can mock the HAL and test edge cases that are hard to trigger on real hardware. When you port to a new MCU, you rewrite the HAL and everything above just works.

The pattern: HAL (bottom) → core modules → domain logic → application (top). Each layer only calls down, never up.

---

## r/embedded — "Low power design tips?"

The biggest wins I've found:
1. Inactivity timers — transition from ACTIVE to IDLE after 30s, then SLEEP after 120s. Wake on BLE connection or GPIO interrupt.
2. High-impedance voltage dividers — use 100K+ resistors if you're reading battery voltage. A 10K divider wastes 370µA constantly, 100K is only 37µA.
3. Disable peripherals you're not using — especially the ADC between readings. Sample, read, disable.
4. Measure your actual sleep current with a µA-capable meter. The datasheet numbers are best-case; real boards have regulators, LEDs, and debug interfaces drawing power.

The difference between "battery lasts a week" and "battery lasts a year" is usually the sleep current, not the active current.

---

## r/esp32 — "ESP32-C3 vs ESP32-S3 for BLE project?"

For BLE-only projects, the C3 is the better choice:
- Cheaper ($3-5 vs $8-10)
- Lower power consumption (single-core RISC-V vs dual-core Xtensa)
- BLE 5.0 on both — same capabilities
- Smaller form factor (MINI module)

The S3 makes sense when you need: WiFi + BLE simultaneously, USB OTG, more GPIO, camera interface, or the extra processing power (AI/ML at the edge).

For a sensor that reads data and sends it over BLE every few seconds, the C3 is the right choice. I've been using it for battery monitoring and it works great with Zephyr RTOS.

---

## r/esp32 — "Best way to reduce ESP32 power consumption?"

Three things that made the biggest difference for me:
1. Use light sleep between readings instead of just delay(). On the C3, light sleep draws ~130µA vs ~20mA active idle.
2. Reduce BLE advertising interval — 1000ms instead of 100ms cuts power significantly when nobody is connected.
3. If you're reading an ADC, use the oneshot API, not continuous mode. Read the value and stop the ADC.

Also watch out for the USB-to-serial chip (CP2102 or CH340) — on many dev boards it draws 5-10mA even when idle. In production boards without a debug chip, you'll see much lower consumption.

---

## r/embedded — "Unit testing embedded code without hardware?"

Absolutely possible and worth doing. The key is separating your hardware-dependent code (HAL) from your business logic.

I run 69 tests on macOS in under 2 seconds — no Zephyr, no toolchain, no hardware. The trick is mock modules:

```c
// mock_hal.c
static int32_t g_mock_voltage = 3000;
static int g_mock_rc = 0;

void mock_hal_set_voltage(int32_t mv) { g_mock_voltage = mv; }
void mock_hal_set_rc(int rc) { g_mock_rc = rc; }

int battery_hal_adc_read_raw(int16_t *raw) {
    if (g_mock_rc != 0) return g_mock_rc;
    *raw = (int16_t)g_mock_voltage;
    return 0;
}
```

Your test configures the mock, calls the real module under test, and checks the output. Unity framework makes this simple. CMake FetchContent pulls it automatically — zero setup.

Test the hard stuff: boundary conditions, error propagation, what happens when the ADC returns garbage. Those bugs are way easier to find in a 0.1s test run than on hardware with a debugger.

---

## r/Zephyr_RTOS — "Porting Zephyr app to a new board?"

The porting process with Zephyr is mostly devicetree + Kconfig, not C code changes — if your app is structured well.

What you need per board:
1. `app/boards/<board>.overlay` — enable peripherals (ADC, temp sensor, GPIO)
2. `app/boards/<board>.conf` — board-specific Kconfig (enable/disable features)
3. A section in your platform header if you have hardware-specific constants (ADC channels, gain, reference voltage)

The key insight: use `DT_NODELABEL()` with fallback chains instead of hardcoded node names. For example, my temp sensor code does:

```c
#if DT_NODE_EXISTS(DT_NODELABEL(temp))
#define TEMP_NODE DT_NODELABEL(temp)       // nRF52
#elif DT_NODE_EXISTS(DT_NODELABEL(die_temp))
#define TEMP_NODE DT_NODELABEL(die_temp)   // STM32
#elif DT_NODE_EXISTS(DT_NODELABEL(coretemp))
#define TEMP_NODE DT_NODELABEL(coretemp)   // ESP32
#endif
```

Zero #ifdefs in the actual read/write functions. Works across 3 platforms with no code changes.
