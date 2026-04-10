/*
 * BLE GATT transport backend for Zephyr.
 *
 * Implements a custom BLE service with a single notification
 * characteristic that carries 20-byte telemetry wire packets.
 *
 * Service UUID:        12340001-5678-9ABC-DEF0-123456789ABC
 * Characteristic UUID: 12340002-5678-9ABC-DEF0-123456789ABC
 *
 * The characteristic supports Read + Notify.  Telemetry is sent via
 * BLE notifications when a client subscribes (enables the CCCD).
 * If no client is subscribed, send() silently succeeds (drop policy).
 *
 * Requires CONFIG_BATTERY_TRANSPORT_BLE=y.
 */

#include <battery_sdk/battery_transport.h>
#include <battery_sdk/battery_status.h>

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#if __has_include(<zephyr/bluetooth/assigned_numbers.h>)
#include <zephyr/bluetooth/assigned_numbers.h>
#endif

#include <string.h>

/* ── UUIDs ──────────────────────────────────────────────────────────── */

/* 12340001-5678-9ABC-DEF0-123456789ABC  (service) */
static struct bt_uuid_128 svc_uuid = BT_UUID_INIT_128(
    BT_UUID_128_ENCODE(0x12340001, 0x5678, 0x9ABC, 0xDEF0, 0x123456789ABC));

/* 12340002-5678-9ABC-DEF0-123456789ABC  (telemetry characteristic) */
static struct bt_uuid_128 chr_uuid = BT_UUID_INIT_128(
    BT_UUID_128_ENCODE(0x12340002, 0x5678, 0x9ABC, 0xDEF0, 0x123456789ABC));

/* ── File-scope state ───────────────────────────────────────────────── */

static uint8_t  g_wire_buf[BATTERY_TRANSPORT_WIRE_SIZE];
static uint8_t  g_wire_len = BATTERY_TRANSPORT_WIRE_SIZE_V1;
static bool     g_notify_enabled;
static struct bt_conn *g_conn;

/* Semaphore for synchronous bt_enable() */
K_SEM_DEFINE(bt_ready_sem, 0, 1);

/* ── GATT callbacks ─────────────────────────────────────────────────── */

static ssize_t on_read(struct bt_conn *conn,
                       const struct bt_gatt_attr *attr,
                       void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             g_wire_buf, g_wire_len);
}

static void on_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    g_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
}

/* ── GATT service definition (compile-time table) ───────────────────── */

BT_GATT_SERVICE_DEFINE(battery_telem_svc,
    BT_GATT_PRIMARY_SERVICE(&svc_uuid),
    BT_GATT_CHARACTERISTIC(&chr_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           on_read, NULL, NULL),
    BT_GATT_CCC(on_ccc_changed,
                 BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/* ── Advertising & scan response data ───────────────────────────────── */

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS,
                  BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL,
                  BT_UUID_128_ENCODE(0x12340001, 0x5678, 0x9ABC,
                                     0xDEF0, 0x123456789ABC)),
};

static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE,
            CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

/* ── Connection callbacks ───────────────────────────────────────────── */

static void on_connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        return;
    }
    g_conn = bt_conn_ref(conn);
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
    if (g_conn) {
        bt_conn_unref(g_conn);
        g_conn = NULL;
    }
    g_notify_enabled = false;

    /* Resume advertising after disconnect */
    struct bt_le_adv_param adv_param = BT_LE_ADV_PARAM_INIT(
        BT_LE_ADV_OPT_CONN,
        BT_GAP_ADV_SLOW_INT_MIN,
        BT_GAP_ADV_SLOW_INT_MAX,
        NULL);

    bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad),
                     sd, ARRAY_SIZE(sd));
}

BT_CONN_CB_DEFINE(conn_cbs) = {
    .connected    = on_connected,
    .disconnected = on_disconnected,
};

/* ── bt_enable() ready callback ─────────────────────────────────────── */

static void bt_ready_cb(int err)
{
    k_sem_give(&bt_ready_sem);
}

/* ── Backend ops ────────────────────────────────────────────────────── */

static int ble_init(void)
{
    int err;

    err = bt_enable(bt_ready_cb);
    if (err && err != -EALREADY) {
        return BATTERY_STATUS_IO;
    }

    /* Wait for BT stack ready (or skip if already enabled) */
    if (err != -EALREADY) {
        k_sem_take(&bt_ready_sem, K_FOREVER);
    }

    /* Start connectable advertising */
    struct bt_le_adv_param adv_param = BT_LE_ADV_PARAM_INIT(
        BT_LE_ADV_OPT_CONN,
        BT_GAP_ADV_SLOW_INT_MIN,
        BT_GAP_ADV_SLOW_INT_MAX,
        NULL);

    err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad),
                          sd, ARRAY_SIZE(sd));
    if (err && err != -EALREADY) {
        return BATTERY_STATUS_IO;
    }

    return BATTERY_STATUS_OK;
}

static int ble_send(const uint8_t *buf, uint8_t len)
{
    if (buf == NULL || len < BATTERY_TRANSPORT_WIRE_SIZE_V1 ||
        len > BATTERY_TRANSPORT_WIRE_SIZE) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    /* Update cached buffer and length (for read characteristic) */
    memcpy(g_wire_buf, buf, len);
    g_wire_len = len;

    /* Drop silently when no subscriber — this is normal */
    if (!g_notify_enabled || g_conn == NULL) {
        return BATTERY_STATUS_OK;
    }

    int err = bt_gatt_notify(g_conn,
                             &battery_telem_svc.attrs[1],
                             g_wire_buf,
                             g_wire_len);
    if (err) {
        return BATTERY_STATUS_IO;
    }

    return BATTERY_STATUS_OK;
}

static int ble_deinit(void)
{
    int err = bt_le_adv_stop();
    if (err && err != -EALREADY) {
        return BATTERY_STATUS_IO;
    }
    return BATTERY_STATUS_OK;
}

static int ble_is_connected(bool *connected_out)
{
    if (connected_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }
    *connected_out = (g_conn != NULL);
    return BATTERY_STATUS_OK;
}

/* ── Exported ops struct ────────────────────────────────────────────── */

const struct battery_transport_ops battery_transport_ble_ops = {
    .init         = ble_init,
    .send         = ble_send,
    .deinit       = ble_deinit,
    .is_connected = ble_is_connected,
};
