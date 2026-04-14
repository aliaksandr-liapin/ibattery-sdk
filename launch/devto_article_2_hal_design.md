---
title: Designing a HAL Abstraction That Actually Ports — Lessons from 3 MCU Families
published: true
tags: embedded, c, architecture, iot
cover_image: (use a screenshot of battery_adc_platform.h showing the 3 platform sections)
---

"Just add a HAL layer" is the most common advice in embedded development. It's also the most under-explained. After porting a battery monitoring SDK across nRF52840, STM32L476, and ESP32-C3, here's what I learned about HAL design that actually works in practice.

## The Problem

I needed to read battery voltage on three MCU families. Sounds simple — until you realize each one does it completely differently:

| Platform | VDD Strategy | How it works |
|----------|-------------|--------------|
| nRF52840 | Direct SAADC | Internal mux connects VDD to the ADC. Read it like any channel. |
| STM32L476 | VREFINT sensor | ADC measures a ~1.21V internal bandgap reference. Back-calculate VDD using factory calibration data stored in ROM. |
| ESP32-C3 | Voltage divider | Can't read VDD at all. Route battery through external resistors to a GPIO pin. |

Three fundamentally different approaches. Same end result: battery voltage in millivolts.

## Attempt 1: The Giant #ifdef

The obvious first approach:

```c
int read_battery_mv(void) {
#if defined(CONFIG_SOC_SERIES_NRF52X)
    // 30 lines of nRF SAADC code
#elif defined(CONFIG_SOC_SERIES_STM32L4X)
    // 40 lines of VREFINT sensor code
#elif defined(CONFIG_SOC_SERIES_ESP32C3)
    // 25 lines of voltage divider code
#endif
}
```

This works for two platforms. By three, it's unreadable. By four, it's unmaintainable. Every new feature means touching every `#ifdef` block.

## What Actually Works: Separate the What from the How

The key insight: **split platform constants from platform logic**.

### Layer 1: Platform Constants (one header)

A single header file defines *what* each platform needs — no logic, just values:

```c
// battery_adc_platform.h

#if defined(CONFIG_SOC_SERIES_NRF52X)
    #define BATTERY_ADC_DT_NODE    DT_NODELABEL(adc)
    #define BATTERY_ADC_VDD_INPUT  NRF_SAADC_VDD
    #define BATTERY_ADC_VDD_GAIN   ADC_GAIN_1_6
    #define BATTERY_ADC_VDD_REF_MV 600

#elif defined(CONFIG_SOC_SERIES_STM32L4X)
    #define BATTERY_ADC_VDD_USE_VREFINT 1
    // No ADC config needed — uses Zephyr sensor driver

#elif defined(CONFIG_SOC_SERIES_ESP32C3)
    #define BATTERY_ADC_VDD_USE_DIVIDER    1
    #define BATTERY_ADC_VDD_DIVIDER_RATIO  2
    #define BATTERY_ADC_DT_NODE    DT_NODELABEL(adc0)
    #define BATTERY_ADC_VDD_INPUT  2  // GPIO2
    #define BATTERY_ADC_VDD_GAIN   ADC_GAIN_1_4
    #define BATTERY_ADC_VDD_REF_MV 2500
#endif
```

Adding a fourth platform means adding 5-10 lines to this one file.

### Layer 2: Strategy Selection (one C file)

The ADC driver selects its strategy based on the flags from Layer 1:

```c
// battery_hal_adc_zephyr.c

#if defined(BATTERY_ADC_VDD_USE_VREFINT)
    // STM32: use Zephyr vref sensor driver
    // sensor_sample_fetch() → sensor_channel_get(VOLTAGE) → mV
    
#elif defined(BATTERY_ADC_VDD_USE_DIVIDER)
    // ESP32: raw ADC read → adc_raw_to_millivolts() → multiply by ratio
    
#else
    // nRF52: raw ADC read → adc_raw_to_millivolts() → done
#endif
```

Each block is self-contained. They don't interact. You can read one without understanding the others.

### Layer 3: Everything Above (zero platform awareness)

The voltage module, SoC estimator, telemetry collector, and BLE transport don't include any platform headers. They call:

```c
int battery_hal_adc_read_raw(int16_t *raw_out);
int battery_hal_adc_raw_to_pin_mv(int16_t raw, int32_t *mv_out);
```

That's it. They don't know if the voltage came from SAADC, VREFINT, or a resistor divider. They don't care.

## The Temperature Sensor Problem

Temperature was trickier. Each platform names its die temp sensor differently in the devicetree:

| Platform | Node label |
|----------|-----------|
| nRF52840 | `temp` |
| STM32L476 | `die_temp` |
| ESP32-C3 | `coretemp` |

The solution: a devicetree-driven fallback chain with zero `CONFIG_SOC_SERIES_*` checks:

```c
#if DT_NODE_EXISTS(DT_NODELABEL(temp))
#define BATTERY_TEMP_NODE DT_NODELABEL(temp)
#elif DT_NODE_EXISTS(DT_NODELABEL(die_temp))
#define BATTERY_TEMP_NODE DT_NODELABEL(die_temp)
#elif DT_NODE_EXISTS(DT_NODELABEL(coretemp))
#define BATTERY_TEMP_NODE DT_NODELABEL(coretemp)
#endif
```

