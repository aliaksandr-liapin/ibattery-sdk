/*
 * Mock transport backend for unit testing.
 *
 * Provides controllable return codes and a capture buffer so tests
 * can inspect the exact bytes passed to the backend send function.
 */

#include <battery_sdk/battery_transport.h>
#include <battery_sdk/battery_status.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ── Controllable state ───────────────────────────────────────────── */

static int     g_mock_init_rc   = BATTERY_STATUS_OK;
static int     g_mock_send_rc   = BATTERY_STATUS_OK;
static int     g_mock_deinit_rc = BATTERY_STATUS_OK;
static bool    g_mock_connected = false;

static uint8_t g_mock_send_buf[BATTERY_TRANSPORT_WIRE_SIZE];
static uint8_t g_mock_send_len  = 0;
static int     g_mock_send_count = 0;

/* ── Control functions (called by tests) ──────────────────────────── */

void mock_transport_set_init_rc(int rc)            { g_mock_init_rc = rc; }
void mock_transport_set_send_rc(int rc)            { g_mock_send_rc = rc; }
void mock_transport_set_deinit_rc(int rc)          { g_mock_deinit_rc = rc; }
void mock_transport_set_connected(bool connected)  { g_mock_connected = connected; }

const uint8_t *mock_transport_get_last_send_buf(void)  { return g_mock_send_buf; }
uint8_t        mock_transport_get_last_send_len(void)   { return g_mock_send_len; }
int            mock_transport_get_send_count(void)       { return g_mock_send_count; }

void mock_transport_reset(void)
{
    g_mock_init_rc   = BATTERY_STATUS_OK;
    g_mock_send_rc   = BATTERY_STATUS_OK;
    g_mock_deinit_rc = BATTERY_STATUS_OK;
    g_mock_connected = false;
    g_mock_send_len  = 0;
    g_mock_send_count = 0;
    memset(g_mock_send_buf, 0, sizeof(g_mock_send_buf));
}

/* ── Backend ops implementation ───────────────────────────────────── */

static int mock_init(void)
{
    return g_mock_init_rc;
}

static int mock_send(const uint8_t *buf, uint8_t len)
{
    if (g_mock_send_rc != BATTERY_STATUS_OK) {
        return g_mock_send_rc;
    }
    if (len <= sizeof(g_mock_send_buf)) {
        memcpy(g_mock_send_buf, buf, len);
    }
    g_mock_send_len = len;
    g_mock_send_count++;
    return BATTERY_STATUS_OK;
}

static int mock_deinit(void)
{
    return g_mock_deinit_rc;
}

static int mock_is_connected(bool *connected_out)
{
    if (connected_out == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }
    *connected_out = g_mock_connected;
    return BATTERY_STATUS_OK;
}

/* Exported ops struct — referenced by battery_transport.c when
 * CONFIG_BATTERY_TRANSPORT_MOCK is defined. */
const struct battery_transport_ops battery_transport_mock_ops = {
    .init         = mock_init,
    .send         = mock_send,
    .deinit       = mock_deinit,
    .is_connected = mock_is_connected,
};
