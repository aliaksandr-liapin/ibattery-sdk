#include "battery_voltage_filter.h"

#include <string.h>
#include <errno.h>

static size_t battery_voltage_filter_sanitize_window_size(size_t window_size)
{
    if ((window_size == 0U) || (window_size > BATTERY_VOLTAGE_FILTER_MAX_WINDOW_SIZE)) {
        return BATTERY_VOLTAGE_FILTER_DEFAULT_WINDOW_SIZE;
    }

    return window_size;
}

int battery_voltage_filter_init(battery_voltage_filter_t *filter, size_t window_size)
{
    if (filter == NULL) {
        return -EINVAL;
    }

    memset(filter, 0, sizeof(*filter));

    filter->window_size = battery_voltage_filter_sanitize_window_size(window_size);
    filter->initialized = true;

    return 0;
}

int battery_voltage_filter_reset(battery_voltage_filter_t *filter)
{
    size_t preserved_window_size;

    if ((filter == NULL) || (filter->initialized == false)) {
        return -EINVAL;
    }

    preserved_window_size = filter->window_size;

    memset(filter->buffer, 0, sizeof(filter->buffer));
    filter->sum = 0U;
    filter->index = 0U;
    filter->count = 0U;
    filter->window_size = preserved_window_size;
    filter->initialized = true;

    return 0;
}

int battery_voltage_filter_update(battery_voltage_filter_t *filter,
                                  uint16_t sample_mv,
                                  uint16_t *filtered_mv_out)
{
    uint16_t oldest_sample;
    uint32_t average;

    if ((filter == NULL) || (filtered_mv_out == NULL) || (filter->initialized == false)) {
        return -EINVAL;
    }

    if (filter->count < filter->window_size) {
        filter->buffer[filter->index] = sample_mv;
        filter->sum += (uint32_t)sample_mv;
        filter->count++;
    } else {
        oldest_sample = filter->buffer[filter->index];
        filter->sum -= (uint32_t)oldest_sample;
        filter->buffer[filter->index] = sample_mv;
        filter->sum += (uint32_t)sample_mv;
    }

    filter->index++;
    if (filter->index >= filter->window_size) {
        filter->index = 0U;
    }

    average = filter->sum / (uint32_t)filter->count;
    *filtered_mv_out = (uint16_t)average;

    return 0;
}

int battery_voltage_filter_get(const battery_voltage_filter_t *filter,
                               uint16_t *filtered_mv_out)
{
    uint32_t average;

    if ((filter == NULL) || (filtered_mv_out == NULL) || (filter->initialized == false)) {
        return -EINVAL;
    }

    if (filter->count == 0U) {
        *filtered_mv_out = 0U;
        return 0;
    }

    average = filter->sum / (uint32_t)filter->count;
    *filtered_mv_out = (uint16_t)average;

    return 0;
}