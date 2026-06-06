/*
 * Runtime-to-empty (time remaining) estimator.
 *
 * Smooths the discharge current with an integer EMA, then divides the
 * remaining charge by that smoothed current to extrapolate minutes-to-empty.
 * Reports NOT_AVAILABLE while idle or charging (the smoothed current sits at
 * or below the idle threshold), since there is no meaningful drain to project.
 *
 * Stateful (one smoothed sample + a prime flag), integer-only, no heap.
 * The final division uses int64 to keep low-drain estimates from overflowing.
 *
 * Design doc: docs/plans/2026-06-04-runtime-to-empty-design.md
 */

#include "battery_sdk/battery_runtime.h"
#include "battery_sdk/battery_status.h"

#include <stddef.h>
#include <stdint.h>

/* EMA weight for the newest sample, in 0.001 units (300 => 0.30). */
#ifndef CONFIG_BATTERY_RUNTIME_EMA_ALPHA_X1000
#define CONFIG_BATTERY_RUNTIME_EMA_ALPHA_X1000 300
#endif

/* At or below this smoothed current (0.01 mA), treat as idle/charging.
 * Zero means "any positive drain counts"; negative (charging) and exactly
 * zero (idle) are reported NOT_AVAILABLE. */
#ifndef CONFIG_BATTERY_RUNTIME_IDLE_THRESHOLD_MA_X100
#define CONFIG_BATTERY_RUNTIME_IDLE_THRESHOLD_MA_X100 0
#endif

static int32_t g_ema_x100;
static int     g_primed;

void battery_runtime_reset(void)
{
    g_ema_x100 = 0;
    g_primed = 0;
}

void battery_runtime_update(int32_t current_ma_x100)
{
    if (!g_primed) {
        /* Seed the EMA with the first sample so it converges quickly. */
        g_ema_x100 = current_ma_x100;
        g_primed = 1;
        return;
    }

    /* ema = (a * sample + (1000 - a) * ema + 500) / 1000
     *
     * The `+ 500` rounds to nearest (the division truncates toward zero
     * otherwise). int64 math avoids overflow on the products. */
    int32_t a = CONFIG_BATTERY_RUNTIME_EMA_ALPHA_X1000;
    g_ema_x100 = (int32_t)(((int64_t)a * current_ma_x100
                            + (int64_t)(1000 - a) * g_ema_x100
                            + 500) / 1000);
}

int battery_runtime_to_empty_min(int32_t remaining_mah_x100,
                                 uint32_t *minutes_out)
{
    if (minutes_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    /* Idle or charging: no meaningful drain to extrapolate. */
    if (g_ema_x100 <= CONFIG_BATTERY_RUNTIME_IDLE_THRESHOLD_MA_X100) {
        return BATTERY_STATUS_NOT_AVAILABLE;
    }

    if (remaining_mah_x100 <= 0) {
        *minutes_out = 0;
        return BATTERY_STATUS_OK;
    }

    /* minutes = (remaining_mah / current_ma) * 60
     *         = (remaining_mah_x100 * 60) / current_ma_x100
     * (the x100 scales cancel). int64 keeps low-drain cases from overflowing
     * before the divide; clamp to fit uint32_t. */
    int64_t minutes = ((int64_t)remaining_mah_x100 * 60) / g_ema_x100;
    if (minutes > (int64_t)(UINT32_MAX - 1)) {
        minutes = UINT32_MAX - 1;
    }

    *minutes_out = (uint32_t)minutes;
    return BATTERY_STATUS_OK;
}
