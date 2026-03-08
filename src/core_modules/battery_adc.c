#include "battery_adc.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(battery_adc, LOG_LEVEL_INF);

#define ADC_NODE DT_NODELABEL(adc)

static const struct device *adc_dev = DEVICE_DT_GET(ADC_NODE);

#define BATTERY_ADC_CHANNEL_ID 0
#define BATTERY_ADC_RESOLUTION 12
#define BATTERY_ADC_GAIN ADC_GAIN_1_6
#define BATTERY_ADC_REFERENCE ADC_REF_INTERNAL

static bool adc_ready = false;

static struct adc_channel_cfg channel_cfg = {
    .gain = BATTERY_ADC_GAIN,
    .reference = BATTERY_ADC_REFERENCE,
    .acquisition_time = ADC_ACQ_TIME_DEFAULT,
    .channel_id = BATTERY_ADC_CHANNEL_ID,
};

int battery_adc_init(void)
{
    int err;

    if (!device_is_ready(adc_dev)) {
        LOG_ERR("ADC device not ready");
        return -1;
    }

    err = adc_channel_setup(adc_dev, &channel_cfg);
    if (err) {
        LOG_ERR("adc_channel_setup failed: %d", err);
        return err;
    }

    adc_ready = true;
    LOG_INF("Battery ADC initialized");

    return 0;
}

int battery_adc_read_mv(void)
{
    int err;
    int16_t raw;

    struct adc_sequence sequence = {
        .channels = BIT(BATTERY_ADC_CHANNEL_ID),
        .buffer = &raw,
        .buffer_size = sizeof(raw),
        .resolution = BATTERY_ADC_RESOLUTION,
    };

    if (!adc_ready) {
        LOG_ERR("battery_adc_read_mv called before init");
        return -13;
    }

    err = adc_read(adc_dev, &sequence);
    if (err) {
        LOG_ERR("adc_read failed: %d", err);
        return err;
    }

    int32_t mv = raw;

    err = adc_raw_to_millivolts(
        600,
        BATTERY_ADC_GAIN,
        BATTERY_ADC_RESOLUTION,
        &mv);

    if (err) {
        LOG_ERR("adc_raw_to_millivolts failed");
        return err;
    }

    LOG_INF("Battery voltage = %d mV", mv);

    return mv;
}