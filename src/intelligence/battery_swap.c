#include <battery_sdk/battery_swap.h>
#include <battery_sdk/battery_soh.h>
#include <battery_sdk/battery_status.h>
#include "../hal/battery_hal_nvs.h"
#include <stdint.h>
#include <stdbool.h>

#ifndef CONFIG_BATTERY_SWAP_SOC_THRESHOLD_PCT_X100
#define CONFIG_BATTERY_SWAP_SOC_THRESHOLD_PCT_X100 2500
#endif
#ifndef CONFIG_BATTERY_SWAP_PERSIST_DELTA_PCT_X100
#define CONFIG_BATTERY_SWAP_PERSIST_DELTA_PCT_X100 500
#endif

static int32_t g_baseline_x100;
static bool    g_have_baseline;
static bool    g_boot_checked;
static bool    g_swapped;

static void persist(int32_t soc_x100)
{
    g_baseline_x100 = soc_x100;
    g_have_baseline = true;
    (void)battery_hal_nvs_write_u32(BATTERY_NVS_KEY_LAST_SOC, (uint32_t)soc_x100);
}

void battery_swap_init(void)
{
    uint32_t stored = 0;
    g_boot_checked = false;
    g_swapped = false;
    if (battery_hal_nvs_read_u32(BATTERY_NVS_KEY_LAST_SOC, &stored) == BATTERY_STATUS_OK) {
        g_baseline_x100 = (int32_t)stored;
        g_have_baseline = true;
    } else {
        g_baseline_x100 = 0;
        g_have_baseline = false;
    }
}

void battery_swap_update(uint16_t soc_pct_x100)
{
    int32_t soc = (int32_t)soc_pct_x100;
    if (!g_boot_checked) {
        g_boot_checked = true;
        if (g_have_baseline &&
            (soc - g_baseline_x100) >= CONFIG_BATTERY_SWAP_SOC_THRESHOLD_PCT_X100) {
            (void)battery_soh_reset();
            g_swapped = true;
        }
        persist(soc);
        return;
    }
    if ((g_baseline_x100 - soc) >= CONFIG_BATTERY_SWAP_PERSIST_DELTA_PCT_X100) {
        persist(soc);
    }
}

bool battery_swap_detected(void) { return g_swapped; }
