#include <battery_sdk/battery_hal_current.h>
#include <battery_sdk/battery_status.h>

int battery_hal_current_init(void)
{
    return BATTERY_STATUS_UNSUPPORTED;
}

int battery_hal_current_read_ma_x100(int32_t *current_ma_x100_out)
{
    (void)current_ma_x100_out;
    return BATTERY_STATUS_UNSUPPORTED;
}
