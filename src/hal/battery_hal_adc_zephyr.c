#include "battery_hal.h"

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>

#include <helpers/nrfx_analog_common.h>

#define BATTERY_ADC_NODE DT_NODELABEL(adc)

#if !DT_NODE_EXISTS(BATTERY_ADC_NODE)
#error "ADC node is missing in devicetree"
#endif

#define BATTERY_ADC_RESOLUTION       12
#define BATTERY_ADC_GAIN             ADC_GAIN_1_6
#define BATTERY_ADC_REFERENCE        ADC_REF_INTERNAL
#define BATTERY_ADC_ACQ_TIME         ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 40)
#define BATTERY_ADC_CHANNEL_ID       0
#define BATTERY_ADC_INPUT_POSITIVE   NRFX_ANALOG_INTERNAL_VDD

static const struct device *g_adc_dev = DEVICE_DT_GET(BATTERY_ADC_NODE);
static int16_t g_adc_sample_buffer;

/*
 * Channel config is kept at file scope so it can be re-applied before
 * every read.  The nRF SAADC driver does not preserve per-channel
 * input-mux settings when other channels are read (e.g. the NTC
 * channel on AIN1), so we must restore ours each time.
 */
static const struct adc_channel_cfg g_vdd_channel_cfg = {
    .gain             = BATTERY_ADC_GAIN,
    .reference        = BATTERY_ADC_REFERENCE,
    .acquisition_time = BATTERY_ADC_ACQ_TIME,
    .channel_id       = BATTERY_ADC_CHANNEL_ID,
#if defined(CONFIG_ADC_CONFIGURABLE_INPUTS)
    .input_positive   = BATTERY_ADC_INPUT_POSITIVE,
#endif
};

int battery_hal_adc_init(void)
{
    if (!device_is_ready(g_adc_dev)) {
        return BATTERY_STATUS_IO;
    }

    if (adc_channel_setup(g_adc_dev, &g_vdd_channel_cfg) < 0) {
        return BATTERY_STATUS_IO;
    }

    return BATTERY_STATUS_OK;
}

int battery_hal_adc_read_raw(int16_t *raw_out)
{
    struct adc_sequence sequence = {
        .channels     = BIT(BATTERY_ADC_CHANNEL_ID),
        .buffer       = &g_adc_sample_buffer,
        .buffer_size  = sizeof(g_adc_sample_buffer),
        .resolution   = BATTERY_ADC_RESOLUTION,
        .oversampling = 4,
        .calibrate    = true,
    };

    if (raw_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    /* Re-setup channel before every read (see comment on g_vdd_channel_cfg). */
    if (adc_channel_setup(g_adc_dev, &g_vdd_channel_cfg) < 0) {
        return BATTERY_STATUS_IO;
    }

    if (adc_read(g_adc_dev, &sequence) < 0) {
        return BATTERY_STATUS_IO;
    }

    *raw_out = g_adc_sample_buffer;
    return BATTERY_STATUS_OK;
}

int battery_hal_adc_raw_to_pin_mv(int16_t raw, int32_t *mv_out)
{
    int32_t value = raw;

    if (mv_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    if (adc_raw_to_millivolts(600,
                               BATTERY_ADC_GAIN,
                               BATTERY_ADC_RESOLUTION,
                               &value) < 0) {
        return BATTERY_STATUS_ERROR;
    }

    *mv_out = value;
    return BATTERY_STATUS_OK;
}
