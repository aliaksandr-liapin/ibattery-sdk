"""CLI entry point for the iBattery BLE gateway."""

import asyncio
import logging
import sys

import click

from . import config
from .decoder import decode_packet, format_packet

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    datefmt="%H:%M:%S",
)
logger = logging.getLogger("ibattery-gateway")


@click.group()
@click.option("--debug", is_flag=True, help="Enable debug logging")
def main(debug: bool) -> None:
    """iBattery BLE Gateway — receive telemetry and write to InfluxDB."""
    if debug:
        logging.getLogger().setLevel(logging.DEBUG)


@main.command()
@click.option("--timeout", default=config.SCAN_TIMEOUT, help="Scan timeout in seconds")
def scan(timeout: float) -> None:
    """Scan for nearby BLE devices."""
    from .scanner import list_nearby_devices

    async def _scan() -> None:
        devices = await list_nearby_devices(timeout=timeout)
        if not devices:
            click.echo("No BLE devices found.")
            return

        click.echo(f"\nFound {len(devices)} device(s):\n")
        click.echo(f"{'Name':<25} {'Address':<20} {'RSSI':<8}")
        click.echo("-" * 55)

        for dev in devices:
            name = dev.name or "(unknown)"
            rssi = f"{dev.rssi} dBm" if dev.rssi is not None else "N/A"
            marker = " ◀ iBattery" if dev.name and config.DEVICE_NAME.lower() in dev.name.lower() else ""
            click.echo(f"{name:<25} {dev.address:<20} {rssi:<8}{marker}")

    asyncio.run(_scan())


@main.command()
@click.option("--device-name", default=config.DEVICE_NAME, help="BLE device name")
@click.option("--timeout", default=config.SCAN_TIMEOUT, help="Scan timeout in seconds")
def stream(device_name: str, timeout: float) -> None:
    """Connect to iBattery and print decoded telemetry to terminal."""
    from .scanner import connect_and_stream, scan_for_device

    packet_count = 0

    def on_packet(data: bytes) -> None:
        nonlocal packet_count
        try:
            decoded = decode_packet(data)
            packet_count += 1
            click.echo(f"[{packet_count:>5}] {format_packet(decoded)}")
        except Exception as e:
            click.echo(f"[ERROR] Failed to decode {len(data)} bytes: {e}", err=True)

    async def _stream() -> None:
        device = await scan_for_device(name=device_name, timeout=timeout)
        if device is None:
            click.echo(f"Device '{device_name}' not found. Is it advertising?", err=True)
            sys.exit(1)

        click.echo(f"\nStreaming from {device.name} ({device.address})...")
        click.echo("Press Ctrl+C to stop.\n")

        await connect_and_stream(device.address, on_packet)

    try:
        asyncio.run(_stream())
    except KeyboardInterrupt:
        click.echo(f"\nStopped. Received {packet_count} packets.")


@main.command()
@click.option("--device-name", default=config.DEVICE_NAME, help="BLE device name")
@click.option("--timeout", default=config.SCAN_TIMEOUT, help="Scan timeout in seconds")
@click.option("--influxdb-url", default=config.INFLUXDB_URL, help="InfluxDB URL")
@click.option("--influxdb-token", default=config.INFLUXDB_TOKEN, help="InfluxDB API token")
@click.option("--influxdb-org", default=config.INFLUXDB_ORG, help="InfluxDB organization")
@click.option("--influxdb-bucket", default=config.INFLUXDB_BUCKET, help="InfluxDB bucket")
def run(
    device_name: str,
    timeout: float,
    influxdb_url: str,
    influxdb_token: str,
    influxdb_org: str,
    influxdb_bucket: str,
) -> None:
    """Connect to iBattery, decode telemetry, and write to InfluxDB."""
    from .analytics.realtime import check_realtime
    from .influxdb_writer import TelemetryWriter
    from .scanner import connect_and_stream, scan_for_device

    packet_count = 0
    anomaly_count = 0
    writer = TelemetryWriter(
        url=influxdb_url,
        token=influxdb_token,
        org=influxdb_org,
        bucket=influxdb_bucket,
    )

    def on_packet(data: bytes) -> None:
        nonlocal packet_count, anomaly_count
        try:
            decoded = decode_packet(data)
            packet_count += 1
            writer.write(decoded, device_name=device_name)
            click.echo(f"[{packet_count:>5}] {format_packet(decoded)}")

            # Real-time anomaly checks
            alerts = check_realtime(decoded)
            for alert in alerts:
                anomaly_count += 1
                severity = alert["severity"].upper()
                click.echo(
                    click.style(f"  [{severity}] {alert['message']}", fg="red" if severity == "CRITICAL" else "yellow"),
                    err=True,
                )
        except Exception as e:
            click.echo(f"[ERROR] {e}", err=True)

    async def _run() -> None:
        device = await scan_for_device(name=device_name, timeout=timeout)
        if device is None:
            click.echo(f"Device '{device_name}' not found. Is it advertising?", err=True)
            sys.exit(1)

        click.echo(f"\nStreaming from {device.name} ({device.address}) -> InfluxDB")
        click.echo("Press Ctrl+C to stop.\n")

        await connect_and_stream(device.address, on_packet)

    try:
        asyncio.run(_run())
    except KeyboardInterrupt:
        click.echo(f"\nStopped. Wrote {packet_count} packets to InfluxDB. {anomaly_count} anomalies detected.")
    finally:
        writer.close()


