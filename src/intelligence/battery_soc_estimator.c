#include <stddef.h>

#include <battery_sdk/battery_soc_estimator.h>
#include <battery_sdk/battery_voltage.h>
#include <battery_sdk/battery_status.h>

#include "../core/battery_internal.h"

#define BATTERY_SOC_EMPTY_MV 3000
#define BATTERY_SOC_FULL_MV 4200

static uint16_t battery_soc_from_voltage_mv(int32_t voltage_mv)
{
    int32_t numerator;
    int32_t denominator;
    int32_t result;

    if (voltage_mv <= BATTERY_SOC_EMPTY_MV) {
        return 0U;
    }

    if (voltage_mv >= BATTERY_SOC_FULL_MV) {
        return 10000U;
    }

    numerator = (voltage_mv - BATTERY_SOC_EMPTY_MV) * 10000;
    denominator = (BATTERY_SOC_FULL_MV - BATTERY_SOC_EMPTY_MV);
    result = numerator / denominator;

    if (result < 0) {
        result = 0;
    }
    if (result > 10000) {
        result = 10000;
    }

    return (uint16_t)result;
}

int battery_soc_estimator_init(void)
{
    struct battery_sdk_runtime_state *state = battery_sdk_state();
    state->soc_initialized = true;
    return BATTERY_STATUS_OK;
}

int battery_soc_estimator_get_pct_x100(uint16_t *soc_pct_x100)
{
    int rc;
    int32_t voltage_mv = 0;
    struct battery_sdk_runtime_state *state = battery_sdk_state();

    if (soc_pct_x100 == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    if (!state->soc_initialized) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }

    rc = battery_voltage_get_mv(&voltage_mv);
    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }

    *soc_pct_x100 = battery_soc_from_voltage_mv(voltage_mv);
    return BATTERY_STATUS_OK;
}