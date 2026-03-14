#ifndef BATTERY_HAL_CHARGER_H
#define BATTERY_HAL_CHARGER_H

#include <battery_sdk/battery_status.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Initialise the charger GPIO pins (CHRG + STDBY). */
int battery_hal_charger_init(void);

/** Read the TP4056 CHRG pin.  *charging_out = true when charging. */
int battery_hal_charger_is_charging(bool *charging_out);

/** Read the TP4056 STDBY pin.  *charged_out = true when charge is complete. */
int battery_hal_charger_is_charged(bool *charged_out);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_HAL_CHARGER_H */
