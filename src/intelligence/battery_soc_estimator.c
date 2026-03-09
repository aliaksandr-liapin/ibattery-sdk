#include <battery_sdk/battery_soc_estimator.h>
#include <battery_sdk/battery_voltage.h>

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

int battery_soc_estimator_get_pct_x100(uint16_t *soc_pct_x100)
{
    int rc;
    uint16_t voltage_mv;

    if (soc_pct_x100 == NULL) {
        return -EINVAL;
    }

    rc = battery_voltage_get_mv(&voltage_mv);
    if (rc != 0) {
        return rc;
    }

    /* Placeholder for Phase 1 Step 3 */
    *soc_pct_x100 = 0U;

    return 0;
}