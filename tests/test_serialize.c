/*
 * Unit tests for battery_serialize_pack() / battery_serialize_unpack().
 *
 * Pure logic tests — no mocks, no Zephyr, no platform dependencies.
 */

#include "unity.h"
#include <battery_sdk/battery_status.h>
#include <battery_sdk/battery_types.h>
#include "../src/transport/battery_serialize.h"
#include <string.h>

/* ── Helpers ──────────────────────────────────────────────────────── */

static struct battery_telemetry_packet make_sample_packet(void)
{
    struct battery_telemetry_packet pkt;
    pkt.telemetry_version   = 1;
    pkt.timestamp_ms        = 123456789;
    pkt.voltage_mv          = 2950;
    pkt.temperature_c_x100  = 2350;
    pkt.soc_pct_x100        = 8500;
    pkt.power_state         = BATTERY_POWER_STATE_ACTIVE;
    pkt.status_flags        = 0x00000000;
    return pkt;
}

void setUp(void) {}
void tearDown(void) {}

/* ── NULL / invalid arg tests ────────────────────────────────────── */

void test_pack_null_packet(void)
{
    uint8_t buf[20];
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_serialize_pack(NULL, buf, sizeof(buf)));
}

void test_pack_null_buffer(void)
{
    struct battery_telemetry_packet pkt = make_sample_packet();
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_serialize_pack(&pkt, NULL, 20));
}

void test_pack_buffer_too_small(void)
{
    struct battery_telemetry_packet pkt = make_sample_packet();
    uint8_t buf[19];
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_serialize_pack(&pkt, buf, sizeof(buf)));
}

void test_unpack_null_buffer(void)
{
    struct battery_telemetry_packet pkt;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_serialize_unpack(NULL, 20, &pkt));
}

void test_unpack_null_packet(void)
{
    uint8_t buf[20] = {0};
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_serialize_unpack(buf, sizeof(buf), NULL));
}

void test_unpack_buffer_too_small(void)
{
    uint8_t buf[19] = {0};
    struct battery_telemetry_packet pkt;
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_serialize_unpack(buf, sizeof(buf), &pkt));
}

/* ── Round-trip tests ────────────────────────────────────────────── */

void test_roundtrip_happy_path(void)
{
    struct battery_telemetry_packet src = make_sample_packet();
    struct battery_telemetry_packet dst;
    uint8_t buf[20];

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_serialize_pack(&src, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_serialize_unpack(buf, sizeof(buf), &dst));

    TEST_ASSERT_EQUAL_UINT8(src.telemetry_version, dst.telemetry_version);
    TEST_ASSERT_EQUAL_UINT32(src.timestamp_ms, dst.timestamp_ms);
    TEST_ASSERT_EQUAL_INT32(src.voltage_mv, dst.voltage_mv);
    TEST_ASSERT_EQUAL_INT32(src.temperature_c_x100, dst.temperature_c_x100);
    TEST_ASSERT_EQUAL_UINT16(src.soc_pct_x100, dst.soc_pct_x100);
    TEST_ASSERT_EQUAL_UINT8(src.power_state, dst.power_state);
    TEST_ASSERT_EQUAL_UINT32(src.status_flags, dst.status_flags);
}

void test_roundtrip_zero_values(void)
{
    struct battery_telemetry_packet src;
    struct battery_telemetry_packet dst;
    uint8_t buf[20];

    memset(&src, 0, sizeof(src));

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_serialize_pack(&src, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_serialize_unpack(buf, sizeof(buf), &dst));

    TEST_ASSERT_EQUAL_UINT8(0, dst.telemetry_version);
    TEST_ASSERT_EQUAL_UINT32(0, dst.timestamp_ms);
    TEST_ASSERT_EQUAL_INT32(0, dst.voltage_mv);
    TEST_ASSERT_EQUAL_INT32(0, dst.temperature_c_x100);
    TEST_ASSERT_EQUAL_UINT16(0, dst.soc_pct_x100);
    TEST_ASSERT_EQUAL_UINT8(0, dst.power_state);
    TEST_ASSERT_EQUAL_UINT32(0, dst.status_flags);
}

