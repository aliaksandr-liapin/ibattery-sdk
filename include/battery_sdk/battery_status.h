#ifndef BATTERY_SDK_BATTERY_STATUS_H
#define BATTERY_SDK_BATTERY_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

enum battery_status {
    BATTERY_STATUS_OK = 0,
    BATTERY_STATUS_ERROR = -1,
    BATTERY_STATUS_INVALID_ARG = -2,
    BATTERY_STATUS_NOT_INITIALIZED = -3,
    BATTERY_STATUS_UNSUPPORTED = -4,
    BATTERY_STATUS_IO = -5
};

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SDK_BATTERY_STATUS_H */