"""Tests for the RUL (Remaining Useful Life) estimator."""

import pytest

from gateway.analytics.rul_estimator import RULEstimator


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
