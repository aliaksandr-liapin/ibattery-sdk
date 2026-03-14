# iBattery Cloud Stack

Local Docker Compose stack for battery telemetry storage and visualization.

## Components

| Service | Port | Description |
|---------|------|-------------|
| InfluxDB 2.x | 8086 | Time-series database for telemetry data |
| Grafana | 3000 | Dashboard and visualization |

## Quick Start

```bash
cd cloud
docker compose up -d
```

## Access

- **InfluxDB UI**: http://localhost:8086 (admin / ibattery-admin)
- **Grafana**: http://localhost:3000 (admin / admin, or anonymous viewer)

## Pre-configured

- InfluxDB org: `ibattery`, bucket: `telemetry`, token: `ibattery-dev-token`
- Grafana datasource: auto-provisioned InfluxDB connection
- Grafana dashboard: "iBattery Telemetry" with 6 panels (voltage, temperature, SoC, power state, raw voltage, status flags)

## Stop

```bash
docker compose down        # Stop containers, keep data
docker compose down -v     # Stop containers and delete data volumes
```
