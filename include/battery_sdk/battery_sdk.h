#ifndef BATTERY_SDK_BATTERY_SDK_H
#define BATTERY_SDK_BATTERY_SDK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize all Battery SDK subsystems.
 *
 * Must be called once before using any SDK functions.
 * Initializes subsystems in dependency order:
 *   HAL -> voltage -> temperature -> power_manager -> soc_estimator -> telemetry
 *
 * Returns BATTERY_STATUS_OK on success, or the first error encountered.
 */
int battery_sdk_init(void);

/**
 * Get system uptime in milliseconds.
 *
 * Provides a platform-independent timestamp via the HAL.
 *
 * @param uptime_ms_out  Pointer to receive uptime in milliseconds.
 * @return BATTERY_STATUS_OK on success, BATTERY_STATUS_INVALID_ARG if NULL,
 *         or BATTERY_STATUS_IO on HAL failure.
 */
int battery_sdk_get_uptime_ms(uint32_t *uptime_ms_out);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SDK_BATTERY_SDK_H */
