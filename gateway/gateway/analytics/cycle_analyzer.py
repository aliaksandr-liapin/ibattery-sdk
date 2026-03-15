"""Charge cycle analyzer — capacity fade, charge duration trends, temperature impact."""

import logging
from datetime import datetime, timezone
from typing import Optional

from influxdb_client import InfluxDBClient

from .. import config

logger = logging.getLogger(__name__)


class CycleAnalyzer:
    """Analyzes charge cycle patterns from InfluxDB telemetry history."""

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

    def analyze_cycles(
        self,
        device: str = "iBattery",
        window_days: int = 90,
    ) -> Optional[dict]:
        """Analyze charge cycle patterns.

        Returns:
            Dict with cycle statistics, or None if insufficient data.
        """
        flux = f'''
        from(bucket: "{self.bucket}")
          |> range(start: -{window_days}d)
          |> filter(fn: (r) => r._measurement == "battery_telemetry")
          |> filter(fn: (r) => r.device == "{device}")
          |> filter(fn: (r) =>
              r._field == "voltage_v" or
              r._field == "cycle_count" or
              r._field == "temperature_c" or
              r._field == "power_state")
          |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
          |> sort(columns: ["_time"])
          |> yield(name: "cycle_data")
        '''

        try:
            tables = self._query_api.query(flux, org=self.org)
        except Exception:
            logger.exception("Failed to query InfluxDB for cycle analysis")
            return None

        records = []
        for table in tables:
            for record in table.records:
                records.append({
                    "time": record.get_time(),
                    "voltage_v": record.values.get("voltage_v"),
                    "cycle_count": record.values.get("cycle_count"),
                    "temperature_c": record.values.get("temperature_c"),
                    "power_state": record.values.get("power_state"),
                })

        if len(records) < 2:
            return None

        # Extract cycle count range
        cycle_counts = [r["cycle_count"] for r in records if r["cycle_count"] is not None]
        voltages = [r["voltage_v"] for r in records if r["voltage_v"] is not None]
        temps = [r["temperature_c"] for r in records if r["temperature_c"] is not None]

        total_cycles = max(cycle_counts) if cycle_counts else 0

        # Detect charging sessions (power_state == 5)
        charging_sessions = []
        in_charging = False
        session_start = None
        for r in records:
            ps = r.get("power_state")
            if ps == 5 and not in_charging:  # CHARGING
                in_charging = True
                session_start = r["time"]
            elif ps != 5 and in_charging:
                in_charging = False
                if session_start and r["time"]:
                    duration = (r["time"] - session_start).total_seconds()
                    if duration > 0:
                        charging_sessions.append({
                            "start": session_start.isoformat(),
                            "duration_min": round(duration / 60, 1),
                        })

        # Capacity fade: compare early vs late voltage averages
        if len(voltages) >= 10:
            quarter = len(voltages) // 4
            early_avg = sum(voltages[:quarter]) / quarter
            late_avg = sum(voltages[-quarter:]) / quarter
            fade_pct = ((early_avg - late_avg) / early_avg) * 100 if early_avg > 0 else 0.0
        else:
            early_avg = late_avg = voltages[-1] if voltages else 0
            fade_pct = 0.0

        # Temperature impact
        if temps:
            avg_temp = sum(temps) / len(temps)
            min_temp = min(temps)
            max_temp = max(temps)
        else:
            avg_temp = min_temp = max_temp = 0.0

        return {
            "total_cycles": total_cycles,
            "data_points": len(records),
            "charging_sessions": len(charging_sessions),
            "avg_charge_duration_min": (
                round(sum(s["duration_min"] for s in charging_sessions) / len(charging_sessions), 1)
                if charging_sessions else None
            ),
            "capacity_fade_pct": round(fade_pct, 2),
            "voltage_early_avg": round(early_avg, 4),
            "voltage_late_avg": round(late_avg, 4),
            "temperature_avg_c": round(avg_temp, 1),
            "temperature_min_c": round(min_temp, 1),
            "temperature_max_c": round(max_temp, 1),
            "computed_at": datetime.now(timezone.utc).isoformat(),
        }

    def close(self) -> None:
        self._client.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        return False
