# iBattery BLE Gateway

Python BLE gateway that receives telemetry from the iBattery nRF52840 firmware and writes it to InfluxDB for visualization in Grafana.

## Prerequisites

- Python 3.10+
- Bluetooth adapter (built-in or USB dongle)
- Docker (for InfluxDB + Grafana stack)

## Install

```bash
cd gateway
pip install -e .
```

For development (includes pytest):

```bash
pip install -e ".[dev]"
```

## Usage

### Scan for devices

```bash
ibattery-gateway scan
```

### Stream to terminal (no database)

```bash
ibattery-gateway stream
```

### Full pipeline (BLE → InfluxDB)

First start the cloud stack:

```bash
cd ../cloud && docker compose up -d
```

Then run the gateway:

```bash
ibattery-gateway run
```

### Options

```bash
ibattery-gateway run --device-name "iBattery" \
                     --influxdb-url http://localhost:8086 \
                     --influxdb-token ibattery-dev-token \
                     --influxdb-org ibattery \
                     --influxdb-bucket telemetry
```

## Tests

```bash
python -m pytest tests/ -v
```

## Architecture

```
nRF52840-DK (BLE notifications, 20/24-byte packets)
    │
    ▼
ibattery-gateway (Python / bleak)
    ├── scanner.py      — BLE scan + connect + subscribe
    ├── decoder.py        — unpack v1 (20B) / v2 (24B) LE wire format
    ├── influxdb_writer   — write Points to InfluxDB 2.x
    ├── cli.py            — click CLI (scan / stream / run / analytics)
    └── analytics/
        ├── realtime.py       — inline anomaly checks per packet
        ├── health_score.py   — voltage-based health scoring
        ├── anomaly_detector.py — historical anomaly detection
        ├── rul_estimator.py  — remaining useful life estimation
        └── cycle_analyzer.py — charge cycle pattern analysis
    │
    ▼
InfluxDB 2.x → Grafana dashboard (11 panels)
```

### Analytics commands

```bash
ibattery-gateway analytics health      # Battery health score (0-100)
ibattery-gateway analytics anomalies   # Detect anomalies in recent data
ibattery-gateway analytics rul         # Remaining useful life estimate
ibattery-gateway analytics cycles      # Charge cycle analysis
```

### Grafana dashboard

Import `grafana/ibattery-dashboard.json` into Grafana for an 11-panel dashboard.
