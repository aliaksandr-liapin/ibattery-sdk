/*
 * Transport abstraction layer.
 *
 * Dispatches init / send / deinit to the compile-time selected
 * backend.  Handles serialization so backends only receive raw bytes.
 */

#include <battery_sdk/battery_transport.h>
#include <battery_sdk/battery_status.h>
#include "battery_serialize.h"

#include <stddef.h>

/* ── Backend selection (compile-time vtable) ──────────────────────── */

#if defined(CONFIG_BATTERY_TRANSPORT_BLE)
extern const struct battery_transport_ops battery_transport_ble_ops;
static const struct battery_transport_ops *g_ops = &battery_transport_ble_ops;

#elif defined(CONFIG_BATTERY_TRANSPORT_MOCK)
extern const struct battery_transport_ops battery_transport_mock_ops;
static const struct battery_transport_ops *g_ops = &battery_transport_mock_ops;

#else
static const struct battery_transport_ops *g_ops = NULL;
#endif

/* ── Public API ───────────────────────────────────────────────────── */

int battery_transport_init(void)
{
    if (g_ops == NULL || g_ops->init == NULL) {
        return BATTERY_STATUS_UNSUPPORTED;
    }
    return g_ops->init();
}

int battery_transport_send(const struct battery_telemetry_packet *packet)
{
    uint8_t buf[BATTERY_SERIALIZE_BUF_SIZE];
    int rc;

    if (packet == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }
    if (g_ops == NULL || g_ops->send == NULL) {
        return BATTERY_STATUS_UNSUPPORTED;
    }

    rc = battery_serialize_pack(packet, buf, sizeof(buf));
    if (rc != BATTERY_STATUS_OK) {
        return rc;
    }

    return g_ops->send(buf, BATTERY_SERIALIZE_BUF_SIZE);
}

int battery_transport_deinit(void)
{
    if (g_ops == NULL || g_ops->deinit == NULL) {
        return BATTERY_STATUS_UNSUPPORTED;
    }
    return g_ops->deinit();
}

int battery_transport_is_connected(bool *connected_out)
{
    if (connected_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }
    if (g_ops == NULL || g_ops->is_connected == NULL) {
        return BATTERY_STATUS_UNSUPPORTED;
    }
    return g_ops->is_connected(connected_out);
}
