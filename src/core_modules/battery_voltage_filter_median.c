/*
 * Median voltage filter — alternative to moving-average for use cases
 * with bursty load-induced voltage sag (e.g. BLE TX pulling Vbat down).
 *
 * Same public API as battery_voltage_filter.c. Selected via Kconfig
 * choice BATTERY_VOLTAGE_FILTER_TYPE.
 *
 * Algorithm: insertion-sort a copy of the buffer, return middle element
 * (or mean of two middle elements for even window).
 *
 * Memory: same battery_voltage_filter_t struct as moving-average filter.
 * The `sum` field is unused in this implementation.
 */

#include "battery_voltage_filter.h"

#include <battery_sdk/battery_status.h>

#include <string.h>

static size_t sanitize_window_size(size_t window_size)
{
    if ((window_size == 0U) ||
        (window_size > BATTERY_VOLTAGE_FILTER_MAX_WINDOW_SIZE)) {
        return BATTERY_VOLTAGE_FILTER_DEFAULT_WINDOW_SIZE;
    }
    return window_size;
}

int battery_voltage_filter_init(battery_voltage_filter_t *filter,
                                size_t window_size)
{
    if (filter == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    memset(filter, 0, sizeof(*filter));
    filter->window_size = sanitize_window_size(window_size);
    filter->initialized = true;
    return BATTERY_STATUS_OK;
}

int battery_voltage_filter_reset(battery_voltage_filter_t *filter)
{
    size_t preserved_window_size;

    if ((filter == NULL) || (filter->initialized == false)) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    preserved_window_size = filter->window_size;
    memset(filter, 0, sizeof(*filter));
    filter->window_size = preserved_window_size;
    filter->initialized = true;
    return BATTERY_STATUS_OK;
}

static void copy_and_sort(const battery_voltage_filter_t *filter,
                          uint16_t *sorted)
{
    size_t n = filter->count;

    for (size_t i = 0; i < n; i++) {
        sorted[i] = filter->buffer[i];
    }

    /* Insertion sort */
    for (size_t i = 1; i < n; i++) {
        uint16_t key = sorted[i];
        size_t j = i;
        while (j > 0 && sorted[j - 1] > key) {
            sorted[j] = sorted[j - 1];
            j--;
        }
        sorted[j] = key;
    }
}

static uint16_t median_of(const battery_voltage_filter_t *filter)
{
    uint16_t sorted[BATTERY_VOLTAGE_FILTER_MAX_WINDOW_SIZE];
    size_t n = filter->count;

    if (n == 0U) {
        return 0U;
    }

    copy_and_sort(filter, sorted);

    if ((n % 2U) == 1U) {
        return sorted[n / 2U];
    }
    return (uint16_t)(((uint32_t)sorted[n / 2U - 1U] +
                       (uint32_t)sorted[n / 2U]) / 2U);
}

int battery_voltage_filter_update(battery_voltage_filter_t *filter,
                                  uint16_t sample_mv,
                                  uint16_t *filtered_mv_out)
{
    if ((filter == NULL) || (filtered_mv_out == NULL) ||
        (filter->initialized == false)) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    filter->buffer[filter->index] = sample_mv;
    if (filter->count < filter->window_size) {
        filter->count++;
    }
    filter->index++;
    if (filter->index >= filter->window_size) {
        filter->index = 0U;
    }

    *filtered_mv_out = median_of(filter);
    return BATTERY_STATUS_OK;
}

int battery_voltage_filter_get(const battery_voltage_filter_t *filter,
                               uint16_t *filtered_mv_out)
{
    if ((filter == NULL) || (filtered_mv_out == NULL) ||
        (filter->initialized == false)) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    *filtered_mv_out = median_of(filter);
    return BATTERY_STATUS_OK;
}
