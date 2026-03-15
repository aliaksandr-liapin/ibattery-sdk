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
#define BATTERY_SERIALIZE_BUF_SIZE BATTERY_SERIALIZE_V2_SIZE

/**
 * Pack a telemetry packet into a wire buffer.
 *
 * Writes 24 bytes (v2 format) when version >= 2, 20 bytes for v1.
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
 * Accepts both 20-byte (v1) and 24-byte (v2) buffers.
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
 * @param version  Telemetry version (1 or 2).
 * @return Wire size in bytes (20 for v1, 24 for v2+).
 */
static inline uint8_t battery_serialize_wire_size(uint8_t version)
{
    return (version >= 2) ? BATTERY_SERIALIZE_V2_SIZE : BATTERY_SERIALIZE_V1_SIZE;
}

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SERIALIZE_H */
