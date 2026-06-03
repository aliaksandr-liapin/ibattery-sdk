/*
 * Module-consumption smoke test: exercise the public SDK entry points so the
 * linker must resolve the whole init chain (incl. battery_soh.c when
 * CONFIG_BATTERY_SOC_SOH=y) through the Zephyr-module build path.
 */
#include <battery_sdk/battery_sdk.h>
#include <battery_sdk/battery_telemetry.h>

int main(void)
{
    struct battery_telemetry_packet pkt;

    (void)battery_sdk_init();
    (void)battery_telemetry_collect(&pkt);
    return 0;
}
