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
    from .influxdb_writer import TelemetryWriter
    from .scanner import connect_and_stream, scan_for_device

    packet_count = 0
    writer = TelemetryWriter(
        url=influxdb_url,
        token=influxdb_token,
        org=influxdb_org,
        bucket=influxdb_bucket,
    )

    def on_packet(data: bytes) -> None:
        nonlocal packet_count
        try:
            decoded = decode_packet(data)
            packet_count += 1
            writer.write(decoded, device_name=device_name)
            click.echo(f"[{packet_count:>5}] {format_packet(decoded)}")
        except Exception as e:
            click.echo(f"[ERROR] {e}", err=True)

    async def _run() -> None:
        device = await scan_for_device(name=device_name, timeout=timeout)
        if device is None:
            click.echo(f"Device '{device_name}' not found. Is it advertising?", err=True)
            sys.exit(1)

        click.echo(f"\nStreaming from {device.name} ({device.address}) → InfluxDB")
        click.echo("Press Ctrl+C to stop.\n")

        await connect_and_stream(device.address, on_packet)

    try:
        asyncio.run(_run())
    except KeyboardInterrupt:
        click.echo(f"\nStopped. Wrote {packet_count} packets to InfluxDB.")
    finally:
        writer.close()


if __name__ == "__main__":
    main()
