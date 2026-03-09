#include "battery_telemetry.h"

#include <battery_sdk/battery_voltage.h>
#include <battery_sdk/battery_temperature.h>
#include <battery_sdk/battery_soc_estimator.h>

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

int battery_telemetry_collect(battery_telemetry_packet_t *packet)
{
    int rc;
    uint16_t voltage_mv;

    if (packet == NULL) {
        return -EINVAL;
    }

    rc = battery_voltage_get_mv(&voltage_mv);
    if (rc != 0) {
        return rc;
    }
    packet->voltage_mv = (int32_t)voltage_mv;

    rc = battery_temperature_get_c_x100(&packet->temperature_c_x100);
    if (rc != 0) {
        return rc;
    }

    rc = battery_soc_estimator_get_pct_x100(&packet->soc_pct_x100);
    if (rc != 0) {
        return rc;
    }

    return 0;
}