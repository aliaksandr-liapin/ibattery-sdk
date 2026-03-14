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
nRF52840-DK (BLE notifications, 20-byte packets)
    │
    ▼
ibattery-gateway (Python / bleak)
    ├── scanner.py      — BLE scan + connect + subscribe
    ├── decoder.py      — unpack 20-byte LE wire format
    ├── influxdb_writer — write Points to InfluxDB 2.x
    └── cli.py          — click CLI (scan / stream / run)
    │
    ▼
InfluxDB 2.x → Grafana dashboard
```
