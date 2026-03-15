"""Tests for the iBattery telemetry packet decoder (v1 + v2)."""

import struct

import pytest

from gateway.decoder import (
    POWER_STATES,
    WIRE_SIZE_V1,
    WIRE_SIZE_V2,
    decode_packet,
    format_packet,
)


def _pack_v1(
    version=1,
    timestamp_ms=0,
    voltage_mv=3000,
    temperature_c_x100=2500,
    soc_pct_x100=5000,
    power_state=1,
    status_flags=0,
) -> bytes:
    """Build a 20-byte v1 wire packet."""
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


def _pack_v2(
    version=2,
    timestamp_ms=0,
    voltage_mv=3000,
    temperature_c_x100=2500,
    soc_pct_x100=5000,
    power_state=1,
    status_flags=0,
    cycle_count=0,
) -> bytes:
    """Build a 24-byte v2 wire packet."""
    return struct.pack(
        "<BIiiHBII",
        version,
        timestamp_ms,
        voltage_mv,
        temperature_c_x100,
        soc_pct_x100,
        power_state,
        status_flags,
        cycle_count,
    )


class TestDecodePacketV1:
    """Tests for v1 (20-byte) packets."""

    def test_basic_decode(self):
        data = _pack_v1(
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
        assert result["cycle_count"] == 0  # v1 defaults to 0
        assert "received_at" in result

    def test_mobile_app_packet(self):
        """Decode the packet captured from nRF Connect mobile app."""
        data = bytes.fromhex("01ABB31200C40B0000450C000010270100000000")
        result = decode_packet(data)

        assert result["version"] == 1
        assert result["timestamp_ms"] == 0x0012B3AB
        assert result["voltage_mv"] == 0x00000BC4
        assert result["voltage_v"] == pytest.approx(3.012)
        assert result["temperature_c"] == pytest.approx(31.41)
        assert result["soc_pct"] == pytest.approx(100.0)
        assert result["power_state"] == "ACTIVE"
        assert result["status_flags"] == 0
        assert result["cycle_count"] == 0

    def test_zero_values(self):
        data = _pack_v1(
            version=0, timestamp_ms=0, voltage_mv=0,
            temperature_c_x100=0, soc_pct_x100=0, power_state=0, status_flags=0,
        )
        result = decode_packet(data)
        assert result["version"] == 0
        assert result["voltage_v"] == 0.0
        assert result["temperature_c"] == 0.0
        assert result["soc_pct"] == 0.0
        assert result["power_state"] == "UNKNOWN"

    def test_max_values(self):
        data = _pack_v1(
            version=255, timestamp_ms=0xFFFFFFFF, voltage_mv=0x7FFFFFFF,
            temperature_c_x100=0x7FFFFFFF, soc_pct_x100=0xFFFF, power_state=4,
            status_flags=0xFFFFFFFF,
        )
        result = decode_packet(data)
        assert result["version"] == 255
        assert result["timestamp_ms"] == 0xFFFFFFFF
        assert result["soc_pct"] == pytest.approx(655.35)
        assert result["power_state"] == "CRITICAL"
        assert result["status_flags"] == 0xFFFFFFFF

    def test_negative_temperature(self):
        data = _pack_v1(temperature_c_x100=-1500)
        result = decode_packet(data)
        assert result["temperature_c"] == pytest.approx(-15.0)

    def test_negative_voltage(self):
        data = _pack_v1(voltage_mv=-100)
        result = decode_packet(data)
        assert result["voltage_mv"] == -100
        assert result["voltage_v"] == pytest.approx(-0.1)

    def test_all_power_states(self):
        for raw, name in POWER_STATES.items():
            data = _pack_v1(power_state=raw)
            result = decode_packet(data)
            assert result["power_state"] == name
            assert result["power_state_raw"] == raw

    def test_unknown_power_state(self):
        data = _pack_v1(power_state=99)
        result = decode_packet(data)
        assert result["power_state"] == "UNKNOWN(99)"

    def test_status_flags_bits(self):
        for bit in range(32):
            flags = 1 << bit
            data = _pack_v1(status_flags=flags)
            result = decode_packet(data)
            assert result["status_flags"] == flags


class TestDecodePacketV2:
    """Tests for v2 (24-byte) packets."""

    def test_basic_v2_decode(self):
        data = _pack_v2(
            version=2, timestamp_ms=500000, voltage_mv=4100,
            temperature_c_x100=2200, soc_pct_x100=9500, power_state=5,
            status_flags=0, cycle_count=42,
        )
        result = decode_packet(data)

        assert result["version"] == 2
        assert result["timestamp_ms"] == 500000
        assert result["voltage_v"] == pytest.approx(4.1)
        assert result["temperature_c"] == pytest.approx(22.0)
        assert result["soc_pct"] == pytest.approx(95.0)
        assert result["power_state"] == "CHARGING"
        assert result["cycle_count"] == 42

    def test_v2_zero_cycles(self):
        data = _pack_v2(cycle_count=0)
        result = decode_packet(data)
        assert result["cycle_count"] == 0

    def test_v2_max_cycles(self):
        data = _pack_v2(cycle_count=0xFFFFFFFF)
        result = decode_packet(data)
        assert result["cycle_count"] == 0xFFFFFFFF

    def test_v2_roundtrip_all_fields(self):
        data = _pack_v2(
            version=2, timestamp_ms=0xDEADBEEF, voltage_mv=3700,
            temperature_c_x100=-500, soc_pct_x100=5555, power_state=7,
            status_flags=0xCAFE1234, cycle_count=1000,
        )
        result = decode_packet(data)
        assert result["version"] == 2
        assert result["timestamp_ms"] == 0xDEADBEEF
        assert result["voltage_mv"] == 3700
        assert result["temperature_c"] == pytest.approx(-5.0)
        assert result["soc_pct"] == pytest.approx(55.55)
        assert result["power_state"] == "CHARGED"
        assert result["status_flags"] == 0xCAFE1234
        assert result["cycle_count"] == 1000

    def test_v2_wire_size(self):
        data = _pack_v2()
        assert len(data) == WIRE_SIZE_V2 == 24


class TestDecodeErrors:
    """Invalid input handling."""

    def test_short_buffer_raises(self):
        with pytest.raises(ValueError, match="Expected 20 or 24"):
            decode_packet(b"\x00" * 19)

    def test_21_bytes_raises(self):
        with pytest.raises(ValueError, match="Expected 20 or 24"):
            decode_packet(b"\x00" * 21)

    def test_25_bytes_raises(self):
        with pytest.raises(ValueError, match="Expected 20 or 24"):
            decode_packet(b"\x00" * 25)

    def test_empty_buffer_raises(self):
        with pytest.raises(ValueError, match="Expected 20 or 24"):
            decode_packet(b"")

    def test_v1_size_constant(self):
        assert WIRE_SIZE_V1 == 20

    def test_v2_size_constant(self):
        assert WIRE_SIZE_V2 == 24


class TestFormatPacket:
    """Tests for format_packet()."""

    def test_format_v1_basic(self):
        data = _pack_v1(
            version=1, timestamp_ms=5000, voltage_mv=3012,
            temperature_c_x100=2541, soc_pct_x100=8750, power_state=1, status_flags=0,
        )
        text = format_packet(decode_packet(data))
        assert "v1" in text
        assert "3.012V" in text
        assert "25.41" in text
        assert "87.5%" in text
        assert "ACTIVE" in text
        assert "ts=5000ms" in text

    def test_format_v2_shows_cycle_count(self):
        data = _pack_v2(version=2, voltage_mv=4100, cycle_count=42)
        text = format_packet(decode_packet(data))
        assert "v2" in text
        assert "cyc=42" in text

    def test_format_v2_zero_cycles_hidden(self):
        data = _pack_v2(version=2, cycle_count=0)
        text = format_packet(decode_packet(data))
        assert "cyc=" not in text
