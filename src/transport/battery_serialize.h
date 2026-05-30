/*
 * Telemetry packet serialization — pack/unpack to wire format.
 *
 * Wire format v1 (20 bytes, little-endian):
 *
 * Offset  Size  Field
 *   0      1    telemetry_version   (uint8)
 *   1      4    timestamp_ms        (uint32 LE)
 *   5      4    voltage_mv          (int32  LE)
 *   9      4    temperature_c_x100  (int32  LE)
 *  13      2    soc_pct_x100        (uint16 LE)
 *  15      1    power_state         (uint8)
 *  16      4    status_flags        (uint32 LE)
 *  ──     20    Total
 *
 * Wire format v2 (24 bytes, little-endian):
 *
 *   0-19         Same as v1
 *  20      4    cycle_count         (uint32 LE)
 *  ──     24    Total
 *
 * Wire format v3 (32 bytes, little-endian):
 *
 *   0-23         Same as v2
 *  24      4    current_ma_x100     (int32  LE)
 *  28      4    coulomb_mah_x100    (int32  LE)
 *  ──     32    Total
 *
 * Wire format v4 (34 bytes, little-endian):
 *
 *   0-31         Same as v3
 *  32      2    soh_pct_x100        (uint16 LE)
 *  ──     34    Total
 *
 * Versions are a superset ladder: a vN buffer contains all fields of
 * v1..vN. The gateway auto-detects the version by buffer length.
 */

#ifndef BATTERY_SERIALIZE_H
#define BATTERY_SERIALIZE_H

#include <battery_sdk/battery_types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BATTERY_SERIALIZE_V1_SIZE 20
#define BATTERY_SERIALIZE_V2_SIZE 24
#define BATTERY_SERIALIZE_V3_SIZE 32
#define BATTERY_SERIALIZE_V4_SIZE 34
#define BATTERY_SERIALIZE_BUF_SIZE BATTERY_SERIALIZE_V4_SIZE

/**
 * Pack a telemetry packet into a wire buffer.
 *
 * Writes the number of bytes for pkt->telemetry_version: 20 (v1), 24 (v2),
 * 32 (v3), or 34 (v4). See battery_serialize_wire_size().
 *
 * @param pkt      Source packet (must not be NULL)
 * @param buf      Destination buffer (must not be NULL, >= BATTERY_SERIALIZE_BUF_SIZE)
 * @param buf_len  Size of buf in bytes
 * @return BATTERY_STATUS_OK or BATTERY_STATUS_INVALID_ARG
 */
int battery_serialize_pack(const struct battery_telemetry_packet *pkt,
                           uint8_t *buf, uint8_t buf_len);

/**
 * Unpack a wire buffer into a telemetry packet.
 *
 * Accepts 20-byte (v1), 24-byte (v2), 32-byte (v3), and 34-byte (v4)
 * buffers. Fields beyond the buffer's length are set to 0.
 *
 * @param buf      Source buffer (must not be NULL, >= 20 bytes)
 * @param buf_len  Size of buf in bytes
 * @param pkt      Destination packet (must not be NULL)
 * @return BATTERY_STATUS_OK or BATTERY_STATUS_INVALID_ARG
 */
int battery_serialize_unpack(const uint8_t *buf, uint8_t buf_len,
                             struct battery_telemetry_packet *pkt);

/**
 * Get the wire size for the given packet version.
 *
 * @param version  Telemetry version (1..4; higher clamps to the latest).
 * @return Wire size in bytes: 20 (v1), 24 (v2), 32 (v3), 34 (v4+).
 */
static inline uint8_t battery_serialize_wire_size(uint8_t version)
{
    if (version >= 4) return BATTERY_SERIALIZE_V4_SIZE;
    if (version >= 3) return BATTERY_SERIALIZE_V3_SIZE;
    if (version >= 2) return BATTERY_SERIALIZE_V2_SIZE;
    return BATTERY_SERIALIZE_V1_SIZE;
}

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SERIALIZE_H */
