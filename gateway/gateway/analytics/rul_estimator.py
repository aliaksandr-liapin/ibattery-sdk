"""Remaining Useful Life (RUL) estimation.

Uses linear regression on health score vs cycle count to extrapolate
when health will reach the end-of-life threshold (default: 80%).
"""

import logging
from datetime import datetime, timezone
from typing import Optional

from influxdb_client import InfluxDBClient

from .. import config

logger = logging.getLogger(__name__)

# Battery is considered end-of-life when health score drops below this
DEFAULT_EOL_THRESHOLD = 80


class RULEstimator:
    """Estimates remaining useful life from cycle count and voltage trends."""

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

    def estimate_rul(
        self,
        device: str = "iBattery",
        window_days: int = 90,
        eol_threshold: int = DEFAULT_EOL_THRESHOLD,
    ) -> Optional[dict]:
        """Estimate remaining useful life.

        Queries voltage and cycle count over time, fits a linear trend,
        and extrapolates to the EOL threshold.

        Returns:
            Dict with RUL estimate, or None if insufficient data.
        """
        flux = f'''
        from(bucket: "{self.bucket}")
          |> range(start: -{window_days}d)
          |> filter(fn: (r) => r._measurement == "battery_telemetry")
          |> filter(fn: (r) => r.device == "{device}")
          |> filter(fn: (r) => r._field == "voltage_v" or r._field == "cycle_count")
          |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
          |> sort(columns: ["_time"])
          |> yield(name: "rul_data")
        '''

        try:
            tables = self._query_api.query(flux, org=self.org)
        except Exception:
            logger.exception("Failed to query InfluxDB for RUL estimation")
            return None

        data_points = []
        for table in tables:
            for record in table.records:
                voltage = record.values.get("voltage_v")
                cycles = record.values.get("cycle_count")
                if voltage is not None and cycles is not None:
                    data_points.append({
                        "voltage_v": float(voltage),
                        "cycle_count": int(cycles),
                        "time": record.get_time(),
                    })

        if len(data_points) < 2:
            logger.warning("Insufficient data for RUL estimation (need >= 2 points)")
            return None

        # Extract voltage and cycle arrays
        voltages = [p["voltage_v"] for p in data_points]
        cycles = [p["cycle_count"] for p in data_points]

        current_voltage = voltages[-1]
        current_cycles = cycles[-1]
        max_cycles = max(cycles)

        # Simple health proxy: voltage ratio to baseline
        baseline_v = voltages[0]
        if baseline_v <= 0:
            return None

        current_health = (current_voltage / baseline_v) * 100.0

        # Linear regression: health vs cycle count
        # Only meaningful if we have varying cycle counts
        unique_cycles = len(set(cycles))
        if unique_cycles < 2 or max_cycles == 0:
            return {
                "current_health": round(current_health, 1),
                "current_cycles": current_cycles,
                "eol_threshold": eol_threshold,
                "remaining_cycles": None,
                "status": "insufficient_cycling_data",
                "data_points": len(data_points),
                "computed_at": datetime.now(timezone.utc).isoformat(),
            }

        # Compute linear regression: health = slope * cycles + intercept
        slope, intercept = self._linear_regression(cycles, voltages)

        # Extrapolate: at what cycle count does voltage hit EOL threshold?
        eol_voltage = baseline_v * (eol_threshold / 100.0)

        if slope >= 0:
            # Voltage not declining — battery not degrading
            return {
                "current_health": round(current_health, 1),
                "current_cycles": current_cycles,
                "eol_threshold": eol_threshold,
                "remaining_cycles": None,
                "status": "no_degradation_detected",
                "slope_mv_per_cycle": round(slope * 1000, 4),
                "data_points": len(data_points),
                "computed_at": datetime.now(timezone.utc).isoformat(),
            }

        # cycles_at_eol = (eol_voltage - intercept) / slope
        cycles_at_eol = (eol_voltage - intercept) / slope
        remaining = max(0, int(cycles_at_eol - current_cycles))

        return {
            "current_health": round(current_health, 1),
            "current_cycles": current_cycles,
            "eol_threshold": eol_threshold,
            "remaining_cycles": remaining,
            "cycles_at_eol": int(cycles_at_eol),
            "status": "estimated",
            "slope_mv_per_cycle": round(slope * 1000, 4),
            "data_points": len(data_points),
            "computed_at": datetime.now(timezone.utc).isoformat(),
        }

    @staticmethod
    def _linear_regression(x: list, y: list) -> tuple[float, float]:
        """Simple linear regression: y = slope * x + intercept."""
        n = len(x)
        sum_x = sum(x)
        sum_y = sum(y)
        sum_xy = sum(xi * yi for xi, yi in zip(x, y))
        sum_x2 = sum(xi * xi for xi in x)

        denom = n * sum_x2 - sum_x * sum_x
        if denom == 0:
            return 0.0, sum_y / n if n > 0 else 0.0

        slope = (n * sum_xy - sum_x * sum_y) / denom
        intercept = (sum_y - slope * sum_x) / n
        return slope, intercept

    def close(self) -> None:
        self._client.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        return False
