/*
 * HAL abstraction for non-volatile storage (NVS).
 *
 * On Zephyr: Zephyr NVS subsystem (wear-leveled flash).
 * In tests:  simple mock.
 */

#ifndef BATTERY_HAL_NVS_H
#define BATTERY_HAL_NVS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** NVS key identifiers. */
#define BATTERY_NVS_KEY_CYCLE_COUNT  1
#define BATTERY_NVS_KEY_COULOMB_MAH  2

/**
 * Initialize the NVS subsystem.
 *
 * @return BATTERY_STATUS_OK on success, error code otherwise.
 */
int battery_hal_nvs_init(void);

/**
 * Read a uint32 value from NVS.
 *
 * @param key      NVS key identifier.
 * @param value    Output: stored value.
 * @return BATTERY_STATUS_OK, BATTERY_STATUS_ERROR if key not found.
 */
int battery_hal_nvs_read_u32(uint16_t key, uint32_t *value);

/**
 * Write a uint32 value to NVS.
 *
 * @param key      NVS key identifier.
 * @param value    Value to store.
 * @return BATTERY_STATUS_OK on success, error code otherwise.
 */
int battery_hal_nvs_write_u32(uint16_t key, uint32_t value);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_HAL_NVS_H */
