/*
 * Telemetry transport abstraction.
 *
 * Provides a uniform interface for sending telemetry packets over
 * different backends (BLE, serial, etc.).  The active backend is
 * selected at compile time via Kconfig.
 *
 * Available when CONFIG_BATTERY_TRANSPORT=y.
 */

#ifndef BATTERY_SDK_BATTERY_TRANSPORT_H
#define BATTERY_SDK_BATTERY_TRANSPORT_H

#include <battery_sdk/battery_types.h>
#include <battery_sdk/battery_status.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BATTERY_TRANSPORT_WIRE_SIZE_V1 20
#define BATTERY_TRANSPORT_WIRE_SIZE_V2 24
#define BATTERY_TRANSPORT_WIRE_SIZE_V3 32
#define BATTERY_TRANSPORT_WIRE_SIZE    BATTERY_TRANSPORT_WIRE_SIZE_V3

/**
 * Transport backend operations — compile-time vtable.
 *
 * Each backend (BLE, serial, …) provides a const instance of this
 * struct.  The transport layer selects the active backend at compile
 * time via Kconfig.
 */
struct battery_transport_ops {
    int (*init)(void);
    int (*send)(const uint8_t *buf, uint8_t len);
    int (*deinit)(void);
    int (*is_connected)(bool *connected_out);
};

/**
 * Initialize the transport subsystem.
 *
 * Calls the active backend's init function.  Should be called after
 * all other SDK subsystems are initialized.
 *
 * @return BATTERY_STATUS_OK, BATTERY_STATUS_IO, or
 *         BATTERY_STATUS_UNSUPPORTED (no backend compiled in).
 */
int battery_transport_init(void);

/**
 * Serialize and send a telemetry packet.
 *
 * Packs the packet into a 20-byte wire buffer and forwards it to
 * the active backend.
 *
 * @param packet  Telemetry packet to send (must not be NULL)
 * @return BATTERY_STATUS_OK, BATTERY_STATUS_INVALID_ARG,
 *         BATTERY_STATUS_UNSUPPORTED, or BATTERY_STATUS_IO.
 */
int battery_transport_send(const struct battery_telemetry_packet *packet);

/**
 * Shut down the transport subsystem.
 *
 * @return BATTERY_STATUS_OK or BATTERY_STATUS_UNSUPPORTED.
 */
int battery_transport_deinit(void);

/**
 * Query whether a remote client is connected / subscribed.
 *
 * @param connected_out  true if a client is actively listening
 * @return BATTERY_STATUS_OK, BATTERY_STATUS_INVALID_ARG, or
 *         BATTERY_STATUS_UNSUPPORTED.
 */
int battery_transport_is_connected(bool *connected_out);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SDK_BATTERY_TRANSPORT_H */
