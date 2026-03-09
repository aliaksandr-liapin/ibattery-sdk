#include <battery_sdk/battery_soc_estimator.h>
#include <battery_sdk/battery_voltage.h>
#include <battery_sdk/battery_status.h>

#include "../core/battery_internal.h"
#include "battery_soc_lut.h"

#include <stddef.h>
#include <stdint.h>

int battery_soc_estimator_init(void)
{
    struct battery_sdk_runtime_state *state = battery_sdk_state();
    state->soc_initialized = true;
    return BATTERY_STATUS_OK;
}

int battery_soc_estimator_get_pct_x100(uint16_t *soc_pct_x100)
{
    int rc;
    uint16_t voltage_mv;

    if (soc_pct_x100 == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    rc = battery_voltage_get_mv(&voltage_mv);
    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }

    rc = battery_soc_lut_interpolate(&battery_soc_lut_cr2032,
                                     voltage_mv,
                                     soc_pct_x100);
    return rc;
}