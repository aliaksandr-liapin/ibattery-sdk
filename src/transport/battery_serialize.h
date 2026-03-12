/*
 * Telemetry packet serialization — pack/unpack to 20-byte wire format.
 *
 * Wire format: little-endian, no padding, no struct packing.
 * 20 bytes fits within a single BLE ATT default MTU (23 − 3 = 20).
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
 */

#ifndef BATTERY_SERIALIZE_H
#define BATTERY_SERIALIZE_H

#include <battery_sdk/battery_types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BATTERY_SERIALIZE_BUF_SIZE 20

/**
 * Pack a telemetry packet into a 20-byte wire buffer.
 *
 * @param pkt      Source packet (must not be NULL)
 * @param buf      Destination buffer (must not be NULL, >= 20 bytes)
 * @param buf_len  Size of buf in bytes
 * @return BATTERY_STATUS_OK or BATTERY_STATUS_INVALID_ARG
 */
int battery_serialize_pack(const struct battery_telemetry_packet *pkt,
                           uint8_t *buf, uint8_t buf_len);

/**
 * Unpack a 20-byte wire buffer into a telemetry packet.
 *
 * @param buf      Source buffer (must not be NULL, >= 20 bytes)
 * @param buf_len  Size of buf in bytes
 * @param pkt      Destination packet (must not be NULL)
 * @return BATTERY_STATUS_OK or BATTERY_STATUS_INVALID_ARG
 */
int battery_serialize_unpack(const uint8_t *buf, uint8_t buf_len,
                             struct battery_telemetry_packet *pkt);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_SERIALIZE_H */
