#include <stddef.h>

#include <battery_sdk/battery_power_manager.h>
#include <battery_sdk/battery_voltage.h>
#include <battery_sdk/battery_status.h>

#include "../core/battery_internal.h"
#include "../hal/battery_hal.h"

#if defined(CONFIG_BATTERY_CHARGER_TP4056)
#include "../hal/battery_hal_charger.h"
#endif

/* Voltage thresholds for state transitions (millivolts).
 * Hysteresis band: drop below CRITICAL_ENTER to enter CRITICAL,
 * rise above CRITICAL_EXIT to return to ACTIVE.                 */
#define BATTERY_POWER_CRITICAL_ENTER_MV  2100
#define BATTERY_POWER_CRITICAL_EXIT_MV   2200

/* Inactivity timeouts (milliseconds) */
#define BATTERY_POWER_IDLE_TIMEOUT_MS    (30U  * 1000U)  /* 30 s  -> IDLE  */
#define BATTERY_POWER_SLEEP_TIMEOUT_MS   (120U * 1000U)  /* 120 s -> SLEEP */

static enum battery_power_state g_current_state = BATTERY_POWER_STATE_UNKNOWN;
static uint32_t g_last_activity_ms;

int battery_power_manager_init(void)
{
    struct battery_sdk_runtime_state *state = battery_sdk_state();
    uint32_t now = 0;

    /* Seed the activity timer with current uptime so we don't immediately
     * enter IDLE/SLEEP on boot.  If uptime fails, zero is acceptable. */
    (void)battery_hal_get_uptime_ms(&now);
    g_last_activity_ms = now;

    g_current_state = BATTERY_POWER_STATE_ACTIVE;
    state->power_manager_initialized = true;
    return BATTERY_STATUS_OK;
}

int battery_power_manager_report_activity(void)
{
    uint32_t now = 0;
    int rc = battery_hal_get_uptime_ms(&now);

    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }

    g_last_activity_ms = now;
    return BATTERY_STATUS_OK;
}

int battery_power_manager_get_state(enum battery_power_state *state_out)
{
    struct battery_sdk_runtime_state *state = battery_sdk_state();
    uint16_t voltage_mv;
    uint32_t now_ms = 0;
    int rc;

    if (state_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    if (!state->power_manager_initialized) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }

    /* ── 1. Read voltage ─────────────────────────────────────────────── */
    rc = battery_voltage_get_mv(&voltage_mv);
    if (rc != BATTERY_STATUS_OK) {
        /* Cannot determine state — return last known */
        *state_out = g_current_state;
        return BATTERY_STATUS_OK;
    }

    /* ── 2. CRITICAL check (highest priority) ────────────────────────── */
#if defined(CONFIG_BATTERY_CHARGER_TP4056)
    {
        bool charging = false;
        /* Allow exit from CRITICAL if charger is actively charging */
        if (g_current_state == BATTERY_POWER_STATE_CRITICAL) {
            (void)battery_hal_charger_is_charging(&charging);
            if (charging) {
                g_current_state = BATTERY_POWER_STATE_CHARGING;
            } else if (voltage_mv > BATTERY_POWER_CRITICAL_EXIT_MV) {
                /* Recovered — fall through to charger/inactivity logic */
                g_current_state = BATTERY_POWER_STATE_UNKNOWN;
            } else {
                *state_out = g_current_state;
                return BATTERY_STATUS_OK;
            }
        }

        /* Enter CRITICAL if voltage drops below threshold (charger off) */
        if (voltage_mv < BATTERY_POWER_CRITICAL_ENTER_MV) {
            (void)battery_hal_charger_is_charging(&charging);
            if (!charging) {
                g_current_state = BATTERY_POWER_STATE_CRITICAL;
                *state_out = g_current_state;
                return BATTERY_STATUS_OK;
            }
            /* Charging with low voltage — stay in CHARGING */
        }
    }
#else
    if (g_current_state == BATTERY_POWER_STATE_CRITICAL) {
        if (voltage_mv > BATTERY_POWER_CRITICAL_EXIT_MV) {
            g_current_state = BATTERY_POWER_STATE_ACTIVE;
        }
        *state_out = g_current_state;
        return BATTERY_STATUS_OK;
    }

    if (voltage_mv < BATTERY_POWER_CRITICAL_ENTER_MV) {
        g_current_state = BATTERY_POWER_STATE_CRITICAL;
        *state_out = g_current_state;
        return BATTERY_STATUS_OK;
    }
#endif

    /* ── 3. Charger state (when TP4056 is enabled) ───────────────────── */
#if defined(CONFIG_BATTERY_CHARGER_TP4056)
    {
        bool charging = false;
        bool charged  = false;

        if (battery_hal_charger_is_charging(&charging) == BATTERY_STATUS_OK &&
            charging) {
            g_current_state = BATTERY_POWER_STATE_CHARGING;
            *state_out = g_current_state;
            return BATTERY_STATUS_OK;
        }

        if (battery_hal_charger_is_charged(&charged) == BATTERY_STATUS_OK &&
            charged) {
            g_current_state = BATTERY_POWER_STATE_CHARGED;
            *state_out = g_current_state;
            return BATTERY_STATUS_OK;
        }
    }
#endif

    /* ── 4. Inactivity logic ─────────────────────────────────────────── */
    rc = battery_hal_get_uptime_ms(&now_ms);
    if (rc == BATTERY_STATUS_OK) {
        uint32_t elapsed = now_ms - g_last_activity_ms;

        if (elapsed >= BATTERY_POWER_SLEEP_TIMEOUT_MS) {
            g_current_state = BATTERY_POWER_STATE_SLEEP;
            *state_out = g_current_state;
            return BATTERY_STATUS_OK;
        }

        if (elapsed >= BATTERY_POWER_IDLE_TIMEOUT_MS) {
            g_current_state = BATTERY_POWER_STATE_IDLE;
            *state_out = g_current_state;
            return BATTERY_STATUS_OK;
        }
    }
    /* If uptime read fails, skip inactivity — stay ACTIVE */

    /* ── 5. Default: ACTIVE (or DISCHARGING when charger is present) ── */
#if defined(CONFIG_BATTERY_CHARGER_TP4056)
    g_current_state = BATTERY_POWER_STATE_DISCHARGING;
#else
    g_current_state = BATTERY_POWER_STATE_ACTIVE;
#endif

    *state_out = g_current_state;
    return BATTERY_STATUS_OK;
}
