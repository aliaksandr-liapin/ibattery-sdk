#ifndef BATTERY_SDK_BATTERY_SOC_ESTIMATOR_H
#define BATTERY_SDK_BATTERY_SOC_ESTIMATOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int battery_soc_estimator_init(void);
int battery_soc_estimator_get_pct_x100(uint16_t *soc_pct_x100);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SDK_BATTERY_SOC_ESTIMATOR_H */