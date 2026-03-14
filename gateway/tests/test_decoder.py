"""Tests for the iBattery telemetry packet decoder."""

import struct

import pytest

from gateway.decoder import POWER_STATES, WIRE_SIZE, decode_packet, format_packet


def _pack_packet(
    version=1,
    timestamp_ms=0,
    voltage_mv=3000,
    temperature_c_x100=2500,
    soc_pct_x100=5000,
    power_state=1,
    status_flags=0,
) -> bytes:
    """Helper: build a 20-byte wire packet from field values."""
    return struct.pack(
        "<BIiiHBI",
        version,
        timestamp_ms,
        voltage_mv,
        temperature_c_x100,
        soc_pct_x100,
        power_state,
        status_flags,
    )


class TestDecodePacket:
    """Tests for decode_packet()."""

    def test_basic_decode(self):
        data = _pack_packet(
            version=1,
            timestamp_ms=120000,
            voltage_mv=3012,
            temperature_c_x100=2541,
            soc_pct_x100=8750,
            power_state=1,
            status_flags=0,
        )
        result = decode_packet(data)

        assert result["version"] == 1
        assert result["timestamp_ms"] == 120000
        assert result["voltage_mv"] == 3012
        assert result["voltage_v"] == pytest.approx(3.012)
        assert result["temperature_c"] == pytest.approx(25.41)
        assert result["soc_pct"] == pytest.approx(87.50)
        assert result["power_state"] == "ACTIVE"
        assert result["power_state_raw"] == 1
        assert result["status_flags"] == 0
        assert "received_at" in result

    def test_mobile_app_packet(self):
        """Decode the packet captured from nRF Connect mobile app."""
        # 01 AB B3 12 00 C4 0B 00 00 45 0C 00 00 10 27 01 00 00 00 00
        data = bytes.fromhex("01ABB31200C40B0000450C000010270100000000")
        result = decode_packet(data)

        assert result["version"] == 1
        assert result["timestamp_ms"] == 0x0012B3AB  # ~1225643 ms
        assert result["voltage_mv"] == 0x00000BC4     # 3012 mV
        assert result["voltage_v"] == pytest.approx(3.012)
        assert result["temperature_c"] == pytest.approx(31.41)  # 0x00000C45 = 3141
        assert result["soc_pct"] == pytest.approx(100.0)        # 0x2710 = 10000
        assert result["power_state"] == "ACTIVE"
        assert result["status_flags"] == 0

    def test_zero_values(self):
        data = _pack_packet(
            version=0,
            timestamp_ms=0,
            voltage_mv=0,
            temperature_c_x100=0,
            soc_pct_x100=0,
            power_state=0,
            status_flags=0,
        )
        result = decode_packet(data)

        assert result["version"] == 0
        assert result["voltage_v"] == 0.0
        assert result["temperature_c"] == 0.0
        assert result["soc_pct"] == 0.0
        assert result["power_state"] == "BOOT"

    def test_max_values(self):
        data = _pack_packet(
            version=255,
            timestamp_ms=0xFFFFFFFF,
            voltage_mv=0x7FFFFFFF,
            temperature_c_x100=0x7FFFFFFF,
            soc_pct_x100=0xFFFF,
            power_state=4,
            status_flags=0xFFFFFFFF,
        )
        result = decode_packet(data)

        assert result["version"] == 255
        assert result["timestamp_ms"] == 0xFFFFFFFF
        assert result["soc_pct"] == pytest.approx(655.35)
        assert result["power_state"] == "SHUTDOWN"
        assert result["status_flags"] == 0xFFFFFFFF

    def test_negative_temperature(self):
        data = _pack_packet(temperature_c_x100=-1500)
        result = decode_packet(data)
        assert result["temperature_c"] == pytest.approx(-15.0)

    def test_negative_voltage(self):
        """Negative voltage (error/miscalibration) should still decode."""
        data = _pack_packet(voltage_mv=-100)
        result = decode_packet(data)
        assert result["voltage_mv"] == -100
        assert result["voltage_v"] == pytest.approx(-0.1)

    def test_all_power_states(self):
        for raw, name in POWER_STATES.items():
            data = _pack_packet(power_state=raw)
            result = decode_packet(data)
            assert result["power_state"] == name
            assert result["power_state_raw"] == raw

    def test_unknown_power_state(self):
        data = _pack_packet(power_state=99)
        result = decode_packet(data)
        assert result["power_state"] == "UNKNOWN(99)"

    def test_short_buffer_raises(self):
        with pytest.raises(ValueError, match="Expected 20 bytes"):
            decode_packet(b"\x00" * 19)

    def test_long_buffer_raises(self):
        with pytest.raises(ValueError, match="Expected 20 bytes"):
            decode_packet(b"\x00" * 21)

    def test_empty_buffer_raises(self):
        with pytest.raises(ValueError, match="Expected 20 bytes"):
            decode_packet(b"")

    def test_status_flags_bits(self):
        """Individual flag bits survive roundtrip."""
        for bit in range(32):
            flags = 1 << bit
            data = _pack_packet(status_flags=flags)
            result = decode_packet(data)
            assert result["status_flags"] == flags


class TestFormatPacket:
    """Tests for format_packet()."""

    def test_format_basic(self):
        data = _pack_packet(
            version=1,
            timestamp_ms=5000,
            voltage_mv=3012,
            temperature_c_x100=2541,
            soc_pct_x100=8750,
            power_state=1,
            status_flags=0,
        )
        decoded = decode_packet(data)
        text = format_packet(decoded)

        assert "v1" in text
        assert "3.012V" in text
        assert "25.41" in text
        assert "87.5%" in text
        assert "ACTIVE" in text
        assert "0x00000000" in text
        assert "ts=5000ms" in text
