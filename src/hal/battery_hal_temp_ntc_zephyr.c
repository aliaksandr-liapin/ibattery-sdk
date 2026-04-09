/*
 * Temperature HAL — External NTC thermistor via nRF52840 SAADC.
 *
 * Reads a 10K NTC in a voltage-divider circuit on AIN1 (P0.03):
 *
 *   VDD --- [10K pullup] ---+--- [NTC] --- GND
 *                           |
 *                        AIN1 (P0.03)
 *
 * The ADC reading is converted to resistance, then looked up in
 * the NTC LUT to get temperature in 0.01 °C units.
 *
 * Replaces battery_hal_temp_zephyr.c (die sensor) when
 * CONFIG_BATTERY_TEMP_NTC=y.  The interface is identical:
 * battery_hal_temp_init() and battery_hal_temp_read_c_x100().
 *
 * NOTE: The nRF SAADC driver does not preserve per-channel input
 * mux settings across reads of different channels (e.g. internal
 * VDD on channel 0 clobbers channel 1's AIN selection).  We work
 * around this by calling adc_channel_setup() before every read.
 */

#include "battery_hal.h"
#include "battery_ntc_lut.h"

#include <stdint.h>
#include <stddef.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>

#include "helpers/battery_adc_platform.h"

/* ── ADC configuration for NTC channel ──────────────────────────── */

#if defined(CONFIG_SOC_SERIES_STM32L4X)
#define NTC_ADC_NODE          DT_NODELABEL(adc1)
#elif defined(CONFIG_SOC_SERIES_ESP32C3)
#define NTC_ADC_NODE          DT_NODELABEL(adc0)
#else
#define NTC_ADC_NODE          DT_NODELABEL(adc)
#endif
#define NTC_ADC_CHANNEL_ID    1
#define NTC_ADC_RESOLUTION    12
#define NTC_ADC_GAIN          BATTERY_ADC_NTC_GAIN
#define NTC_ADC_REFERENCE     BATTERY_ADC_NTC_REFERENCE
#define NTC_ADC_ACQ_TIME      ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 40)
#define NTC_ADC_INPUT         BATTERY_ADC_NTC_INPUT

/* ── Circuit parameters ─────────────────────────────────────────── */

#define NTC_PULLUP_OHM        10000
#define NTC_VDD_MV            3300

/* ── Static state ───────────────────────────────────────────────── */

static const struct device *g_adc_dev = DEVICE_DT_GET(NTC_ADC_NODE);
static int16_t g_ntc_sample_buffer;

static const struct adc_channel_cfg g_ntc_channel_cfg = {
    .gain             = NTC_ADC_GAIN,
    .reference        = NTC_ADC_REFERENCE,
    .acquisition_time = NTC_ADC_ACQ_TIME,
    .channel_id       = NTC_ADC_CHANNEL_ID,
#if defined(CONFIG_ADC_CONFIGURABLE_INPUTS)
    .input_positive   = NTC_ADC_INPUT,
#endif
};

int battery_hal_temp_init(void)
{
    if (!device_is_ready(g_adc_dev)) {
        return BATTERY_STATUS_IO;
    }

    if (adc_channel_setup(g_adc_dev, &g_ntc_channel_cfg) < 0) {
        return BATTERY_STATUS_IO;
    }

    return BATTERY_STATUS_OK;
}

int battery_hal_temp_read_c_x100(int32_t *temp_c_x100_out)
{
    struct adc_sequence sequence = {
        .channels     = BIT(NTC_ADC_CHANNEL_ID),
        .buffer       = &g_ntc_sample_buffer,
        .buffer_size  = sizeof(g_ntc_sample_buffer),
        .resolution   = NTC_ADC_RESOLUTION,
        .oversampling = 4,
        .calibrate    = true,
    };
    int32_t adc_mv;
    uint32_t resistance;
    int rc;

    if (temp_c_x100_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    /*
     * Re-setup the channel before every read.  The VDD channel (ch 0)
     * uses an internal input that clobbers ch 1's AIN mux in the nRF
     * SAADC driver.  Re-calling adc_channel_setup() restores AIN1.
     */
    if (adc_channel_setup(g_adc_dev, &g_ntc_channel_cfg) < 0) {
        return BATTERY_STATUS_IO;
    }

    /* Step 1: Read ADC */
    if (adc_read(g_adc_dev, &sequence) < 0) {
        return BATTERY_STATUS_IO;
    }

    /* Step 2: Convert raw → millivolts */
    adc_mv = g_ntc_sample_buffer;
    if (adc_raw_to_millivolts(BATTERY_ADC_NTC_REF_MV, NTC_ADC_GAIN, NTC_ADC_RESOLUTION,
                              &adc_mv) < 0) {
        return BATTERY_STATUS_ERROR;
    }

    /* Step 3: Convert millivolts → resistance */
    rc = battery_ntc_resistance_from_mv(NTC_PULLUP_OHM, NTC_VDD_MV,
                                        (uint32_t)adc_mv, &resistance);
    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }

    /* Step 4: Look up resistance → temperature */
    rc = battery_ntc_lut_interpolate(&battery_ntc_lut_10k_3950,
                                     resistance, temp_c_x100_out);
    return rc;
}
