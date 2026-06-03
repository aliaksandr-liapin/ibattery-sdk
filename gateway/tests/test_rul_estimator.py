"""Tests for the RUL (Remaining Useful Life) estimator."""

from datetime import datetime, timezone
from unittest.mock import MagicMock, patch

import pytest

from gateway.analytics.rul_estimator import RULEstimator

T0 = datetime(2026, 1, 1, 0, 0, 0, tzinfo=timezone.utc)


class TestLinearRegression:
    """Unit tests for the internal linear regression helper."""

    def test_perfect_positive_slope(self):
        slope, intercept = RULEstimator._linear_regression([0, 1, 2], [0, 1, 2])
        assert slope == pytest.approx(1.0)
        assert intercept == pytest.approx(0.0)

    def test_perfect_negative_slope(self):
        slope, intercept = RULEstimator._linear_regression([0, 1, 2], [4.2, 4.1, 4.0])
        assert slope == pytest.approx(-0.1)
        assert intercept == pytest.approx(4.2)

    def test_flat_line(self):
        slope, intercept = RULEstimator._linear_regression([0, 10, 20], [3.5, 3.5, 3.5])
        assert slope == pytest.approx(0.0)
        assert intercept == pytest.approx(3.5)

    def test_single_x_value(self):
        """All x the same → denom is 0 → slope 0, intercept = mean(y)."""
        slope, intercept = RULEstimator._linear_regression([5, 5, 5], [1.0, 2.0, 3.0])
        assert slope == pytest.approx(0.0)
        assert intercept == pytest.approx(2.0)

    def test_two_points(self):
        slope, intercept = RULEstimator._linear_regression([0, 100], [4.2, 3.8])
        assert slope == pytest.approx(-0.004)
        assert intercept == pytest.approx(4.2)


def _rec(voltage=None, cycles=None, t=T0):
    r = MagicMock()
    r.values = {"voltage_v": voltage, "cycle_count": cycles}
    r.get_time.return_value = t
    return r


def _tables(records):
    table = MagicMock()
    table.records = records
    return [table]


def _estimator(query_return=None, query_exc=None):
    with patch("gateway.analytics.rul_estimator.InfluxDBClient") as cls:
        q = MagicMock()
        if query_exc is not None:
            q.query.side_effect = query_exc
        else:
            q.query.return_value = query_return
        cls.return_value.query_api.return_value = q
        return RULEstimator()


class TestEstimateRul:
    def test_insufficient_points_returns_none(self):
        est = _estimator(_tables([_rec(4.0, 0)]))
        assert est.estimate_rul() is None

    def test_query_failure_returns_none(self):
        est = _estimator(query_exc=Exception("influx down"))
        assert est.estimate_rul() is None

    def test_constant_cycles_is_insufficient_cycling(self):
        est = _estimator(_tables([_rec(4.0, 10), _rec(3.9, 10)]))
        result = est.estimate_rul()
        assert result["status"] == "insufficient_cycling_data"
        assert result["remaining_cycles"] is None

    def test_declining_voltage_estimates_rul(self):
        est = _estimator(_tables([_rec(4.00, 0), _rec(3.95, 50), _rec(3.90, 100)]))
        result = est.estimate_rul(eol_threshold=80)
        assert result["status"] == "estimated"
        assert isinstance(result["remaining_cycles"], int)
        assert result["remaining_cycles"] >= 0
        assert result["slope_mv_per_cycle"] < 0

    def test_rising_voltage_is_no_degradation(self):
        est = _estimator(_tables([_rec(3.90, 0), _rec(3.95, 50), _rec(4.00, 100)]))
        result = est.estimate_rul()
        assert result["status"] == "no_degradation_detected"
        assert result["remaining_cycles"] is None