The rest of the file — `sensor_sample_fetch()`, `sensor_channel_get(SENSOR_CHAN_DIE_TEMP)` — is identical across all platforms. Zephyr's sensor API handles the underlying driver differences.

This pattern is more resilient than `#ifdef` chains. If a future platform uses `temp` (same as nRF), it works automatically with zero changes.

## What I Got Wrong (and Fixed)

### Mistake 1: Hardcoded ADC parameters

My first NTC thermistor driver had this:

```c
#define NTC_ADC_ACQ_TIME  ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 40)
```

Worked on nRF52840 and STM32. Crashed on ESP32-C3 — the driver only accepts `ADC_ACQ_TIME_DEFAULT`. Same for `.calibrate = true` and `.oversampling = 4` in the ADC sequence struct.

Fix: platform-conditional defaults in the NTC driver. Lesson: even "standard" Zephyr APIs have platform-specific capabilities.

### Mistake 2: Assuming VDD is readable

I designed the API around `battery_hal_adc_read_raw()` + `battery_hal_adc_raw_to_pin_mv()` — a raw ADC read followed by a conversion. This maps cleanly to nRF52 and ESP32.

But STM32's VREFINT path doesn't use the ADC driver at all — it uses the Zephyr sensor API (`sensor_sample_fetch`). The "raw" value is meaningless; the real millivolts come from the sensor channel.

I solved this by storing the computed millivolts in a global during `read_raw()` and returning them from `raw_to_pin_mv()`, ignoring the raw parameter. It's a pragmatic hack that keeps the interface stable.

A cleaner design would have been a single `battery_hal_adc_read_mv(int32_t *mv_out)` function. But by the time I hit this problem, the two-function API was used everywhere. The adapter pattern was the right tradeoff.

### Mistake 3: NVS flash page size

I hardcoded the NVS sector size to 4096 bytes (nRF52840 flash page size). STM32L476 has 2048-byte pages. The fix: query it at runtime:

```c
struct flash_pages_info page_info;
flash_get_page_info_by_offs(flash_dev, offset, &page_info);
nvs.sector_size = page_info.size;
```

Lesson: even infrastructure like flash storage has platform-specific geometry.

## The Porting Checklist

After three ports, here's my checklist for adding a new platform:

1. **Add platform section to `battery_adc_platform.h`** (~10 lines)
   - ADC node label, VDD input channel, gain, reference, strategy flag
   - NTC channel, gain, reference

2. **Create board overlay** (`app/boards/<board>.overlay`)
   - Enable ADC, temperature sensor, GPIO aliases

3. **Create board config** (`app/boards/<board>.conf`)
   - Kconfig: temp source, BLE, charger pins

4. **Check devicetree node labels**
   - Die temp sensor name? Add to the fallback chain if new.
   - ADC node name? Add to NTC driver if different.

5. **Test ADC sequence compatibility**
   - Does the platform support oversampling? Calibrate flag? Custom acquisition time?

6. **Build and verify** — no core module changes should be needed.

If step 6 requires core changes, the HAL interface is incomplete. Go back and fix the abstraction.

## Results

| Platform | Flash | RAM | Core code changes |
|----------|-------|-----|-------------------|
| nRF52840-DK | 152 KB | 30 KB | (original) |
| NUCLEO-L476RG | 38 KB | 10 KB | 0 |
| ESP32-C3 DevKitM | 356 KB | 138 KB | 0 |

Zero core module, intelligence, telemetry, or transport changes across all three ports. Only HAL files and board configs.

## Key Takeaways

1. **Separate constants from logic.** A platform header with just `#defines` is easy to extend. Logic with `#ifdefs` is not.

2. **Use devicetree fallback chains** instead of `CONFIG_SOC_SERIES_*` checks where possible. They're more resilient to new platforms.

3. **Design for the weird platform.** If you design your HAL around the easiest platform (nRF52's direct VDD read), the others won't fit. Design for the most constrained one and the simple cases fall out naturally.

4. **Test on host, validate on hardware.** 69 tests run in under 2 seconds without any embedded toolchain. Hardware validation is the final step, not the first.

5. **The HAL boundary is where you'll find bugs.** Most issues during porting were at the HAL edge — ADC parameters that don't transfer, node labels that differ, flash page sizes that vary. The core logic never broke.

## Links

- **GitHub**: [aliaksandr-liapin/ibattery-sdk](https://github.com/aliaksandr-liapin/ibattery-sdk)
- **Platform header**: [battery_adc_platform.h](https://github.com/aliaksandr-liapin/ibattery-sdk/blob/main/src/hal/helpers/battery_adc_platform.h)
- **Architecture docs**: [ARCHITECTURE.md](https://github.com/aliaksandr-liapin/ibattery-sdk/blob/main/docs/ARCHITECTURE.md)
- **Demo video**: [YouTube](https://www.youtube.com/watch?v=ClD72f-qkmU)
- **First article**: [How I Built a Portable Battery SDK That Runs on 3 MCU Platforms](https://dev.to/aliaksandrliapin/how-i-built-a-portable-battery-sdk-that-runs-on-3-mcu-platforms-28gp)
