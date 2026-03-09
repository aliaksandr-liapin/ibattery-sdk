#ifndef BATTERY_VOLTAGE_FILTER_H
#define BATTERY_VOLTAGE_FILTER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Keep this configurable but bounded for embedded safety.
 * You can tune later without changing public SDK API.
 */
#define BATTERY_VOLTAGE_FILTER_MAX_WINDOW_SIZE   16U
#define BATTERY_VOLTAGE_FILTER_DEFAULT_WINDOW_SIZE  12

typedef struct
{
    uint16_t buffer[BATTERY_VOLTAGE_FILTER_MAX_WINDOW_SIZE];
    uint32_t sum;
    size_t window_size;
    size_t index;
    size_t count;
    bool initialized;
} battery_voltage_filter_t;

/*
 * Initializes the moving average filter.
 * If window_size is invalid, default size is used.
 */
int battery_voltage_filter_init(battery_voltage_filter_t *filter, size_t window_size);

/*
 * Clears runtime state but preserves configured window size.
 */
int battery_voltage_filter_reset(battery_voltage_filter_t *filter);

/*
 * Pushes a new sample into the filter and returns the filtered result.
 */
int battery_voltage_filter_update(battery_voltage_filter_t *filter,
                                  uint16_t sample_mv,
                                  uint16_t *filtered_mv_out);

/*
 * Returns the current filtered value without adding a new sample.
 */
int battery_voltage_filter_get(const battery_voltage_filter_t *filter,
                               uint16_t *filtered_mv_out);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_VOLTAGE_FILTER_H */