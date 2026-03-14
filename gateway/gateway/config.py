"""Default configuration constants for the iBattery gateway."""

# BLE UUIDs (must match firmware in battery_transport_ble_zephyr.c)
SERVICE_UUID = "12340001-5678-9abc-def0-123456789abc"
CHAR_UUID = "12340002-5678-9abc-def0-123456789abc"

# Default BLE device name (CONFIG_BT_DEVICE_NAME in firmware prj.conf)
DEVICE_NAME = "iBattery"

# BLE scan timeout (seconds)
SCAN_TIMEOUT = 10.0

# InfluxDB 2.x connection defaults (match cloud/docker-compose.yml)
INFLUXDB_URL = "http://localhost:8086"
INFLUXDB_TOKEN = "ibattery-dev-token"
INFLUXDB_ORG = "ibattery"
INFLUXDB_BUCKET = "telemetry"

# Reconnect settings
RECONNECT_DELAY_INITIAL = 1.0   # seconds
RECONNECT_DELAY_MAX = 30.0      # seconds
RECONNECT_BACKOFF_FACTOR = 2.0