void test_roundtrip_max_values(void)
{
    struct battery_telemetry_packet src;
    struct battery_telemetry_packet dst;
    uint8_t buf[20];

    src.telemetry_version   = 255;
    src.timestamp_ms        = UINT32_MAX;
    src.voltage_mv          = INT32_MAX;
    src.temperature_c_x100  = INT32_MAX;
    src.soc_pct_x100        = UINT16_MAX;
    src.power_state         = 255;
    src.status_flags        = UINT32_MAX;

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_serialize_pack(&src, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_serialize_unpack(buf, sizeof(buf), &dst));

    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, dst.timestamp_ms);
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, dst.voltage_mv);
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, dst.temperature_c_x100);
    TEST_ASSERT_EQUAL_UINT16(UINT16_MAX, dst.soc_pct_x100);
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, dst.status_flags);
}

void test_roundtrip_negative_temperature(void)
{
    struct battery_telemetry_packet src = make_sample_packet();
    struct battery_telemetry_packet dst;
    uint8_t buf[20];

    src.temperature_c_x100 = -1050;  /* -10.50 °C */

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_serialize_pack(&src, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_serialize_unpack(buf, sizeof(buf), &dst));

    TEST_ASSERT_EQUAL_INT32(-1050, dst.temperature_c_x100);
}

void test_roundtrip_negative_voltage(void)
{
    struct battery_telemetry_packet src = make_sample_packet();
    struct battery_telemetry_packet dst;
    uint8_t buf[20];

    src.voltage_mv = -100;

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_serialize_pack(&src, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_serialize_unpack(buf, sizeof(buf), &dst));

    TEST_ASSERT_EQUAL_INT32(-100, dst.voltage_mv);
}

/* ── Wire format verification ────────────────────────────────────── */

void test_wire_format_exact_bytes(void)
{
    struct battery_telemetry_packet src;
    uint8_t buf[20];

    src.telemetry_version   = 0x01;
    src.timestamp_ms        = 0x04030201;
    src.voltage_mv          = 0x08070605;
    src.temperature_c_x100  = 0x0C0B0A09;
    src.soc_pct_x100        = 0x0E0D;
    src.power_state         = 0x0F;
    src.status_flags        = 0x13121110;

    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_serialize_pack(&src, buf, sizeof(buf)));

    /* Verify little-endian byte layout */
    TEST_ASSERT_EQUAL_HEX8(0x01, buf[0]);   /* version */

    TEST_ASSERT_EQUAL_HEX8(0x01, buf[1]);   /* timestamp LE */
    TEST_ASSERT_EQUAL_HEX8(0x02, buf[2]);
    TEST_ASSERT_EQUAL_HEX8(0x03, buf[3]);
    TEST_ASSERT_EQUAL_HEX8(0x04, buf[4]);

    TEST_ASSERT_EQUAL_HEX8(0x05, buf[5]);   /* voltage LE */
    TEST_ASSERT_EQUAL_HEX8(0x06, buf[6]);
    TEST_ASSERT_EQUAL_HEX8(0x07, buf[7]);
    TEST_ASSERT_EQUAL_HEX8(0x08, buf[8]);

    TEST_ASSERT_EQUAL_HEX8(0x09, buf[9]);   /* temperature LE */
    TEST_ASSERT_EQUAL_HEX8(0x0A, buf[10]);
    TEST_ASSERT_EQUAL_HEX8(0x0B, buf[11]);
    TEST_ASSERT_EQUAL_HEX8(0x0C, buf[12]);

    TEST_ASSERT_EQUAL_HEX8(0x0D, buf[13]);  /* soc LE */
    TEST_ASSERT_EQUAL_HEX8(0x0E, buf[14]);

    TEST_ASSERT_EQUAL_HEX8(0x0F, buf[15]);  /* power_state */

    TEST_ASSERT_EQUAL_HEX8(0x10, buf[16]);  /* status_flags LE */
    TEST_ASSERT_EQUAL_HEX8(0x11, buf[17]);
    TEST_ASSERT_EQUAL_HEX8(0x12, buf[18]);
    TEST_ASSERT_EQUAL_HEX8(0x13, buf[19]);
}

