"""InfluxDB 2.x writer for iBattery telemetry data."""

import logging
from datetime import datetime, timezone

from influxdb_client import InfluxDBClient, Point, WritePrecision
from influxdb_client.client.write_api import SYNCHRONOUS

from . import config

logger = logging.getLogger(__name__)


class TelemetryWriter:
    """Writes decoded telemetry packets to InfluxDB 2.x."""

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
        self._write_api = self._client.write_api(write_options=SYNCHRONOUS)
        logger.info("InfluxDB writer initialized: %s org=%s bucket=%s", url, org, bucket)

    def write(self, decoded: dict, device_name: str = config.DEVICE_NAME) -> None:
        """Write a decoded telemetry packet to InfluxDB.

        Args:
            decoded: Dictionary from decoder.decode_packet().
            device_name: BLE device name tag.
        """
        point = (
            Point("battery_telemetry")
            .tag("device", device_name)
            .field("voltage_v", decoded["voltage_v"])
            .field("voltage_mv", decoded["voltage_mv"])
            .field("temperature_c", decoded["temperature_c"])
            .field("soc_pct", decoded["soc_pct"])
            .field("power_state", decoded["power_state_raw"])
            .field("status_flags", decoded["status_flags"])
            .time(datetime.now(timezone.utc), WritePrecision.MS)
        )

        try:
            self._write_api.write(bucket=self.bucket, org=self.org, record=point)
        except Exception:
            logger.exception("Failed to write telemetry to InfluxDB")

    def close(self) -> None:
        """Close the InfluxDB client connection."""
        self._client.close()
        logger.info("InfluxDB writer closed")

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        return False
