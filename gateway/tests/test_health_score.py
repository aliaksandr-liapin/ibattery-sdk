"""Tests for battery health score computation."""

from unittest.mock import MagicMock, patch

from gateway.analytics.health_score import BatteryHealthScorer


def _make_record(value):
    """Create a mock InfluxDB record."""
    record = MagicMock()
    record.get_value.return_value = value
    return record


def _make_tables(voltage_values):
    """Create mock InfluxDB tables from a list of voltage values."""
    table = MagicMock()
    table.records = [_make_record(v) for v in voltage_values]
    return [table]


class TestHealthScoreComputation:
    """Health score computation tests."""

    @patch("gateway.analytics.health_score.InfluxDBClient")
    def test_stable_voltage_gives_score_100(self, mock_client_cls):
        mock_query = MagicMock()
        mock_query.query.return_value = _make_tables([3.80, 3.80, 3.80, 3.80])
        mock_client_cls.return_value.query_api.return_value = mock_query

        scorer = BatteryHealthScorer()
        result = scorer.compute_health_score()

        assert result is not None
        assert result["score"] == 100
        assert result["trend"] == "stable"

    @patch("gateway.analytics.health_score.InfluxDBClient")
    def test_declining_voltage_gives_lower_score(self, mock_client_cls):
        mock_query = MagicMock()
        mock_query.query.return_value = _make_tables([3.80, 3.75, 3.70, 3.60])
        mock_client_cls.return_value.query_api.return_value = mock_query

        scorer = BatteryHealthScorer()
        result = scorer.compute_health_score()

        assert result is not None
        assert result["score"] < 100
        assert result["trend"] == "declining"

    @patch("gateway.analytics.health_score.InfluxDBClient")
    def test_no_data_returns_none(self, mock_client_cls):
        mock_query = MagicMock()
        mock_query.query.return_value = _make_tables([])
        mock_client_cls.return_value.query_api.return_value = mock_query

        scorer = BatteryHealthScorer()
        result = scorer.compute_health_score()

        assert result is None

    @patch("gateway.analytics.health_score.InfluxDBClient")
    def test_single_data_point(self, mock_client_cls):
        mock_query = MagicMock()
        mock_query.query.return_value = _make_tables([3.85])
        mock_client_cls.return_value.query_api.return_value = mock_query

        scorer = BatteryHealthScorer()
        result = scorer.compute_health_score()

        assert result is not None
        assert result["score"] == 100
        assert result["data_points"] == 1

    @patch("gateway.analytics.health_score.InfluxDBClient")
    def test_result_contains_expected_keys(self, mock_client_cls):
        mock_query = MagicMock()
        mock_query.query.return_value = _make_tables([3.80, 3.78])
        mock_client_cls.return_value.query_api.return_value = mock_query

        scorer = BatteryHealthScorer()
        result = scorer.compute_health_score()

        assert result is not None
        expected_keys = {"score", "trend", "baseline_v", "current_v",
                         "variance_ratio", "data_points", "computed_at"}
        assert expected_keys == set(result.keys())

    @patch("gateway.analytics.health_score.InfluxDBClient")
    def test_score_clamped_to_100(self, mock_client_cls):
        """If current voltage is somehow higher than baseline, clamp to 100."""
        mock_query = MagicMock()
        mock_query.query.return_value = _make_tables([3.70, 3.80])
        mock_client_cls.return_value.query_api.return_value = mock_query

        scorer = BatteryHealthScorer()
        result = scorer.compute_health_score()

        assert result is not None
        assert result["score"] <= 100

    @patch("gateway.analytics.health_score.InfluxDBClient")
    def test_query_exception_returns_none(self, mock_client_cls):
        mock_query = MagicMock()
        mock_query.query.side_effect = Exception("Connection refused")
        mock_client_cls.return_value.query_api.return_value = mock_query

        scorer = BatteryHealthScorer()
        result = scorer.compute_health_score()

        assert result is None
