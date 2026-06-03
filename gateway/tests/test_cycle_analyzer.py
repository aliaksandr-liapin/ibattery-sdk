"""Tests for the charge cycle analyzer."""

from datetime import datetime, timedelta, timezone
from unittest.mock import MagicMock, patch

from gateway.analytics.cycle_analyzer import CycleAnalyzer

T0 = datetime(2026, 1, 1, 0, 0, 0, tzinfo=timezone.utc)


class TestCycleAnalyzerLifecycle:
    def test_class_instantiation(self):
        analyzer = CycleAnalyzer(
            url="http://localhost:8086", token="test-token",
            org="test-org", bucket="test-bucket",
        )
        assert analyzer.org == "test-org"
        assert analyzer.bucket == "test-bucket"
        analyzer.close()

    def test_context_manager(self):
        with CycleAnalyzer(
            url="http://localhost:8086", token="test-token",
            org="test-org", bucket="test-bucket",
        ) as analyzer:
            assert analyzer is not None


def _rec(voltage=None, cycles=None, temp=None, power_state=None, t=T0):
    r = MagicMock()
    r.values = {
        "voltage_v": voltage, "cycle_count": cycles,
        "temperature_c": temp, "power_state": power_state,
    }
    r.get_time.return_value = t
    return r


def _tables(records):
    table = MagicMock()
    table.records = records
    return [table]


def _analyzer(query_return=None, query_exc=None):
    with patch("gateway.analytics.cycle_analyzer.InfluxDBClient") as cls:
        q = MagicMock()
        if query_exc is not None:
            q.query.side_effect = query_exc
        else:
            q.query.return_value = query_return
        cls.return_value.query_api.return_value = q
        return CycleAnalyzer()


class TestAnalyzeCycles:
    def test_insufficient_records_returns_none(self):
        an = _analyzer(_tables([_rec(3.9, 1)]))
        assert an.analyze_cycles() is None

    def test_query_failure_returns_none(self):
        an = _analyzer(query_exc=Exception("influx down"))
        assert an.analyze_cycles() is None

    def test_basic_statistics(self):
        recs = [
            _rec(3.90, 5, 25.0, 2, T0),
            _rec(3.88, 7, 26.0, 2, T0 + timedelta(minutes=1)),
            _rec(3.87, 9, 27.0, 2, T0 + timedelta(minutes=2)),
        ]
        result = _analyzer(_tables(recs)).analyze_cycles()
        assert result["total_cycles"] == 9        # max cycle_count
        assert result["data_points"] == 3
        assert result["temperature_avg_c"] == 26.0
        assert result["temperature_min_c"] == 25.0
        assert result["temperature_max_c"] == 27.0

    def test_charging_session_detected(self):
        # power_state 5 == CHARGING; one session of 2 minutes (60s->180s)
        recs = [
            _rec(3.80, 1, 25.0, 2, T0),
            _rec(3.85, 1, 25.0, 5, T0 + timedelta(seconds=60)),   # start charging
            _rec(3.95, 1, 26.0, 5, T0 + timedelta(seconds=120)),
            _rec(4.00, 2, 26.0, 4, T0 + timedelta(seconds=180)),  # charge complete
        ]
        result = _analyzer(_tables(recs)).analyze_cycles()
        assert result["charging_sessions"] == 1
        assert result["avg_charge_duration_min"] == 2.0

    def test_capacity_fade_from_declining_voltage(self):
        # >= 10 voltage points, declining → positive fade percent
        recs = [_rec(4.0 - i * 0.02, 1, 25.0, 2, T0 + timedelta(minutes=i)) for i in range(12)]
        result = _analyzer(_tables(recs)).analyze_cycles()
        assert result["capacity_fade_pct"] > 0
        assert result["voltage_late_avg"] < result["voltage_early_avg"]