void test_roundtrip_all_power_states(void)
{
    uint8_t states[] = {
        BATTERY_POWER_STATE_UNKNOWN,
        BATTERY_POWER_STATE_ACTIVE,
        BATTERY_POWER_STATE_IDLE,
        BATTERY_POWER_STATE_SLEEP,
        BATTERY_POWER_STATE_CRITICAL
    };

    for (int i = 0; i < (int)(sizeof(states) / sizeof(states[0])); i++) {
        struct battery_telemetry_packet src = make_sample_packet();
        struct battery_telemetry_packet dst;
        uint8_t buf[20];

        src.power_state = states[i];
        battery_serialize_pack(&src, buf, sizeof(buf));
        battery_serialize_unpack(buf, sizeof(buf), &dst);

        TEST_ASSERT_EQUAL_UINT8(states[i], dst.power_state);
    }
}

void test_roundtrip_individual_flag_bits(void)
{
    uint32_t flags[] = {
        BATTERY_TELEMETRY_FLAG_VOLTAGE_ERR,
        BATTERY_TELEMETRY_FLAG_TEMP_ERR,
        BATTERY_TELEMETRY_FLAG_SOC_ERR,
        BATTERY_TELEMETRY_FLAG_POWER_STATE_ERR,
        BATTERY_TELEMETRY_FLAG_TIMESTAMP_ERR,
        BATTERY_TELEMETRY_FLAG_VOLTAGE_ERR | BATTERY_TELEMETRY_FLAG_TEMP_ERR,
        0x1F  /* all 5 flags set */
    };

    for (int i = 0; i < (int)(sizeof(flags) / sizeof(flags[0])); i++) {
        struct battery_telemetry_packet src = make_sample_packet();
        struct battery_telemetry_packet dst;
        uint8_t buf[20];

        src.status_flags = flags[i];
        battery_serialize_pack(&src, buf, sizeof(buf));
        battery_serialize_unpack(buf, sizeof(buf), &dst);

        TEST_ASSERT_EQUAL_UINT32(flags[i], dst.status_flags);
    }
}

void test_pack_exact_20_byte_buffer(void)
{
    struct battery_telemetry_packet pkt = make_sample_packet();
    uint8_t buf[20];

    /* Exactly 20 bytes should succeed */
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_serialize_pack(&pkt, buf, 20));
}

/* ── Test runner ─────────────────────────────────────────────────── */

int main(void)
{
    UNITY_BEGIN();

    /* NULL / invalid arg */
    RUN_TEST(test_pack_null_packet);
    RUN_TEST(test_pack_null_buffer);
    RUN_TEST(test_pack_buffer_too_small);
    RUN_TEST(test_unpack_null_buffer);
    RUN_TEST(test_unpack_null_packet);
    RUN_TEST(test_unpack_buffer_too_small);

    /* Round-trip */
    RUN_TEST(test_roundtrip_happy_path);
    RUN_TEST(test_roundtrip_zero_values);
    RUN_TEST(test_roundtrip_max_values);
    RUN_TEST(test_roundtrip_negative_temperature);
    RUN_TEST(test_roundtrip_negative_voltage);

    /* Wire format */
    RUN_TEST(test_wire_format_exact_bytes);
    RUN_TEST(test_roundtrip_all_power_states);
    RUN_TEST(test_roundtrip_individual_flag_bits);
    RUN_TEST(test_pack_exact_20_byte_buffer);

    return UNITY_END();
}