# ── Analytics commands ───────────────────────────────────────────────────────

@main.group()
def analytics() -> None:
    """Battery analytics — health scoring and anomaly detection."""
    pass


@analytics.command()
@click.option("--device", default=config.DEVICE_NAME, help="Device name")
@click.option("--window", default=30, help="Analysis window in days")
@click.option("--influxdb-url", default=config.INFLUXDB_URL, help="InfluxDB URL")
@click.option("--influxdb-token", default=config.INFLUXDB_TOKEN, help="InfluxDB token")
@click.option("--influxdb-org", default=config.INFLUXDB_ORG, help="InfluxDB org")
@click.option("--influxdb-bucket", default=config.INFLUXDB_BUCKET, help="InfluxDB bucket")
def health(device: str, window: int, influxdb_url: str, influxdb_token: str, influxdb_org: str, influxdb_bucket: str) -> None:
    """Compute battery health score from voltage history."""
    from .analytics.health_score import BatteryHealthScorer

    with BatteryHealthScorer(url=influxdb_url, token=influxdb_token, org=influxdb_org, bucket=influxdb_bucket) as scorer:
        result = scorer.compute_health_score(device=device, window_days=window)

    if result is None:
        click.echo("Insufficient data for health scoring. Run the gateway longer to collect data.")
        return

    score = result["score"]
    if score >= 80:
        color = "green"
    elif score >= 60:
        color = "yellow"
    else:
        color = "red"

    click.echo("\n  Battery Health Report")
    click.echo("  " + "=" * 40)
    click.echo(f"  Health Score:    {click.style(str(score), fg=color, bold=True)} / 100")
    click.echo(f"  Trend:           {result['trend']}")
    click.echo(f"  Baseline V:      {result['baseline_v']:.4f} V")
    click.echo(f"  Current V:       {result['current_v']:.4f} V")
    click.echo(f"  Variance Ratio:  {result['variance_ratio']:.3f}")
    click.echo(f"  Data Points:     {result['data_points']}")
    click.echo(f"  Computed At:     {result['computed_at']}")
    click.echo()


@analytics.command()
@click.option("--device", default=config.DEVICE_NAME, help="Device name")
@click.option("--window", default=60, help="Analysis window in minutes")
@click.option("--influxdb-url", default=config.INFLUXDB_URL, help="InfluxDB URL")
@click.option("--influxdb-token", default=config.INFLUXDB_TOKEN, help="InfluxDB token")
@click.option("--influxdb-org", default=config.INFLUXDB_ORG, help="InfluxDB org")
@click.option("--influxdb-bucket", default=config.INFLUXDB_BUCKET, help="InfluxDB bucket")
def anomalies(device: str, window: int, influxdb_url: str, influxdb_token: str, influxdb_org: str, influxdb_bucket: str) -> None:
    """Detect anomalies in recent telemetry data."""
    from .analytics.anomaly_detector import AnomalyDetector

    with AnomalyDetector(url=influxdb_url, token=influxdb_token, org=influxdb_org, bucket=influxdb_bucket) as detector:
        voltage_anomalies = detector.detect_voltage_anomalies(device=device, window_minutes=window)
        temp_anomalies = detector.detect_temperature_anomalies(device=device, window_minutes=window)

    all_anomalies = voltage_anomalies + temp_anomalies

    if not all_anomalies:
        click.echo(f"\nNo anomalies detected in the last {window} minutes.")
        return

    click.echo(f"\n  Found {len(all_anomalies)} anomalie(s) in the last {window} minutes:\n")
    for a in all_anomalies:
        severity = a["severity"].upper()
        color = "red" if severity == "CRITICAL" else "yellow"
        click.echo(f"  [{click.style(severity, fg=color)}] {a['timestamp']}: {a['message']}")
    click.echo()


