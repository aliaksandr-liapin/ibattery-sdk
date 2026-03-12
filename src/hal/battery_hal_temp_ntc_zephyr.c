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
 */

#include "battery_hal.h"
#include "battery_ntc_lut.h"

#include <stdint.h>
#include <stddef.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>

#include <hal/nrf_saadc.h>

/* ── ADC configuration for NTC channel ──────────────────────────── */

#define NTC_ADC_NODE          DT_NODELABEL(adc)
#define NTC_ADC_CHANNEL_ID    1
#define NTC_ADC_RESOLUTION    12
#define NTC_ADC_GAIN          ADC_GAIN_1_6
#define NTC_ADC_REFERENCE     ADC_REF_INTERNAL
#define NTC_ADC_ACQ_TIME      ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 40)
#define NTC_ADC_INPUT         NRF_SAADC_INPUT_AIN1    /* P0.03 */

/* ── Circuit parameters ─────────────────────────────────────────── */

#define NTC_PULLUP_OHM        10000
#define NTC_VDD_MV            3300

/* ── Static state ───────────────────────────────────────────────── */

static const struct device *g_adc_dev = DEVICE_DT_GET(NTC_ADC_NODE);
static int16_t g_ntc_sample_buffer;

int battery_hal_temp_init(void)
{
    struct adc_channel_cfg channel_cfg = {
        .gain             = NTC_ADC_GAIN,
        .reference        = NTC_ADC_REFERENCE,
        .acquisition_time = NTC_ADC_ACQ_TIME,
        .channel_id       = NTC_ADC_CHANNEL_ID,
#if defined(CONFIG_ADC_CONFIGURABLE_INPUTS)
        .input_positive   = NTC_ADC_INPUT,
#endif
    };

    if (!device_is_ready(g_adc_dev)) {
        return BATTERY_STATUS_IO;
    }

    if (adc_channel_setup(g_adc_dev, &channel_cfg) < 0) {
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

    /* Step 1: Read ADC */
    if (adc_read(g_adc_dev, &sequence) < 0) {
        return BATTERY_STATUS_IO;
    }

    /* Step 2: Convert raw → millivolts */
    adc_mv = g_ntc_sample_buffer;
    if (adc_raw_to_millivolts(600, NTC_ADC_GAIN, NTC_ADC_RESOLUTION,
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
