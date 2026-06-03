"""Tests for battery telemetry anomaly detection."""

from datetime import datetime, timedelta, timezone
from unittest.mock import MagicMock, patch

from gateway.analytics.anomaly_detector import AnomalyDetector

T0 = datetime(2026, 1, 1, 0, 0, 0, tzinfo=timezone.utc)


def _vrec(voltage=None, soc=None, t=T0):
    """Pivoted voltage/soc record: AnomalyDetector reads record.values + get_time()."""
    r = MagicMock()
    r.values = {"voltage_v": voltage, "soc_pct": soc}
    r.get_time.return_value = t
    return r


def _trec(temp, t=T0):
    """Temperature record: AnomalyDetector reads get_value() + get_time()."""
    r = MagicMock()
    r.get_value.return_value = temp
    r.get_time.return_value = t
    return r


def _tables(records):
    table = MagicMock()
    table.records = records
    return [table]


def _detector(query_return=None, query_exc=None):
    """Build an AnomalyDetector with a mocked InfluxDB query_api."""
    with patch("gateway.analytics.anomaly_detector.InfluxDBClient") as cls:
        query_api = MagicMock()
        if query_exc is not None:
            query_api.query.side_effect = query_exc
        else:
            query_api.query.return_value = query_return
        cls.return_value.query_api.return_value = query_api
        return AnomalyDetector()


class TestVoltageAnomalies:
    def test_sudden_voltage_drop_flagged(self):
        det = _detector(_tables([_vrec(voltage=3.80), _vrec(voltage=3.50)]))
        anomalies = det.detect_voltage_anomalies()
        drops = [a for a in anomalies if a["type"] == "voltage_drop"]
        assert len(drops) == 1
        assert drops[0]["value"] == 0.30
        assert drops[0]["severity"] == "warning"

    def test_gradual_voltage_change_not_flagged(self):
        det = _detector(_tables([_vrec(voltage=3.80), _vrec(voltage=3.75)]))
        drops = [a for a in det.detect_voltage_anomalies() if a["type"] == "voltage_drop"]
        assert drops == []

    def test_soc_voltage_inconsistency_flagged(self):
        # voltage below LOW threshold (2.8) but SoC high (>50) → inconsistency
        det = _detector(_tables([_vrec(voltage=2.70, soc=60.0)]))
        inc = [a for a in det.detect_voltage_anomalies() if a["type"] == "soc_inconsistency"]
        assert len(inc) == 1
        assert inc[0]["value"] == 2.70

    def test_low_voltage_with_low_soc_is_consistent(self):
        det = _detector(_tables([_vrec(voltage=2.70, soc=5.0)]))
        inc = [a for a in det.detect_voltage_anomalies() if a["type"] == "soc_inconsistency"]
        assert inc == []

    def test_query_failure_returns_empty(self):
        det = _detector(query_exc=Exception("influx down"))
        assert det.detect_voltage_anomalies() == []

    def test_no_data_returns_empty(self):
        det = _detector(_tables([]))
        assert det.detect_voltage_anomalies() == []


class TestTemperatureAnomalies:
    def test_high_temperature_flagged(self):
        det = _detector(_tables([_trec(50.0)]))
        hi = [a for a in det.detect_temperature_anomalies() if a["type"] == "temp_high"]
        assert len(hi) == 1
        assert hi[0]["severity"] == "warning"

    def test_low_temperature_flagged_critical(self):
        det = _detector(_tables([_trec(-5.0)]))
        lo = [a for a in det.detect_temperature_anomalies() if a["type"] == "temp_low"]
        assert len(lo) == 1
        assert lo[0]["severity"] == "critical"

    def test_temperature_spike_rate_flagged(self):
        # 25C -> 45C over 60s = 20 C/min > 15 C/min threshold
        det = _detector(_tables([_trec(25.0, T0), _trec(45.0, T0 + timedelta(seconds=60))]))
        spikes = [a for a in det.detect_temperature_anomalies() if a["type"] == "temp_spike"]
        assert len(spikes) == 1
        assert spikes[0]["value"] == 20.0

    def test_slow_temperature_change_not_flagged(self):
        # 25C -> 30C over 60s = 5 C/min < 15 threshold
        det = _detector(_tables([_trec(25.0, T0), _trec(30.0, T0 + timedelta(seconds=60))]))
        spikes = [a for a in det.detect_temperature_anomalies() if a["type"] == "temp_spike"]
        assert spikes == []

    def test_none_temperature_skipped(self):
        det = _detector(_tables([_trec(None)]))
        assert det.detect_temperature_anomalies() == []

    def test_query_failure_returns_empty(self):
        det = _detector(query_exc=Exception("influx down"))
        assert det.detect_temperature_anomalies() == []


class TestContextManager:
    def test_context_manager_closes_client(self):
        det = _detector(_tables([]))
        with det as d:
            assert d is det
        det._client.close.assert_called_once()
