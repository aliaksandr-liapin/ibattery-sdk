/*
 * NVS HAL implementation for Zephyr — uses Zephyr NVS subsystem.
 *
 * Flash partition: "storage" (must be defined in the DTS).
 * The nRF52840-DK default DTS includes a storage partition.
 */

#include "battery_hal_nvs.h"
#include <battery_sdk/battery_status.h>

#include <zephyr/fs/nvs.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/device.h>

static struct nvs_fs g_nvs;
static bool g_nvs_initialized;

int battery_hal_nvs_init(void)
{
    int rc;

    if (g_nvs_initialized) {
        return BATTERY_STATUS_OK;
    }

    g_nvs.flash_device = FIXED_PARTITION_DEVICE(storage_partition);
    if (g_nvs.flash_device == NULL) {
        return BATTERY_STATUS_IO;
    }

    g_nvs.offset = FIXED_PARTITION_OFFSET(storage_partition);
    g_nvs.sector_size = 4096;   /* nRF52840 flash page size */
    g_nvs.sector_count = 2;     /* Minimum for wear leveling */

    rc = nvs_mount(&g_nvs);
    if (rc < 0) {
        return BATTERY_STATUS_IO;
    }

    g_nvs_initialized = true;
    return BATTERY_STATUS_OK;
}

int battery_hal_nvs_read_u32(uint16_t key, uint32_t *value)
{
    ssize_t bytes;

    if (value == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    if (!g_nvs_initialized) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }

    bytes = nvs_read(&g_nvs, key, value, sizeof(*value));
    if (bytes < 0 || bytes != sizeof(*value)) {
        /* Key not found or read error — caller gets default */
        return BATTERY_STATUS_ERROR;
    }

    return BATTERY_STATUS_OK;
}

int battery_hal_nvs_write_u32(uint16_t key, uint32_t value)
{
    ssize_t bytes;

    if (!g_nvs_initialized) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }

    bytes = nvs_write(&g_nvs, key, &value, sizeof(value));
    if (bytes < 0) {
        return BATTERY_STATUS_IO;
    }

    return BATTERY_STATUS_OK;
}
