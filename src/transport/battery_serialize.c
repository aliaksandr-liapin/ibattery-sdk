/*
 * Telemetry packet serialization.
 *
 * Encodes/decodes battery_telemetry_packet to/from a wire buffer
 * using explicit byte shifts.  No struct packing, no memcpy —
 * fully portable across compilers and platforms.
 *
 * v1: 20 bytes  (version == 1)
 * v2: 24 bytes  (version >= 2, adds cycle_count)
 * v3: 32 bytes  (version >= 3, adds current_ma_x100, coulomb_mah_x100)
 */

#include "battery_serialize.h"
#include <battery_sdk/battery_status.h>
#include <stddef.h>

/* ── Helpers ───────────────────────────────────────────────────────── */

static void put_u16_le(uint8_t *buf, uint16_t v)
{
    buf[0] = (uint8_t)(v);
    buf[1] = (uint8_t)(v >> 8);
}

static void put_u32_le(uint8_t *buf, uint32_t v)
{
    buf[0] = (uint8_t)(v);
    buf[1] = (uint8_t)(v >> 8);
    buf[2] = (uint8_t)(v >> 16);
    buf[3] = (uint8_t)(v >> 24);
}

static uint16_t get_u16_le(const uint8_t *buf)
{
    return (uint16_t)buf[0]
         | ((uint16_t)buf[1] << 8);
}

static uint32_t get_u32_le(const uint8_t *buf)
{
    return (uint32_t)buf[0]
         | ((uint32_t)buf[1] << 8)
         | ((uint32_t)buf[2] << 16)
         | ((uint32_t)buf[3] << 24);
}

/* ── Pack ──────────────────────────────────────────────────────────── */

int battery_serialize_pack(const struct battery_telemetry_packet *pkt,
                           uint8_t *buf, uint8_t buf_len)
{
    uint8_t required;

    if (pkt == NULL || buf == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    required = battery_serialize_wire_size(pkt->telemetry_version);
    if (buf_len < required) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    buf[0] = pkt->telemetry_version;                       /* offset  0 */
    put_u32_le(&buf[1],  pkt->timestamp_ms);               /* offset  1 */
    put_u32_le(&buf[5],  (uint32_t)pkt->voltage_mv);       /* offset  5 */
    put_u32_le(&buf[9],  (uint32_t)pkt->temperature_c_x100); /* offset 9 */
    put_u16_le(&buf[13], pkt->soc_pct_x100);               /* offset 13 */
    buf[15] = pkt->power_state;                             /* offset 15 */
    put_u32_le(&buf[16], pkt->status_flags);                /* offset 16 */

    /* v2 extension */
    if (pkt->telemetry_version >= 2) {
        put_u32_le(&buf[20], pkt->cycle_count);             /* offset 20 */
    }

    /* v3 extension */
    if (pkt->telemetry_version >= 3) {
        put_u32_le(&buf[24], (uint32_t)pkt->current_ma_x100);
        put_u32_le(&buf[28], (uint32_t)pkt->coulomb_mah_x100);
    }

    return BATTERY_STATUS_OK;
}

/* ── Unpack ────────────────────────────────────────────────────────── */

int battery_serialize_unpack(const uint8_t *buf, uint8_t buf_len,
                             struct battery_telemetry_packet *pkt)
{
    if (buf == NULL || pkt == NULL || buf_len < BATTERY_SERIALIZE_V1_SIZE) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    pkt->telemetry_version   = buf[0];
    pkt->timestamp_ms        = get_u32_le(&buf[1]);
    pkt->voltage_mv          = (int32_t)get_u32_le(&buf[5]);
    pkt->temperature_c_x100  = (int32_t)get_u32_le(&buf[9]);
    pkt->soc_pct_x100        = get_u16_le(&buf[13]);
    pkt->power_state         = buf[15];
    pkt->status_flags        = get_u32_le(&buf[16]);

    /* v2 extension */
    if (pkt->telemetry_version >= 2 && buf_len >= BATTERY_SERIALIZE_V2_SIZE) {
        pkt->cycle_count = get_u32_le(&buf[20]);
    } else {
        pkt->cycle_count = 0;
    }

    /* v3 extension */
    if (pkt->telemetry_version >= 3 && buf_len >= BATTERY_SERIALIZE_V3_SIZE) {
        pkt->current_ma_x100  = (int32_t)get_u32_le(&buf[24]);
        pkt->coulomb_mah_x100 = (int32_t)get_u32_le(&buf[28]);
    } else {
        pkt->current_ma_x100  = 0;
        pkt->coulomb_mah_x100 = 0;
    }

    return BATTERY_STATUS_OK;
}
