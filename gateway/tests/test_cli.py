"""Tests for the ibattery-gateway CLI (Click commands)."""

import logging
from unittest.mock import AsyncMock, MagicMock, patch

from click.testing import CliRunner

from gateway.cli import main

runner = CliRunner()


def _ctx_instance(mock_cls):
    """The object yielded by `with SomeClass(...) as x:` for a patched class."""
    return mock_cls.return_value.__enter__.return_value


class TestMainGroup:
    def test_help_lists_commands(self):
        result = runner.invoke(main, ["--help"])
        assert result.exit_code == 0
        for cmd in ("scan", "stream", "run", "analytics"):
            assert cmd in result.output

    def test_debug_flag_sets_debug_level(self):
        # Use a leaf command that does no I/O when its dep returns nothing.
        with patch("gateway.analytics.health_score.BatteryHealthScorer") as cls:
            _ctx_instance(cls).compute_health_score.return_value = None
            result = runner.invoke(main, ["--debug", "analytics", "health"])
        assert result.exit_code == 0
        assert logging.getLogger().level == logging.DEBUG


class TestAnalyticsHealth:
    @patch("gateway.analytics.health_score.BatteryHealthScorer")
    def test_insufficient_data(self, cls):
        _ctx_instance(cls).compute_health_score.return_value = None
        result = runner.invoke(main, ["analytics", "health"])
        assert result.exit_code == 0
        assert "Insufficient data" in result.output

    @patch("gateway.analytics.health_score.BatteryHealthScorer")
    def test_reports_score(self, cls):
        _ctx_instance(cls).compute_health_score.return_value = {
            "score": 92, "trend": "stable", "baseline_v": 3.80, "current_v": 3.79,
            "variance_ratio": 1.02, "data_points": 120, "computed_at": "2026-01-01T00:00:00Z",
        }
        result = runner.invoke(main, ["analytics", "health"])
        assert result.exit_code == 0
        assert "Battery Health Report" in result.output
        assert "92" in result.output


class TestAnalyticsAnomalies:
    @patch("gateway.analytics.anomaly_detector.AnomalyDetector")
    def test_no_anomalies(self, cls):
        inst = _ctx_instance(cls)
        inst.detect_voltage_anomalies.return_value = []
        inst.detect_temperature_anomalies.return_value = []
        result = runner.invoke(main, ["analytics", "anomalies"])
        assert result.exit_code == 0
        assert "No anomalies detected" in result.output

    @patch("gateway.analytics.anomaly_detector.AnomalyDetector")
    def test_lists_anomalies(self, cls):
        inst = _ctx_instance(cls)
        inst.detect_voltage_anomalies.return_value = [
            {"severity": "warning", "timestamp": "t1", "message": "Sudden voltage drop"},
        ]
        inst.detect_temperature_anomalies.return_value = [
            {"severity": "critical", "timestamp": "t2", "message": "Temperature below 0C"},
        ]
        result = runner.invoke(main, ["analytics", "anomalies"])
        assert result.exit_code == 0
        assert "Found 2 anomalie(s)" in result.output
        assert "Sudden voltage drop" in result.output


class TestAnalyticsRul:
    @patch("gateway.analytics.rul_estimator.RULEstimator")
    def test_insufficient_data(self, cls):
        _ctx_instance(cls).estimate_rul.return_value = None
        result = runner.invoke(main, ["analytics", "rul"])
        assert result.exit_code == 0
        assert "Insufficient data" in result.output

    @patch("gateway.analytics.rul_estimator.RULEstimator")
    def test_reports_rul(self, cls):
        _ctx_instance(cls).estimate_rul.return_value = {
            "status": "degrading", "current_health": 88, "current_cycles": 40,
            "eol_threshold": 80, "remaining_cycles": 120, "cycles_at_eol": 160,
            "slope_mv_per_cycle": -1.5, "data_points": 50, "computed_at": "2026-01-01T00:00:00Z",
        }
        result = runner.invoke(main, ["analytics", "rul"])
        assert result.exit_code == 0
        assert "Remaining Useful Life" in result.output
        assert "120" in result.output


class TestAnalyticsCycles:
    @patch("gateway.analytics.cycle_analyzer.CycleAnalyzer")
    def test_insufficient_data(self, cls):
        _ctx_instance(cls).analyze_cycles.return_value = None
        result = runner.invoke(main, ["analytics", "cycles"])
        assert result.exit_code == 0
        assert "Insufficient data" in result.output

    @patch("gateway.analytics.cycle_analyzer.CycleAnalyzer")
    def test_reports_cycles(self, cls):
        _ctx_instance(cls).analyze_cycles.return_value = {
            "total_cycles": 30, "data_points": 500, "charging_sessions": 12,
            "avg_charge_duration_min": 45.0, "capacity_fade_pct": 3.2,
            "voltage_early_avg": 3.90, "voltage_late_avg": 3.85,
            "temperature_avg_c": 25.0, "temperature_min_c": 18.0, "temperature_max_c": 33.0,
            "computed_at": "2026-01-01T00:00:00Z",
        }
        result = runner.invoke(main, ["analytics", "cycles"])
        assert result.exit_code == 0
        assert "Charge Cycle Analysis" in result.output
        assert "30" in result.output


class TestScan:
    @patch("gateway.scanner.list_nearby_devices", new_callable=AsyncMock)
    def test_no_devices(self, mock_scan):
        mock_scan.return_value = []
        result = runner.invoke(main, ["scan"])
        assert result.exit_code == 0
        assert "No BLE devices found" in result.output

    @patch("gateway.scanner.list_nearby_devices", new_callable=AsyncMock)
    def test_lists_devices(self, mock_scan):
        dev = MagicMock()
        dev.name = "iBattery-STM32"
        dev.address = "AA:BB:CC:DD:EE:FF"
        dev.rssi = -55
        mock_scan.return_value = [dev]
        result = runner.invoke(main, ["scan"])
        assert result.exit_code == 0
        assert "Found 1 device(s)" in result.output
        assert "iBattery-STM32" in result.output


class TestStreamDeviceLookup:
    @patch("gateway.scanner.scan_for_device", new_callable=AsyncMock)
    def test_device_not_found_exits_nonzero(self, mock_scan):
        mock_scan.return_value = None
        result = runner.invoke(main, ["stream", "--device-name", "Nope", "--timeout", "0.1"])
        assert result.exit_code == 1
        assert "not found" in result.output
