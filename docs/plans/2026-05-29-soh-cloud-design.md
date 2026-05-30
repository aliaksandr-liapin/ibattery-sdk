# SoH Cloud Layer (wire v4 + gateway + Grafana) Design

**Status:** Approved design (brainstorm) — not yet implemented
**Date:** 2026-05-29
**Depends on:** Phase 8d on-device SoH MVP (`CONFIG_BATTERY_SOC_SOH`, merged to main)

## Overview

The Phase 8d MVP computes State of Health on-device but keeps it in RAM — it
never leaves the chip. This layer makes SoH *visible*: it travels firmware →
BLE → gateway → InfluxDB → Grafana, so a fleet dashboard can answer "which
devices need battery replacement?". This is the monetizable surface (ROADMAP
SaaS fleet monitoring / commercial license); the on-device math is only
valuable once it reaches a dashboard. The work mirrors the v0.8.5 pattern that
added `current_ma`/`coulomb_mah` end-to-end.

## Wire format v4

`v4 = 34 bytes = v3 (32) + soh_pct_x100 (uint16 LE at offset 32)`.

```
Offset  Size  Field
 0-31    32   v3 (version..coulomb_mah_x100)
 32      2    soh_pct_x100   (uint16 LE, 0..10000)
 ──     34    Total
```

- `battery_serialize.h`: `BATTERY_SERIALIZE_V4_SIZE 34`; `BUF_SIZE → 34`;
  `wire_size(version >= 4) → 34`.
- `battery_serialize.c`: pack/unpack `soh_pct_x100` when `version >= 4`
  (otherwise the field stays 0, as v2/v3 fields already do).
- `battery_types.h`: add `uint16_t soh_pct_x100;` (v4 field, zero when
  `telemetry_version < 4`).

### Conditional version

`BATTERY_TELEMETRY_VERSION = 4` iff `CONFIG_BATTERY_SOC_SOH`, else `3`. This
diverges from the existing unconditional-v3 constant deliberately: SoH-off
builds stay byte-identical to today (v3, 32 bytes, no MTU change), and the
gateway already auto-detects format by length. `battery_telemetry.c`
populates `soh_pct_x100` from `battery_soh_get_pct_x100()` under the same
`#if defined(CONFIG_BATTERY_SOC_SOH)` guard. (SoH depends on SOC_COULOMB
depends on CURRENT_SENSE, so a v4 packet always has meaningful v3 fields too.)

## BLE MTU

A 34-byte notification needs `ATT_MTU >= 34 + 3 = 37`. Bump
`CONFIG_BT_L2CAP_TX_MTU` 35 → **37** and ACL buffers 39 → **41** in the BLE
configs. `nucleo_l476rg_ble_current.conf` is the one paired with SoH and must
change; `prj.conf` (nRF) and `esp32c3_devkitm.conf` are bumped too for
consistency (37 still carries v3's 32-byte payload). This is the same class of
fix as the v3 MTU bug — sized to the actual max payload, not under it.

## Gateway

- `decoder.py`: `WIRE_SIZE_V4 = 34`, `_WIRE_FMT_V4 = "<BIiiHBIIiiH"` (v3 + a
  trailing `uint16` SoH), decode `soh_pct = soh_x100 / 100.0`. The decoder
  already branches on length (20/24/32); add the 34 branch. v1–v3 unchanged.
- `influxdb_writer.py`: add a `soh_pct` field to the `battery_telemetry` point
  (`decoded.get("soh_pct", ...)`, defaulting so older packets are fine).

## Grafana

Add a **"State of Health (%)"** timeseries panel to
`gateway/grafana/ibattery-dashboard.json`, querying the new `soh_pct` field
(0–100). Timeseries chosen over a gauge because capacity *fade over time* is
the predictive-maintenance view. Existing panels are untouched.

## Testing

- **Host — `tests/test_serialize.c`:** v4 round-trip (pack then unpack
  preserves `soh_pct_x100`); v1/v2/v3 still round-trip (back-compat);
  `wire_size(4) == 34`.
- **Gateway — `tests/test_decoder.py`:** a 34-byte v4 buffer decodes with the
  expected `soh_pct`; 20/24/32-byte buffers still decode unchanged; a
  wrong-length buffer still raises.
- **Hardware E2E:** flash the BLE+current build with `CONFIG_BATTERY_SOC_SOH=y`
  (MTU now 37), confirm a 34-byte v4 packet reaches the gateway and `soh_pct`
  lands in InfluxDB and renders in the Grafana panel. SoH reads 100% until a
  deep discharge cycle — expected and documented, not a failure.

## Back-compat & scope

- Gateway accepts v1–v4 by length; SoH-off firmware is byte-identical to today.
- **Out of scope (unchanged from the 8d design):** NVS persistence,
  partial-excursion learning, current-offset auto-calibration.

## References

- `docs/plans/2026-05-29-phase-8d-soh-design.md` — on-device SoH (this layer's
  source of the value).
- v0.8.5 in `docs/RELEASE_NOTES.md` — the `current_ma`/`coulomb_mah` cloud
  precedent this mirrors.
- `src/transport/battery_serialize.{h,c}`, `gateway/gateway/decoder.py`,
  `gateway/gateway/influxdb_writer.py`, `gateway/grafana/ibattery-dashboard.json`.
