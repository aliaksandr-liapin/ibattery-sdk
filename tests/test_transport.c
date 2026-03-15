/*
 * Unit tests for the transport abstraction layer.
 *
 * Uses mock_transport.c as the backend (CONFIG_BATTERY_TRANSPORT_MOCK).
 */

#include "unity.h"
#include <battery_sdk/battery_transport.h>
#include <battery_sdk/battery_status.h>
#include <battery_sdk/battery_types.h>
#include "../src/transport/battery_serialize.h"
#include <string.h>

/* Mock control functions */
extern void          mock_transport_reset(void);
extern void          mock_transport_set_init_rc(int rc);
extern void          mock_transport_set_send_rc(int rc);
extern void          mock_transport_set_deinit_rc(int rc);
extern void          mock_transport_set_connected(bool connected);
extern const uint8_t *mock_transport_get_last_send_buf(void);
extern uint8_t       mock_transport_get_last_send_len(void);
extern int           mock_transport_get_send_count(void);

/* ── Helpers ──────────────────────────────────────────────────────── */

static struct battery_telemetry_packet make_sample_packet(void)
{
    struct battery_telemetry_packet pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.telemetry_version   = BATTERY_TELEMETRY_VERSION;  /* v2 */
    pkt.timestamp_ms        = 5000;
    pkt.voltage_mv          = 2950;
    pkt.temperature_c_x100  = 2500;
    pkt.soc_pct_x100        = 8500;
    pkt.power_state         = BATTERY_POWER_STATE_ACTIVE;
    pkt.status_flags        = 0;
    pkt.cycle_count         = 7;
    return pkt;
}

void setUp(void)
{
    mock_transport_reset();
}

void tearDown(void) {}

/* ── Init tests ──────────────────────────────────────────────────── */

void test_init_calls_backend(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_transport_init());
}

void test_init_error_propagates(void)
{
    mock_transport_set_init_rc(BATTERY_STATUS_IO);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_IO, battery_transport_init());
}

/* ── Send tests ──────────────────────────────────────────────────── */

void test_send_null_packet(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_transport_send(NULL));
}

void test_send_happy_path(void)
{
    struct battery_telemetry_packet pkt = make_sample_packet();
    uint8_t expected_len = battery_serialize_wire_size(pkt.telemetry_version);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_transport_send(&pkt));
    TEST_ASSERT_EQUAL_UINT8(expected_len,
                            mock_transport_get_last_send_len());
    TEST_ASSERT_EQUAL_INT(1, mock_transport_get_send_count());
}

void test_send_captures_correct_wire_bytes(void)
{
    struct battery_telemetry_packet pkt = make_sample_packet();
    uint8_t expected[BATTERY_SERIALIZE_BUF_SIZE];
    uint8_t wire_len = battery_serialize_wire_size(pkt.telemetry_version);

    /* Compute expected wire bytes independently */
    battery_serialize_pack(&pkt, expected, sizeof(expected));

    battery_transport_send(&pkt);

    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected,
                                  mock_transport_get_last_send_buf(),
                                  wire_len);
}

void test_send_backend_error_propagates(void)
{
    struct battery_telemetry_packet pkt = make_sample_packet();
    mock_transport_set_send_rc(BATTERY_STATUS_IO);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_IO, battery_transport_send(&pkt));
}

/* ── Deinit tests ────────────────────────────────────────────────── */

void test_deinit_calls_backend(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK, battery_transport_deinit());
}

void test_deinit_error_propagates(void)
{
    mock_transport_set_deinit_rc(BATTERY_STATUS_IO);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_IO, battery_transport_deinit());
}

/* ── Connection query tests ──────────────────────────────────────── */

void test_is_connected_null_pointer(void)
{
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_transport_is_connected(NULL));
}

void test_is_connected_returns_false(void)
{
    bool connected = true;
    mock_transport_set_connected(false);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_transport_is_connected(&connected));
    TEST_ASSERT_FALSE(connected);
}

void test_is_connected_returns_true(void)
{
    bool connected = false;
    mock_transport_set_connected(true);
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_transport_is_connected(&connected));
    TEST_ASSERT_TRUE(connected);
}

/* ── Test runner ─────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_calls_backend);
    RUN_TEST(test_init_error_propagates);

    RUN_TEST(test_send_null_packet);
    RUN_TEST(test_send_happy_path);
    RUN_TEST(test_send_captures_correct_wire_bytes);
    RUN_TEST(test_send_backend_error_propagates);

    RUN_TEST(test_deinit_calls_backend);
    RUN_TEST(test_deinit_error_propagates);

    RUN_TEST(test_is_connected_null_pointer);
    RUN_TEST(test_is_connected_returns_false);
    RUN_TEST(test_is_connected_returns_true);

    return UNITY_END();
}
