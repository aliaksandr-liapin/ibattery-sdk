/*
 * Phase 8d: on-device State of Health (capacity-fade learning).
 *
 * Learns the battery's true usable capacity from full->empty discharge
 * excursions: the coulomb integral between a full anchor and an empty
 * anchor is the actual delivered charge. Smooths the learned capacity
 * with an integer EMA and reports SoH = learned / rated.
 *
 * Stateful, integer-only, no heap. RAM-only (no persistence in the MVP).
 *
 * Design doc: docs/plans/2026-05-29-phase-8d-soh-design.md
 */

#ifndef BATTERY_SDK_BATTERY_SOH_H
#define BATTERY_SDK_BATTERY_SOH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize SoH tracking. learned_capacity := rated; disarmed.
 *  @param rated_mah_x100 Rated capacity in 0.01 mAh units (> 0). */
int battery_soh_init(int32_t rated_mah_x100);

/** Arm a measurable excursion: a full anchor has just fired. */
void battery_soh_note_full_anchor(void);

/** Feed the empty-anchor edge. If armed, compute measured capacity
 *  (rated - q_before_empty), EMA-update learned capacity if the value
 *  passes the plausibility guard, then disarm. No-op if not armed.
 *  @param q_before_empty_x100 Remaining charge read just before the
 *         coulomb counter is reset to 0 at the empty anchor. */
int battery_soh_observe_empty_anchor(int32_t q_before_empty_x100);

/** SoH in 0.01% units (0..10000). */
int battery_soh_get_pct_x100(uint16_t *soh_x100_out);

/** Learned usable capacity in 0.01 mAh units. */
int battery_soh_get_learned_capacity_mah_x100(int32_t *cap_x100_out);

/** Reset learned capacity to rated and disarm. */
int battery_soh_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SDK_BATTERY_SOH_H */
