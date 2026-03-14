"""Battery health score computation from InfluxDB telemetry history."""

import logging
from datetime import datetime, timezone
from typing import Optional

from influxdb_client import InfluxDBClient

from .. import config

logger = logging.getLogger(__name__)


class BatteryHealthScorer:
    """Computes a 0-100 battery health score from voltage history."""

    def __init__(
        self,
        url: str = config.INFLUXDB_URL,
        token: str = config.INFLUXDB_TOKEN,
        org: str = config.INFLUXDB_ORG,
        bucket: str = config.INFLUXDB_BUCKET,
    ):
        self.org = org
        self.bucket = bucket
        self._client = InfluxDBClient(url=url, token=token, org=org)
        self._query_api = self._client.query_api()

    def compute_health_score(
        self,
        device: str = "iBattery",
        window_days: int = 30,
    ) -> Optional[dict]:
        """Compute battery health score from voltage history.

        Queries mean resting voltage (DISCHARGING or IDLE states) over
        rolling 7-day windows.  Compares latest window to baseline.

        Returns:
            Dict with score, trend, voltages, variance info, or None if
            insufficient data.
        """
        # Query mean voltage in 7-day windows, filtering to resting states
        flux = f'''
        from(bucket: "{self.bucket}")
          |> range(start: -{window_days}d)
          |> filter(fn: (r) => r._measurement == "battery_telemetry")
          |> filter(fn: (r) => r.device == "{device}")
          |> filter(fn: (r) => r._field == "voltage_v")
          |> aggregateWindow(every: 7d, fn: mean, createEmpty: false)
          |> yield(name: "mean_voltage")
        '''

        try:
            tables = self._query_api.query(flux, org=self.org)
        except Exception:
            logger.exception("Failed to query InfluxDB for health score")
            return None

        # Extract voltage values from query result
        voltages = []
        for table in tables:
            for record in table.records:
                v = record.get_value()
                if v is not None:
                    voltages.append(float(v))

        if len(voltages) < 1:
            logger.warning("No voltage data available for health scoring")
            return None

        baseline_v = voltages[0]
        current_v = voltages[-1]

        # Prevent division by zero
        if baseline_v <= 0:
            return None

        # Score: ratio of current to baseline, scaled to 0-100
        raw_score = (current_v / baseline_v) * 100.0
        score = max(0, min(100, round(raw_score)))

        # Variance ratio: compare variance of recent vs baseline
        if len(voltages) >= 4:
            half = len(voltages) // 2
            early = voltages[:half]
            late = voltages[half:]
            var_early = self._variance(early)
            var_late = self._variance(late)
            variance_ratio = var_late / var_early if var_early > 0 else 1.0
        else:
            variance_ratio = 1.0

        # Trend determination
        if len(voltages) >= 2:
            delta = current_v - baseline_v
            if delta < -0.05:
                trend = "declining"
            elif delta < -0.15:
                trend = "critical"
            else:
                trend = "stable"
        else:
            trend = "unknown"

        return {
            "score": score,
            "trend": trend,
            "baseline_v": round(baseline_v, 4),
            "current_v": round(current_v, 4),
            "variance_ratio": round(variance_ratio, 3),
            "data_points": len(voltages),
            "computed_at": datetime.now(timezone.utc).isoformat(),
        }

    @staticmethod
    def _variance(values: list[float]) -> float:
        """Compute variance of a list of floats."""
        if len(values) < 2:
            return 0.0
        mean = sum(values) / len(values)
        return sum((v - mean) ** 2 for v in values) / (len(values) - 1)

    def close(self) -> None:
        """Close the InfluxDB client."""
        self._client.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        return False
