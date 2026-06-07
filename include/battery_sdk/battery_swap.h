/*
 * Swap-aware SoH auto-reset (Phase 1 — primary cell, power-off swap).
 *
 * Detects a power-off battery swap (device off -> cell swapped -> on) on the
 * first SoC reading after boot: if SoC jumped upward vs a baseline persisted
 * to flash (NVS) before power-off, the cell was replaced. On detection, the
 * learned SoH is reset to rated and a session flag is raised so telemetry can
 * report BATTERY_TELEMETRY_FLAG_BATTERY_SWAPPED.
 *
 * Primary-cell only this phase: a rechargeable charged while powered off would
 * look identical, so the Kconfig gate defaults on only for CR2032.
 *
 * The persisted baseline is refreshed as SoC drains (throttled by a configurable
 * delta) so the next boot compares against a recent value.
 *
 * Stateful, integer-only, no heap. NVS access is best-effort.
 *
 * Design doc: docs/plans/2026-06-06-swap-aware-soh-design.md
 */

#ifndef BATTERY_SDK_BATTERY_SWAP_H
#define BATTERY_SDK_BATTERY_SWAP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize swap detection. Loads the persisted SoC baseline from NVS (if
 *  any) and arms the one-shot boot check. Clears the detected flag. */
void battery_swap_init(void);

/** Feed one SoC reading. On the first call after init, compares against the
 *  persisted baseline: an upward jump beyond the threshold is treated as a
 *  swap (resets learned SoH, raises the detected flag). Always persists/refreshes
 *  the baseline (first call, or thereafter when SoC has dropped past the delta).
 *  @param soc_pct_x100 State of charge in 0.01% units (0..10000). */
void battery_swap_update(uint16_t soc_pct_x100);

/** True if a battery swap was detected this session. */
bool battery_swap_detected(void);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SDK_BATTERY_SWAP_H */
