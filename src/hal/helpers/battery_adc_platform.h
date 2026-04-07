/*
 * Platform-specific ADC configuration for battery voltage and NTC channels.
 *
 * Replaces the nRF-only <helpers/nrfx_analog_common.h> with a portable
 * abstraction that works across Zephyr-supported SoC families.
 *
 * Each platform defines:
 *   - VDD channel: input selector, gain, reference, ref_mv
 *   - NTC channel: input selector, gain, reference, ref_mv
 *   - BATTERY_ADC_VDD_USE_VREFINT — defined when VDD cannot be read
 *     directly and must be computed via VREFINT calibration (STM32).
 */

#ifndef BATTERY_ADC_PLATFORM_H
#define BATTERY_ADC_PLATFORM_H

#include <zephyr/drivers/adc.h>

/* ── nRF52 series ─────────────────────────────────────────────────── */

#if defined(CONFIG_SOC_SERIES_NRF52X)

#include <zephyr/dt-bindings/adc/nrf-saadc.h>

/* VDD channel — internal SAADC input, 0.6 V ref, 1/6 gain */
#define BATTERY_ADC_VDD_INPUT       NRF_SAADC_VDD
#define BATTERY_ADC_VDD_GAIN        ADC_GAIN_1_6
#define BATTERY_ADC_VDD_REFERENCE   ADC_REF_INTERNAL
#define BATTERY_ADC_VDD_REF_MV      600

/* NTC channel — external AIN1 (P0.03), same ADC settings */
#define BATTERY_ADC_NTC_INPUT       NRF_SAADC_AIN1
#define BATTERY_ADC_NTC_GAIN        ADC_GAIN_1_6
#define BATTERY_ADC_NTC_REFERENCE   ADC_REF_INTERNAL
#define BATTERY_ADC_NTC_REF_MV      600

/* ── STM32L4 series ───────────────────────────────────────────────── */

#elif defined(CONFIG_SOC_SERIES_STM32L4X)

/*
 * STM32L4 cannot read VDD directly.  Instead the ADC measures VREFINT
 * (an internal ~1.21 V bandgap) against VDDA, and VDD is computed:
 *
 *   VDD_mV = VREFINT_CAL_VREF * (*VREFINT_CAL_ADDR) / adc_raw
 *
 * VREFINT is on ADC1 channel 0 (internal).
 * Factory calibration: address 0x1FFF75AA, measured at 3000 mV.
 */
#define BATTERY_ADC_VDD_USE_VREFINT   1

/* VREFINT on channel 0; gain=1, ref=internal (VDDA) */
#define BATTERY_ADC_VDD_INPUT       0
#define BATTERY_ADC_VDD_GAIN        ADC_GAIN_1
#define BATTERY_ADC_VDD_REFERENCE   ADC_REF_INTERNAL
#define BATTERY_ADC_VDD_REF_MV      3300   /* VDDA (used by adc_raw_to_millivolts) */

/* VREFINT calibration constants from STM32 ROM */
#define BATTERY_STM32_VREFINT_CAL_ADDR  ((const uint16_t *)0x1FFF75AAUL)
#define BATTERY_STM32_VREFINT_CAL_VREF  3000  /* mV at which factory cal was done */

/*
 * NTC channel — external pin.  The exact channel number depends on
 * which pin is wired; set via the board's devicetree overlay.
 * Default: PA0 = ADC1 channel 5 on STM32L476.
 * Gain=1, ref=internal (VDDA = 3.3 V).
 */
#define BATTERY_ADC_NTC_INPUT       5
#define BATTERY_ADC_NTC_GAIN        ADC_GAIN_1
#define BATTERY_ADC_NTC_REFERENCE   ADC_REF_INTERNAL
#define BATTERY_ADC_NTC_REF_MV      3300

/* ── Unsupported ──────────────────────────────────────────────────── */

#else
#error "battery_adc_platform.h: unsupported SoC series — add a section for your platform"
#endif

#endif /* BATTERY_ADC_PLATFORM_H */
