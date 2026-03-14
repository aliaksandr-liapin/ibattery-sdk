"""Anomaly detection for battery telemetry data."""

import logging
from datetime import datetime, timezone
from typing import Optional

from influxdb_client import InfluxDBClient

from .. import config

logger = logging.getLogger(__name__)

# Thresholds
VOLTAGE_DROP_THRESHOLD_V = 0.200       # 200 mV sudden drop
VOLTAGE_LOW_THRESHOLD_V = 3.2          # Low voltage warning
VOLTAGE_CRITICAL_THRESHOLD_V = 3.0     # Critical voltage
SOC_VOLTAGE_INCONSISTENCY_SOC = 30.0   # SoC above this + low voltage = anomaly
TEMP_HIGH_THRESHOLD_C = 45.0           # High temperature warning
TEMP_LOW_THRESHOLD_C = 0.0             # Low temperature (LiPo danger)
TEMP_RATE_THRESHOLD_C_PER_MIN = 5.0    # Rate of change warning


class AnomalyDetector:
    """Detects anomalies in battery telemetry from InfluxDB history."""

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

    def detect_voltage_anomalies(
        self,
        device: str = "iBattery",
        window_minutes: int = 60,
    ) -> list[dict]:
        """Detect sudden voltage drops and SoC-voltage inconsistencies."""
        flux = f'''
        from(bucket: "{self.bucket}")
          |> range(start: -{window_minutes}m)
          |> filter(fn: (r) => r._measurement == "battery_telemetry")
          |> filter(fn: (r) => r.device == "{device}")
          |> filter(fn: (r) => r._field == "voltage_v" or r._field == "soc_pct")
          |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
          |> sort(columns: ["_time"])
          |> yield(name: "voltage_soc")
        '''

        anomalies = []
        try:
            tables = self._query_api.query(flux, org=self.org)
        except Exception:
            logger.exception("Failed to query InfluxDB for voltage anomalies")
            return anomalies

        prev_voltage = None
        for table in tables:
            for record in table.records:
                voltage = record.values.get("voltage_v")
                soc = record.values.get("soc_pct")
                ts = record.get_time().isoformat() if record.get_time() else "unknown"

                if voltage is not None:
                    # Check sudden voltage drop
                    if prev_voltage is not None:
                        delta = prev_voltage - voltage
                        if delta > VOLTAGE_DROP_THRESHOLD_V:
                            anomalies.append({
                                "type": "voltage_drop",
                                "severity": "warning",
                                "timestamp": ts,
                                "value": round(delta, 4),
                                "threshold": VOLTAGE_DROP_THRESHOLD_V,
                                "message": f"Sudden voltage drop of {delta:.3f}V",
                            })

                    # Check SoC-voltage inconsistency
                    if soc is not None and voltage < VOLTAGE_LOW_THRESHOLD_V and soc > SOC_VOLTAGE_INCONSISTENCY_SOC:
                        anomalies.append({
                            "type": "soc_inconsistency",
                            "severity": "warning",
                            "timestamp": ts,
                            "value": round(voltage, 4),
                            "threshold": VOLTAGE_LOW_THRESHOLD_V,
                            "message": f"Voltage {voltage:.3f}V but SoC {soc:.1f}%",
                        })

                    prev_voltage = voltage

        return anomalies

    def detect_temperature_anomalies(
        self,
        device: str = "iBattery",
        window_minutes: int = 60,
    ) -> list[dict]:
        """Detect temperature spikes and dangerous ranges."""
        flux = f'''
        from(bucket: "{self.bucket}")
          |> range(start: -{window_minutes}m)
          |> filter(fn: (r) => r._measurement == "battery_telemetry")
          |> filter(fn: (r) => r.device == "{device}")
          |> filter(fn: (r) => r._field == "temperature_c")
          |> sort(columns: ["_time"])
          |> yield(name: "temperature")
        '''

        anomalies = []
        try:
            tables = self._query_api.query(flux, org=self.org)
        except Exception:
            logger.exception("Failed to query InfluxDB for temperature anomalies")
            return anomalies

        prev_temp = None
        prev_time = None
        for table in tables:
            for record in table.records:
                temp = record.get_value()
                ts = record.get_time()

                if temp is None:
                    continue

                ts_str = ts.isoformat() if ts else "unknown"

                # High temperature
                if temp > TEMP_HIGH_THRESHOLD_C:
                    anomalies.append({
                        "type": "temp_high",
                        "severity": "warning",
                        "timestamp": ts_str,
                        "value": round(temp, 2),
                        "threshold": TEMP_HIGH_THRESHOLD_C,
                        "message": f"Temperature {temp:.1f}C exceeds {TEMP_HIGH_THRESHOLD_C}C",
                    })

                # Low temperature (LiPo danger)
                if temp < TEMP_LOW_THRESHOLD_C:
                    anomalies.append({
                        "type": "temp_low",
                        "severity": "critical",
                        "timestamp": ts_str,
                        "value": round(temp, 2),
                        "threshold": TEMP_LOW_THRESHOLD_C,
                        "message": f"Temperature {temp:.1f}C below {TEMP_LOW_THRESHOLD_C}C — LiPo danger",
                    })

                # Rate of change
                if prev_temp is not None and prev_time is not None and ts is not None:
                    dt_seconds = (ts - prev_time).total_seconds()
                    if dt_seconds > 0:
                        rate_per_min = abs(temp - prev_temp) / (dt_seconds / 60.0)
                        if rate_per_min > TEMP_RATE_THRESHOLD_C_PER_MIN:
                            anomalies.append({
                                "type": "temp_spike",
                                "severity": "warning",
                                "timestamp": ts_str,
                                "value": round(rate_per_min, 2),
                                "threshold": TEMP_RATE_THRESHOLD_C_PER_MIN,
                                "message": f"Temperature changing at {rate_per_min:.1f}C/min",
                            })

                prev_temp = temp
                prev_time = ts

        return anomalies

    def close(self) -> None:
        """Close the InfluxDB client."""
        self._client.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        return False
