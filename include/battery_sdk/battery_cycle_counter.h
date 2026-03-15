/*
 * Charge cycle counter.
 *
 * Tracks CHARGING → CHARGED transitions and persists the count
 * to flash via the HAL NVS interface.
 */

#ifndef BATTERY_SDK_BATTERY_CYCLE_COUNTER_H
#define BATTERY_SDK_BATTERY_CYCLE_COUNTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the cycle counter.
 *
 * Loads the persisted cycle count from flash (or starts at 0 if
 * no stored value exists).
 *
 * @return BATTERY_STATUS_OK on success.
 */
int battery_cycle_counter_init(void);

/**
 * Notify the cycle counter of a power state change.
 *
 * Call this every time the power state is evaluated.  The counter
 * internally tracks the previous state and increments when a
 * CHARGING → CHARGED transition is detected.
 *
 * @param power_state  Current power state (enum battery_power_state value).
 * @return BATTERY_STATUS_OK on success.
 */
int battery_cycle_counter_update(uint8_t power_state);

/**
 * Get the current charge cycle count.
 *
 * @param count_out  Output: number of completed charge cycles.
 * @return BATTERY_STATUS_OK, or BATTERY_STATUS_INVALID_ARG if NULL.
 */
int battery_cycle_counter_get(uint32_t *count_out);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SDK_BATTERY_CYCLE_COUNTER_H */
