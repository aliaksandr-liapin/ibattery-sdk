#include <battery_sdk/battery_sdk.h>
#include <battery_sdk/battery_telemetry.h>
#include <battery_sdk/battery_voltage.h>
#include <battery_sdk/battery_temperature.h>
#include <battery_sdk/battery_soc_estimator.h>
#include <battery_sdk/battery_power_manager.h>
#include <battery_sdk/battery_cycle_counter.h>
#include <battery_sdk/battery_status.h>

#include "../core/battery_internal.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#if defined(CONFIG_BATTERY_CURRENT_SENSE)
#include <battery_sdk/battery_hal_current.h>
#include <battery_sdk/battery_coulomb.h>
#endif

#if defined(CONFIG_BATTERY_CURRENT_SENSE)
static uint32_t g_prev_timestamp_ms;
static bool g_prev_timestamp_valid;
#endif

int battery_telemetry_init(void)
{
    struct battery_sdk_runtime_state *state = battery_sdk_state();
    state->telemetry_initialized = true;
    return BATTERY_STATUS_OK;
}

int battery_telemetry_collect(struct battery_telemetry_packet *packet)
{
    int rc;
    uint16_t voltage_mv;
    uint32_t uptime_ms;
    enum battery_power_state power_state;

    if (packet == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    memset(packet, 0, sizeof(*packet));
    packet->telemetry_version = BATTERY_TELEMETRY_VERSION;

    /* Timestamp — best-effort */
    rc = battery_sdk_get_uptime_ms(&uptime_ms);
    if (rc == BATTERY_STATUS_OK) {
        packet->timestamp_ms = uptime_ms;
    } else {
        packet->status_flags |= BATTERY_TELEMETRY_FLAG_TIMESTAMP_ERR;
    }

    /* Voltage — best-effort */
    rc = battery_voltage_get_mv(&voltage_mv);
    if (rc == BATTERY_STATUS_OK) {
        packet->voltage_mv = (int32_t)voltage_mv;
    } else {
        packet->status_flags |= BATTERY_TELEMETRY_FLAG_VOLTAGE_ERR;
    }

    /* Temperature — best-effort */
    rc = battery_temperature_get_c_x100(&packet->temperature_c_x100);
    if (rc != BATTERY_STATUS_OK) {
        packet->status_flags |= BATTERY_TELEMETRY_FLAG_TEMP_ERR;
    }

    /* SoC — best-effort */
    rc = battery_soc_estimator_get_pct_x100(&packet->soc_pct_x100);
    if (rc != BATTERY_STATUS_OK) {
        packet->status_flags |= BATTERY_TELEMETRY_FLAG_SOC_ERR;
    }

    /* Power state — best-effort */
    rc = battery_power_manager_get_state(&power_state);
    if (rc == BATTERY_STATUS_OK) {
        packet->power_state = (uint8_t)power_state;
        /* Feed power state to cycle counter for transition detection */
        (void)battery_cycle_counter_update(packet->power_state);
    } else {
        packet->status_flags |= BATTERY_TELEMETRY_FLAG_POWER_STATE_ERR;
    }

    /* Cycle count — best-effort */
    (void)battery_cycle_counter_get(&packet->cycle_count);

    /* Current + coulomb — best-effort (v3) */
#if defined(CONFIG_BATTERY_CURRENT_SENSE)
    {
        int32_t current_ma_x100;
        rc = battery_hal_current_read_ma_x100(&current_ma_x100);
        if (rc == BATTERY_STATUS_OK) {
            packet->current_ma_x100 = current_ma_x100;
            uint32_t dt_ms = 0;
            if (g_prev_timestamp_valid) {
                dt_ms = packet->timestamp_ms - g_prev_timestamp_ms;
            }
            g_prev_timestamp_ms = packet->timestamp_ms;
            g_prev_timestamp_valid = true;

            rc = battery_coulomb_update(current_ma_x100, dt_ms);
            if (rc == BATTERY_STATUS_OK) {
                (void)battery_coulomb_get_mah_x100(&packet->coulomb_mah_x100);
            } else {
                packet->status_flags |= BATTERY_TELEMETRY_FLAG_COULOMB_ERR;
            }
        } else {
            packet->status_flags |= BATTERY_TELEMETRY_FLAG_CURRENT_ERR;
        }
    }
#endif

    /* Always succeeds — partial data is flagged, not fatal */
    return BATTERY_STATUS_OK;
}
