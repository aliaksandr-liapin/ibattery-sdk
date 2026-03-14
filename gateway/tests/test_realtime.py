"""Tests for real-time anomaly detection on decoded packets."""

from gateway.analytics.realtime import check_realtime


def _make_packet(**overrides):
    """Create a default normal packet, with optional overrides."""
    pkt = {
        "version": 1,
        "timestamp_ms": 10000,
        "voltage_mv": 3800,
        "voltage_v": 3.800,
        "temperature_c": 25.0,
        "soc_pct": 80.0,
        "power_state": "ACTIVE",
        "power_state_raw": 1,
        "status_flags": 0,
        "received_at": "2026-03-14T00:00:00+00:00",
    }
    pkt.update(overrides)
    return pkt


class TestRealtimeNormal:
    """Normal packets should produce no anomalies."""

    def test_normal_packet_no_anomalies(self):
        result = check_realtime(_make_packet())
        assert result == []

    def test_high_voltage_no_anomaly(self):
        result = check_realtime(_make_packet(voltage_v=4.2))
        assert result == []

    def test_room_temp_no_anomaly(self):
        result = check_realtime(_make_packet(temperature_c=25.0))
        assert result == []


class TestRealtimeVoltageAnomalies:
    """Voltage-related anomaly checks."""

    def test_critical_voltage(self):
        result = check_realtime(_make_packet(voltage_v=2.8))
        assert len(result) == 1
        assert result[0]["type"] == "voltage_critical"
        assert result[0]["severity"] == "critical"

    def test_low_voltage_high_soc_inconsistency(self):
        result = check_realtime(_make_packet(voltage_v=3.1, soc_pct=50.0))
        assert len(result) == 1
        assert result[0]["type"] == "soc_inconsistency"
        assert result[0]["severity"] == "warning"

    def test_low_voltage_low_soc_no_inconsistency(self):
        """Low voltage with low SoC is expected, not anomalous."""
        result = check_realtime(_make_packet(voltage_v=3.1, soc_pct=10.0))
        assert result == []

    def test_borderline_voltage_3_0_is_critical(self):
        result = check_realtime(_make_packet(voltage_v=2.999))
        types = [a["type"] for a in result]
        assert "voltage_critical" in types


class TestRealtimeTemperatureAnomalies:
    """Temperature-related anomaly checks."""

    def test_high_temperature(self):
        result = check_realtime(_make_packet(temperature_c=50.0))
        assert len(result) == 1
        assert result[0]["type"] == "temp_high"
        assert result[0]["severity"] == "warning"

    def test_low_temperature_lipo_danger(self):
        result = check_realtime(_make_packet(temperature_c=-5.0))
        assert len(result) == 1
        assert result[0]["type"] == "temp_low"
        assert result[0]["severity"] == "critical"

    def test_zero_degrees_no_anomaly(self):
        """Exactly 0 C is at the threshold — not anomalous."""
        result = check_realtime(_make_packet(temperature_c=0.0))
        assert result == []

    def test_exactly_45_no_anomaly(self):
        """Exactly 45 C is at the threshold — not anomalous."""
        result = check_realtime(_make_packet(temperature_c=45.0))
        assert result == []


class TestRealtimeMultipleAnomalies:
    """Multiple anomalies in a single packet."""

    def test_critical_voltage_and_high_temp(self):
        result = check_realtime(_make_packet(voltage_v=2.5, temperature_c=50.0))
        types = {a["type"] for a in result}
        assert "voltage_critical" in types
        assert "temp_high" in types
        assert len(result) == 2
