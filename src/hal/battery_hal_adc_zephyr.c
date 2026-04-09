#include "battery_hal.h"

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>

#include "helpers/battery_adc_platform.h"

/*
 * ── STM32: Use Zephyr vref sensor for VDD measurement ──────────────
 *
 * STM32 cannot read VDD directly.  The Zephyr "st,stm32-vref" sensor
 * driver handles VREFINT ADC path enable, stabilization delay, and
 * factory calibration.  We use the sensor API here (same pattern as
 * the die temperature sensor), which avoids raw ADC channel config.
 *
 * ── nRF52: Use SAADC with internal VDD input ───────────────────────
 *
 * nRF52 reads VDD directly via the SAADC internal input, with a
 * 0.6 V reference and 1/6 gain.
 */

#if defined(BATTERY_ADC_VDD_USE_VREFINT)

/* ── STM32 path: vref sensor ──────────────────────────────────────── */

#include <zephyr/drivers/sensor.h>

static const struct device *g_vref_dev = DEVICE_DT_GET(DT_NODELABEL(vref));
static int32_t g_last_vdd_mv;

int battery_hal_adc_init(void)
{
    if (!device_is_ready(g_vref_dev)) {
        return BATTERY_STATUS_IO;
    }
    return BATTERY_STATUS_OK;
}

int battery_hal_adc_read_raw(int16_t *raw_out)
{
    struct sensor_value val;

    if (raw_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    if (sensor_sample_fetch(g_vref_dev) < 0) {
        return BATTERY_STATUS_IO;
    }

    if (sensor_channel_get(g_vref_dev, SENSOR_CHAN_VOLTAGE, &val) < 0) {
        return BATTERY_STATUS_IO;
    }

    /* vref sensor returns VDD in volts (val1=V, val2=uV).
     * Convert to mV and store for raw_to_pin_mv to return. */
    g_last_vdd_mv = val.val1 * 1000 + val.val2 / 1000;

    /* Return a dummy "raw" value; the real mV is in g_last_vdd_mv. */
    *raw_out = (int16_t)(g_last_vdd_mv > 32767 ? 32767 : g_last_vdd_mv);
    return BATTERY_STATUS_OK;
}

int battery_hal_adc_raw_to_pin_mv(int16_t raw, int32_t *mv_out)
{
    (void)raw;  /* On STM32, the real value was captured in read_raw. */

    if (mv_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    *mv_out = g_last_vdd_mv;
    return BATTERY_STATUS_OK;
}

#elif defined(BATTERY_ADC_VDD_USE_DIVIDER)

/* ── ESP32 path: external voltage divider ────────────────────────── */

#define BATTERY_ADC_NODE BATTERY_ADC_DT_NODE

#if !DT_NODE_EXISTS(BATTERY_ADC_NODE)
#error "ADC node is missing in devicetree"
#endif

#define BATTERY_ADC_RESOLUTION       12
#define BATTERY_ADC_GAIN             BATTERY_ADC_VDD_GAIN
#define BATTERY_ADC_REFERENCE        BATTERY_ADC_VDD_REFERENCE
#define BATTERY_ADC_ACQ_TIME         ADC_ACQ_TIME_DEFAULT
#define BATTERY_ADC_CHANNEL_ID       0
#define BATTERY_ADC_INPUT_POSITIVE   BATTERY_ADC_VDD_INPUT

static const struct device *g_adc_dev = DEVICE_DT_GET(BATTERY_ADC_NODE);
static int16_t g_adc_sample_buffer;

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
    };

    if (raw_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

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

    if (adc_raw_to_millivolts(BATTERY_ADC_VDD_REF_MV,
                               BATTERY_ADC_GAIN,
                               BATTERY_ADC_RESOLUTION,
                               &value) < 0) {
        return BATTERY_STATUS_ERROR;
    }

    /* Apply voltage divider ratio to reconstruct battery voltage. */
    value *= BATTERY_ADC_VDD_DIVIDER_RATIO;

    *mv_out = value;
    return BATTERY_STATUS_OK;
}

#else

/* ── nRF52 path: direct SAADC VDD read ───────────────────────────── */

#define BATTERY_ADC_NODE BATTERY_ADC_DT_NODE

#if !DT_NODE_EXISTS(BATTERY_ADC_NODE)
#error "ADC node is missing in devicetree"
#endif

#define BATTERY_ADC_RESOLUTION       12
#define BATTERY_ADC_GAIN             BATTERY_ADC_VDD_GAIN
#define BATTERY_ADC_REFERENCE        BATTERY_ADC_VDD_REFERENCE
#define BATTERY_ADC_ACQ_TIME         ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 40)
#define BATTERY_ADC_CHANNEL_ID       0
#define BATTERY_ADC_INPUT_POSITIVE   BATTERY_ADC_VDD_INPUT

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

    if (adc_raw_to_millivolts(BATTERY_ADC_VDD_REF_MV,
                               BATTERY_ADC_GAIN,
                               BATTERY_ADC_RESOLUTION,
                               &value) < 0) {
        return BATTERY_STATUS_ERROR;
    }

    *mv_out = value;
    return BATTERY_STATUS_OK;
}

#endif /* BATTERY_ADC_VDD_USE_VREFINT */
