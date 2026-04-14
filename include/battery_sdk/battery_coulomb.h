/*
 * Coulomb counter — tracks charge consumed/added via current integration.
 *
 * Uses trapezoidal rule with integer-only arithmetic.
 * Persists accumulated charge to NVS for reboot survival.
 */

#ifndef BATTERY_SDK_BATTERY_COULOMB_H
#define BATTERY_SDK_BATTERY_COULOMB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int battery_coulomb_init(void);
int battery_coulomb_update(int32_t current_ma_x100, uint32_t dt_ms);
int battery_coulomb_get_mah_x100(int32_t *mah_x100_out);
int battery_coulomb_reset(int32_t mah_x100);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SDK_BATTERY_COULOMB_H */
