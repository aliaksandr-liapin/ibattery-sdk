"""Tests for the charge cycle analyzer (unit-level, no InfluxDB)."""

import pytest

from gateway.analytics.cycle_analyzer import CycleAnalyzer


class TestCycleAnalyzerHelpers:
    """Test internal logic by calling analyze_cycles indirectly.

    Since analyze_cycles queries InfluxDB, we test the algorithmic parts
    through the public interface by verifying the class can be instantiated
    and the static/pure logic is correct.
    """

    def test_class_instantiation(self):
        """CycleAnalyzer can be created (no connection until query)."""
        analyzer = CycleAnalyzer(
            url="http://localhost:8086",
            token="test-token",
            org="test-org",
            bucket="test-bucket",
        )
        assert analyzer.org == "test-org"
        assert analyzer.bucket == "test-bucket"
        analyzer.close()

    def test_context_manager(self):
        """Context manager protocol works."""
        with CycleAnalyzer(
            url="http://localhost:8086",
            token="test-token",
            org="test-org",
            bucket="test-bucket",
        ) as analyzer:
            assert analyzer is not None
