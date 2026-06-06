"""Tests for BLE advertisement matching logic.

Regression coverage for matching the iBattery device by its advertised
service UUID rather than by GAP name. On macOS, CoreBluetooth often leaves
the name empty on a given advertisement packet, so name-only matching missed
a device that was advertising correctly. The service UUID is the reliable
discriminator.
"""

from types import SimpleNamespace

from gateway import config
from gateway.scanner import matches_ibattery

OTHER_UUID = "0000feaf-0000-1000-8000-00805f9b34fb"


def _dev(name, address="AA"):
    return SimpleNamespace(name=name, address=address)


def _adv(service_uuids, local_name=None):
    return SimpleNamespace(service_uuids=service_uuids, local_name=local_name)


def test_matches_by_service_uuid_when_name_missing():
    # macOS case: no name on this packet, but service UUID is advertised.
    assert matches_ibattery(_dev(None), _adv([config.SERVICE_UUID.upper()]))


def test_matches_service_uuid_case_insensitive():
    assert matches_ibattery(_dev(None), _adv([config.SERVICE_UUID.lower()]))


def test_falls_back_to_name_when_no_service_uuid():
    assert matches_ibattery(_dev("iBattery-STM32"), _adv([]))


def test_matches_name_from_adv_local_name():
    # Name only present in the advertisement's local_name, not device.name.
    assert matches_ibattery(_dev(None), _adv([], local_name="iBattery-STM32"))


def test_no_match_for_unrelated_device():
    assert not matches_ibattery(_dev("RandomDevice"), _adv([OTHER_UUID]))


def test_no_match_when_no_name_and_no_uuid():
    assert not matches_ibattery(_dev(None), _adv(None))


# ── Device-tag resolution ─────────────────────────────────────────────────────
# The InfluxDB `device` tag (shown on the Grafana "Connected Device" tile) must
# reflect the BOARD that is actually streaming — i.e. its advertised BLE name
# (iBattery-STM32 / iBattery-nRF52840 / iBattery-ESP32C3) — not the gateway's
# static filter/default. Falls back to the provided default when the peripheral
# advertises no usable name (common on macOS CoreBluetooth).
from gateway.scanner import resolve_device_tag


def test_resolve_device_tag_prefers_advertised_name():
    assert resolve_device_tag("iBattery-nRF52840", "iBattery") == "iBattery-nRF52840"


def test_resolve_device_tag_falls_back_when_name_missing():
    assert resolve_device_tag(None, "iBattery") == "iBattery"


def test_resolve_device_tag_falls_back_on_empty_name():
    assert resolve_device_tag("", "iBattery") == "iBattery"
