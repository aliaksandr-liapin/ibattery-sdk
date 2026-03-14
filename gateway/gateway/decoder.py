"""Decode 20-byte LE wire packets from the iBattery BLE telemetry characteristic.

Wire format (matches firmware battery_serialize.c):

    Offset  Size  Field               Encoding
    ------  ----  ------------------  ----------
    0       1     telemetry_version   uint8
    1       4     timestamp_ms        uint32 LE
    5       4     voltage_mv          int32  LE
    9       4     temperature_c_x100  int32  LE
    13      2     soc_pct_x100        uint16 LE
    15      1     power_state         uint8
    16      4     status_flags        uint32 LE
    ------  ----
    Total: 20 bytes
"""

import struct
from datetime import datetime, timezone

WIRE_SIZE = 20

# struct format: <  = little-endian
#   B  = uint8   (version)
#   I  = uint32  (timestamp_ms)
#   i  = int32   (voltage_mv)
#   i  = int32   (temperature_c_x100)
#   H  = uint16  (soc_pct_x100)
#   B  = uint8   (power_state)
#   I  = uint32  (status_flags)
_WIRE_FMT = "<BIiiHBI"
_WIRE_STRUCT = struct.Struct(_WIRE_FMT)

POWER_STATES = {
    0: "BOOT",
    1: "ACTIVE",
    2: "LOW",
    3: "CRITICAL",
    4: "SHUTDOWN",
}


def decode_packet(data: bytes) -> dict:
    """Unpack a 20-byte LE wire buffer into a telemetry dictionary.

    Args:
        data: Exactly 20 bytes from the BLE notification.

    Returns:
        Dictionary with human-readable telemetry values.

    Raises:
        ValueError: If data length is not 20.
    """
    if len(data) != WIRE_SIZE:
        raise ValueError(f"Expected {WIRE_SIZE} bytes, got {len(data)}")

    version, ts_ms, mv, temp_x100, soc_x100, ps, flags = _WIRE_STRUCT.unpack(data)

    return {
        "version": version,
        "timestamp_ms": ts_ms,
        "voltage_mv": mv,
        "voltage_v": mv / 1000.0,
        "temperature_c": temp_x100 / 100.0,
        "soc_pct": soc_x100 / 100.0,
        "power_state": POWER_STATES.get(ps, f"UNKNOWN({ps})"),
        "power_state_raw": ps,
        "status_flags": flags,
        "received_at": datetime.now(timezone.utc).isoformat(),
    }


def format_packet(decoded: dict) -> str:
    """Format a decoded packet as a human-readable one-line string."""
    return (
        f"v{decoded['version']} | "
        f"{decoded['voltage_v']:.3f}V | "
        f"{decoded['temperature_c']:.2f}°C | "
        f"SoC {decoded['soc_pct']:.1f}% | "
        f"{decoded['power_state']} | "
        f"flags=0x{decoded['status_flags']:08X} | "
        f"ts={decoded['timestamp_ms']}ms"
    )
