"""Decode LE wire packets from the iBattery BLE telemetry characteristic.

Supports v1 (20-byte), v2 (24-byte), and v3 (32-byte) wire formats.

Wire format v1:

    Offset  Size  Field               Encoding
    ------  ----  ------------------  ----------
    0       1     telemetry_version   uint8
    1       4     timestamp_ms        uint32 LE
    5       4     voltage_mv          int32  LE
    9       4     temperature_c_x100  int32  LE
    13      2     soc_pct_x100        uint16 LE
    15      1     power_state         uint8
    16      4     status_flags        uint32 LE
    Total: 20 bytes

Wire format v2 (extends v1):

    20      4     cycle_count         uint32 LE
    Total: 24 bytes

Wire format v3 (extends v2):

    24      4     current_ma_x100     int32  LE (signed, /100 → mA)
    28      4     coulomb_mah_x100    int32  LE (signed, /100 → mAh)
    Total: 32 bytes
"""

import struct
from datetime import datetime, timezone

WIRE_SIZE_V1 = 20
WIRE_SIZE_V2 = 24
WIRE_SIZE_V3 = 32

# struct formats: < = little-endian
_WIRE_FMT_V1 = "<BIiiHBI"
_WIRE_FMT_V2 = "<BIiiHBII"  # adds cycle_count (uint32)
_WIRE_FMT_V3 = "<BIiiHBIIii"  # adds current_ma_x100, coulomb_mah_x100 (int32)
_WIRE_STRUCT_V1 = struct.Struct(_WIRE_FMT_V1)
_WIRE_STRUCT_V2 = struct.Struct(_WIRE_FMT_V2)
_WIRE_STRUCT_V3 = struct.Struct(_WIRE_FMT_V3)

POWER_STATES = {
    0: "UNKNOWN",
    1: "ACTIVE",
    2: "IDLE",
    3: "SLEEP",
    4: "CRITICAL",
    5: "CHARGING",
    6: "DISCHARGING",
    7: "CHARGED",
}


def decode_packet(data: bytes) -> dict:
    """Unpack a v1/v2/v3 wire buffer into a telemetry dict.

    Args:
        data: 20, 24, or 32 bytes from the BLE notification.

    Returns:
        Dictionary with human-readable telemetry values.

    Raises:
        ValueError: If data length is not 20, 24, or 32.
    """
    if len(data) == WIRE_SIZE_V3:
        (version, ts_ms, mv, temp_x100, soc_x100, ps, flags,
         cycles, current_x100, coulomb_x100) = _WIRE_STRUCT_V3.unpack(data)
    elif len(data) == WIRE_SIZE_V2:
        version, ts_ms, mv, temp_x100, soc_x100, ps, flags, cycles = _WIRE_STRUCT_V2.unpack(data)
        current_x100 = 0
        coulomb_x100 = 0
    elif len(data) == WIRE_SIZE_V1:
        version, ts_ms, mv, temp_x100, soc_x100, ps, flags = _WIRE_STRUCT_V1.unpack(data)
        cycles = 0
        current_x100 = 0
        coulomb_x100 = 0
    else:
        raise ValueError(
            f"Expected {WIRE_SIZE_V1}, {WIRE_SIZE_V2}, or {WIRE_SIZE_V3} bytes, got {len(data)}"
        )

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
        "cycle_count": cycles,
        "current_ma": current_x100 / 100.0,
        "coulomb_mah": coulomb_x100 / 100.0,
        "received_at": datetime.now(timezone.utc).isoformat(),
    }


def format_packet(decoded: dict) -> str:
    """Format a decoded packet as a human-readable one-line string."""
    cycle_str = f" | cyc={decoded['cycle_count']}" if decoded.get("cycle_count", 0) > 0 else ""
    current_ma = decoded.get("current_ma", 0.0)
    coulomb_mah = decoded.get("coulomb_mah", 0.0)
    current_str = f" | {current_ma:+.2f}mA {coulomb_mah:.1f}mAh" if current_ma != 0.0 or coulomb_mah != 0.0 else ""
    return (
        f"v{decoded['version']} | "
        f"{decoded['voltage_v']:.3f}V | "
        f"{decoded['temperature_c']:.2f}\u00b0C | "
        f"SoC {decoded['soc_pct']:.1f}% | "
        f"{decoded['power_state']} | "
        f"flags=0x{decoded['status_flags']:08X}{cycle_str}{current_str} | "
        f"ts={decoded['timestamp_ms']}ms"
    )
