#include <stddef.h>

#include <battery_sdk/battery_adc.h>
#include <battery_sdk/battery_status.h>

#include "../core/battery_internal.h"
#include "../hal/battery_hal.h"

int battery_adc_init(void)
{
    int rc;
    struct battery_sdk_runtime_state *state = battery_sdk_state();

    rc = battery_hal_init();
    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }

    state->adc_initialized = true;
    return BATTERY_STATUS_OK;
}

int battery_adc_read_raw(int16_t *raw_value)
{
    struct battery_sdk_runtime_state *state = battery_sdk_state();

    if (raw_value == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    if (!state->adc_initialized) {
        return BATTERY_STATUS_NOT_INITIALIZED;
    }

    return battery_hal_adc_read_raw(raw_value);
}