"""BLE scanner and notification subscriber using bleak.

Connects to the iBattery device and streams telemetry notifications.
"""

import asyncio
import logging
from dataclasses import dataclass
from typing import Callable, Optional

from bleak import BleakClient, BleakScanner
from bleak.backends.device import BLEDevice

from . import config

logger = logging.getLogger(__name__)


@dataclass
class DiscoveredDevice:
    """BLE device with advertisement data (name, address, RSSI)."""

    device: BLEDevice
    rssi: int

    @property
    def name(self) -> Optional[str]:
        return self.device.name

    @property
    def address(self) -> str:
        return self.device.address


def matches_ibattery(
    device: BLEDevice,
    adv_data,
    name: str = config.DEVICE_NAME,
    service_uuid: str = config.SERVICE_UUID,
) -> bool:
    """Return True if an advertisement is the iBattery device.

    Matches on the advertised service UUID first, then falls back to a name
    substring match. The service UUID lives in the primary advertisement and
    is the most reliable discriminator on macOS, where CoreBluetooth populates
    the GAP name asynchronously and leaves it empty on many packets.
    """
    uuids = [u.lower() for u in (adv_data.service_uuids or [])]
    if service_uuid.lower() in uuids:
        return True
    adv_name = getattr(adv_data, "local_name", None) or device.name
    return bool(adv_name and name.lower() in adv_name.lower())


async def scan_for_device(
    name: str = config.DEVICE_NAME,
    timeout: float = config.SCAN_TIMEOUT,
    service_uuid: str = config.SERVICE_UUID,
) -> Optional[BLEDevice]:
    """Scan for the iBattery device by service UUID (name as fallback).

    Uses find_device_by_filter, which evaluates every advertisement packet as
    it arrives and returns on the first match. This is robust against the
    BleakScanner.discover() snapshot on macOS, where a device's service_uuids
    is intermittently empty in the final merged result depending on whether an
    advertisement or scan-response packet landed last.

    Args:
        name: Target device name substring (e.g., "iBattery").
        timeout: Scan duration in seconds.
        service_uuid: Advertised primary service UUID to match on.

    Returns:
        BLEDevice if found, None otherwise.
    """
    logger.info(
        "Scanning for iBattery (service %s / name '%s', timeout %.1fs)...",
        service_uuid,
        name,
        timeout,
    )
    device = await BleakScanner.find_device_by_filter(
        lambda d, adv: matches_ibattery(d, adv, name, service_uuid),
        timeout=timeout,
    )

    if device is None:
        logger.warning("Device not found (service %s / name '%s')", service_uuid, name)
    else:
        logger.info("Found iBattery at %s (name=%r)", device.address, device.name)
    return device


async def list_nearby_devices(
    timeout: float = config.SCAN_TIMEOUT,
) -> list[DiscoveredDevice]:
    """List all nearby BLE devices sorted by RSSI (strongest first).

    Args:
        timeout: Scan duration in seconds.

    Returns:
        List of DiscoveredDevice with RSSI information.
    """
    logger.info("Scanning for nearby BLE devices (%.1fs)...", timeout)
    discovered = await BleakScanner.discover(timeout=timeout, return_adv=True)
    results = [
        DiscoveredDevice(device=device, rssi=adv_data.rssi)
        for _address, (device, adv_data) in discovered.items()
    ]
    return sorted(results, key=lambda d: d.rssi or -999, reverse=True)


async def connect_and_stream(
    address: str,
    on_packet: Callable[[bytes], None],
    char_uuid: str = config.CHAR_UUID,
) -> None:
    """Connect to a BLE device and stream notification packets.

    Automatically reconnects with exponential backoff on disconnection.

    Args:
        address: BLE device address.
        on_packet: Callback receiving raw 20-byte notification data.
        char_uuid: Characteristic UUID to subscribe to.
    """
    delay = config.RECONNECT_DELAY_INITIAL
    stop_event = asyncio.Event()

    while not stop_event.is_set():
        try:
            logger.info("Connecting to %s...", address)

            def on_disconnect(client: BleakClient) -> None:
                logger.warning("Disconnected from %s", address)

            async with BleakClient(address, disconnected_callback=on_disconnect) as client:
                logger.info("Connected to %s", address)
                delay = config.RECONNECT_DELAY_INITIAL  # Reset backoff

                def notification_handler(sender, data: bytearray) -> None:
                    on_packet(bytes(data))

                await client.start_notify(char_uuid, notification_handler)
                logger.info("Subscribed to notifications on %s", char_uuid)

                # Stay connected until disconnected
                while client.is_connected:
                    await asyncio.sleep(1.0)

        except asyncio.CancelledError:
            logger.info("Stream cancelled")
            break
        except Exception:
            logger.exception("Connection error")

        # Reconnect with exponential backoff
        logger.info("Reconnecting in %.1fs...", delay)
        try:
            await asyncio.sleep(delay)
        except asyncio.CancelledError:
            break
        delay = min(delay * config.RECONNECT_BACKOFF_FACTOR, config.RECONNECT_DELAY_MAX)
