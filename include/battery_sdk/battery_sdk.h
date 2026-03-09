#ifndef BATTERY_SDK_H
#define BATTERY_SDK_H

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

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SDK_H */
