#ifndef BATTERY_INTERNAL_H
#define BATTERY_INTERNAL_H

#include <stdbool.h>

struct battery_sdk_runtime_state {
    bool adc_initialized;
    bool voltage_initialized;
    bool temperature_initialized;
    bool soc_initialized;
    bool power_manager_initialized;
    bool telemetry_initialized;
};

struct battery_sdk_runtime_state *battery_sdk_state(void);

#endif /* BATTERY_INTERNAL_H */