/*
 * Runtime-to-empty (time remaining) estimator.
 *
 * Estimates the minutes until the battery is empty from an EMA-smoothed
 * discharge current and the remaining charge. Reports NOT_AVAILABLE while
 * the battery is idle or charging (no meaningful drain to extrapolate).
 *
 * Pure and host-testable: integer-only math, no heap, no Zephyr headers.
 *
 * Design doc: docs/plans/2026-06-04-runtime-to-empty-design.md
 */

#ifndef BATTERY_SDK_BATTERY_RUNTIME_H
#define BATTERY_SDK_BATTERY_RUNTIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Reset the estimator: clear the smoothed current and the prime flag. */
void battery_runtime_reset(void);

/** Feed one current sample into the EMA.
 *  @param current_ma_x100 Current in 0.01 mA units; positive = discharge. */
void battery_runtime_update(int32_t current_ma_x100);

/** Estimate minutes-to-empty from the smoothed discharge current.
 *  @param remaining_mah_x100 Remaining charge in 0.01 mAh units.
 *  @param minutes_out        Receives the estimate (only on OK).
 *  @return BATTERY_STATUS_OK with *minutes_out set, or
 *          BATTERY_STATUS_NOT_AVAILABLE when idle/charging. */
int battery_runtime_to_empty_min(int32_t remaining_mah_x100,
                                 uint32_t *minutes_out);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SDK_BATTERY_RUNTIME_H */