@analytics.command()
@click.option("--device", default=config.DEVICE_NAME, help="Device name")
@click.option("--window", default=90, help="Analysis window in days")
@click.option("--eol-threshold", default=80, help="End-of-life health threshold (%%)")
@click.option("--influxdb-url", default=config.INFLUXDB_URL, help="InfluxDB URL")
@click.option("--influxdb-token", default=config.INFLUXDB_TOKEN, help="InfluxDB token")
@click.option("--influxdb-org", default=config.INFLUXDB_ORG, help="InfluxDB org")
@click.option("--influxdb-bucket", default=config.INFLUXDB_BUCKET, help="InfluxDB bucket")
def rul(device: str, window: int, eol_threshold: int, influxdb_url: str, influxdb_token: str, influxdb_org: str, influxdb_bucket: str) -> None:
    """Estimate remaining useful life from cycle and voltage trends."""
    from .analytics.rul_estimator import RULEstimator

    with RULEstimator(url=influxdb_url, token=influxdb_token, org=influxdb_org, bucket=influxdb_bucket) as estimator:
        result = estimator.estimate_rul(device=device, window_days=window, eol_threshold=eol_threshold)

    if result is None:
        click.echo("Insufficient data for RUL estimation. Need more telemetry with cycle counts.")
        return

    click.echo("\n  Remaining Useful Life Estimate")
    click.echo("  " + "=" * 40)
    click.echo(f"  Status:           {result['status']}")
    click.echo(f"  Current Health:   {result['current_health']}%")
    click.echo(f"  Current Cycles:   {result['current_cycles']}")
    click.echo(f"  EOL Threshold:    {result['eol_threshold']}%")

    if result.get("remaining_cycles") is not None:
        click.echo(f"  Remaining Cycles: {click.style(str(result['remaining_cycles']), bold=True)}")
        click.echo(f"  Cycles at EOL:    {result['cycles_at_eol']}")

    if result.get("slope_mv_per_cycle") is not None:
        click.echo(f"  Slope:            {result['slope_mv_per_cycle']} mV/cycle")

    click.echo(f"  Data Points:      {result['data_points']}")
    click.echo(f"  Computed At:      {result['computed_at']}")
    click.echo()


@analytics.command()
@click.option("--device", default=config.DEVICE_NAME, help="Device name")
@click.option("--window", default=90, help="Analysis window in days")
@click.option("--influxdb-url", default=config.INFLUXDB_URL, help="InfluxDB URL")
@click.option("--influxdb-token", default=config.INFLUXDB_TOKEN, help="InfluxDB token")
@click.option("--influxdb-org", default=config.INFLUXDB_ORG, help="InfluxDB org")
@click.option("--influxdb-bucket", default=config.INFLUXDB_BUCKET, help="InfluxDB bucket")
def cycles(device: str, window: int, influxdb_url: str, influxdb_token: str, influxdb_org: str, influxdb_bucket: str) -> None:
    """Analyze charge cycle patterns — capacity fade, durations, temperature."""
    from .analytics.cycle_analyzer import CycleAnalyzer

    with CycleAnalyzer(url=influxdb_url, token=influxdb_token, org=influxdb_org, bucket=influxdb_bucket) as analyzer:
        result = analyzer.analyze_cycles(device=device, window_days=window)

    if result is None:
        click.echo("Insufficient data for cycle analysis. Need more telemetry history.")
        return

    click.echo("\n  Charge Cycle Analysis")
    click.echo("  " + "=" * 40)
    click.echo(f"  Total Cycles:         {result['total_cycles']}")
    click.echo(f"  Data Points:          {result['data_points']}")
    click.echo(f"  Charging Sessions:    {result['charging_sessions']}")

    if result["avg_charge_duration_min"] is not None:
        click.echo(f"  Avg Charge Duration:  {result['avg_charge_duration_min']} min")

    fade = result["capacity_fade_pct"]
    fade_color = "green" if fade < 5 else ("yellow" if fade < 15 else "red")
    click.echo(f"  Capacity Fade:        {click.style(f'{fade}%', fg=fade_color)}")
    click.echo(f"  Voltage Early Avg:    {result['voltage_early_avg']:.4f} V")
    click.echo(f"  Voltage Late Avg:     {result['voltage_late_avg']:.4f} V")
    click.echo(f"  Temperature Avg:      {result['temperature_avg_c']}°C")
    click.echo(f"  Temperature Range:    {result['temperature_min_c']}°C — {result['temperature_max_c']}°C")
    click.echo(f"  Computed At:          {result['computed_at']}")
    click.echo()


if __name__ == "__main__":
    main()
