"""Tests for the InfluxDB telemetry writer."""

from unittest.mock import MagicMock, patch

import pytest


@pytest.fixture
def mock_influx():
    """Patch InfluxDBClient and return mocks."""
    with patch("gateway.influxdb_writer.InfluxDBClient") as mock_cls:
        mock_client = MagicMock()
        mock_write_api = MagicMock()
        mock_client.write_api.return_value = mock_write_api
        mock_cls.return_value = mock_client
        yield {
            "client_cls": mock_cls,
            "client": mock_client,
            "write_api": mock_write_api,
        }


@pytest.fixture
def sample_decoded():
    """Sample decoded telemetry packet."""
    return {
        "version": 1,
        "timestamp_ms": 120000,
        "voltage_mv": 3012,
        "voltage_v": 3.012,
        "temperature_c": 25.41,
        "soc_pct": 87.5,
        "power_state": "ACTIVE",
        "power_state_raw": 1,
        "status_flags": 0,
        "received_at": "2026-03-10T12:00:00+00:00",
    }


class TestTelemetryWriter:
    """Tests for TelemetryWriter."""

    def test_init_creates_client(self, mock_influx):
        from gateway.influxdb_writer import TelemetryWriter

        writer = TelemetryWriter(
            url="http://test:8086",
            token="test-token",
            org="test-org",
            bucket="test-bucket",
        )

        mock_influx["client_cls"].assert_called_once_with(
            url="http://test:8086", token="test-token", org="test-org"
        )
        assert writer.bucket == "test-bucket"
        assert writer.org == "test-org"

    def test_write_calls_write_api(self, mock_influx, sample_decoded):
        from gateway.influxdb_writer import TelemetryWriter

        writer = TelemetryWriter()
        writer.write(sample_decoded, device_name="TestDevice")

        mock_influx["write_api"].write.assert_called_once()
        call_kwargs = mock_influx["write_api"].write.call_args
        assert call_kwargs.kwargs["bucket"] == "telemetry"

    def test_write_point_fields(self, mock_influx, sample_decoded):
        """Verify the Point contains expected fields."""
        from gateway.influxdb_writer import TelemetryWriter

        writer = TelemetryWriter()
        writer.write(sample_decoded)

        point = mock_influx["write_api"].write.call_args.kwargs["record"]
        line = point.to_line_protocol()

        assert "battery_telemetry" in line
        assert "voltage_v=3.012" in line
        assert "temperature_c=25.41" in line
        assert "soc_pct=87.5" in line
        assert "power_state=1i" in line
        assert "status_flags=0i" in line

    def test_write_includes_device_tag(self, mock_influx, sample_decoded):
        from gateway.influxdb_writer import TelemetryWriter

        writer = TelemetryWriter()
        writer.write(sample_decoded, device_name="MyDevice")

        point = mock_influx["write_api"].write.call_args.kwargs["record"]
        line = point.to_line_protocol()
        assert "device=MyDevice" in line

    def test_write_error_does_not_raise(self, mock_influx, sample_decoded):
        """Writer should log errors but not crash."""
        from gateway.influxdb_writer import TelemetryWriter

        mock_influx["write_api"].write.side_effect = Exception("connection refused")

        writer = TelemetryWriter()
        # Should not raise
        writer.write(sample_decoded)

    def test_close(self, mock_influx):
        from gateway.influxdb_writer import TelemetryWriter

        writer = TelemetryWriter()
        writer.close()

        mock_influx["client"].close.assert_called_once()

    def test_context_manager(self, mock_influx, sample_decoded):
        from gateway.influxdb_writer import TelemetryWriter

        with TelemetryWriter() as writer:
            writer.write(sample_decoded)

        mock_influx["client"].close.assert_called_once()
