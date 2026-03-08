#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "battery_adc.h"

LOG_MODULE_REGISTER(app_main, LOG_LEVEL_INF);

int main(void)
{
    int err;
    int heartbeat = 0;

    LOG_INF("Battery SDK experiment template booting");
    LOG_INF("Phase 0 firmware baseline started");

    err = battery_adc_init();
    if (err) {
        LOG_ERR("battery_adc_init failed: %d", err);
    }

    while (1) {
        int mv;

        heartbeat++;
        LOG_INF("Heartbeat #%d", heartbeat);

        mv = battery_adc_read_mv();
        if (mv >= 0) {
            LOG_INF("Battery sample: %d mV", mv);
        } else {
            LOG_ERR("Battery sample failed: %d", mv);
        }

        k_sleep(K_SECONDS(3));
    }

    return 0;
}
