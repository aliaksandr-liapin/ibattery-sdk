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


async def scan_for_device(
    name: str = config.DEVICE_NAME,
    timeout: float = config.SCAN_TIMEOUT,
) -> Optional[BLEDevice]:
    """Scan for a BLE device by name.

    Args:
        name: Target device name (e.g., "iBattery").
        timeout: Scan duration in seconds.

    Returns:
        BLEDevice if found, None otherwise.
    """
    logger.info("Scanning for '%s' (timeout %.1fs)...", name, timeout)
    discovered = await BleakScanner.discover(timeout=timeout, return_adv=True)

    for address, (device, adv_data) in discovered.items():
        if device.name and name.lower() in device.name.lower():
            logger.info(
                "Found '%s' at %s (RSSI: %s dBm)",
                device.name,
                device.address,
                adv_data.rssi,
            )
            return device

    logger.warning("Device '%s' not found", name)
    return None


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
