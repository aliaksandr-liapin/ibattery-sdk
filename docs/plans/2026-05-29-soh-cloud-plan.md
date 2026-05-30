# SoH Cloud Layer Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Carry on-device State of Health end-to-end — wire v4 → gateway → InfluxDB → Grafana — so SoH is visible on a fleet dashboard.

**Architecture:** Conditional wire v4 (34 bytes = v3 + `soh_pct_x100` uint16 at offset 32); `BATTERY_TELEMETRY_VERSION = 4` only when `CONFIG_BATTERY_SOC_SOH`. Bump BLE MTU to fit 34 bytes. Gateway auto-detects v4 by length and writes a `soh_pct` InfluxDB field; Grafana gets a State-of-Health panel. Back-compatible with v1–v3.

**Tech Stack:** C11 (Zephyr), Unity host tests; Python gateway (bleak/influxdb-client), pytest; Grafana JSON.

**Conventions:** `x100` units throughout. SoH is `uint16` 0..10000 (0.01%). Design: `docs/plans/2026-05-29-soh-cloud-design.md`.

---

### Task 1: Wire v4 — packet field, sizes, conditional version

**Files:**
- Modify: `include/battery_sdk/battery_types.h`
- Modify: `src/transport/battery_serialize.h`

**Step 1: Add the v4 field + conditional version** in `battery_types.h`.

Replace `#define BATTERY_TELEMETRY_VERSION 3U` with:

```c
#if defined(CONFIG_BATTERY_SOC_SOH)
#define BATTERY_TELEMETRY_VERSION 4U
#else
#define BATTERY_TELEMETRY_VERSION 3U
#endif
```

Add the field at the end of `struct battery_telemetry_packet` (after `coulomb_mah_x100`):

```c
    /* v4 field — zero when telemetry_version < 4 */
    uint16_t soh_pct_x100;
```

**Step 2: Add v4 size + wire_size** in `battery_serialize.h`:

```c
#define BATTERY_SERIALIZE_V4_SIZE 34
```
Change `#define BATTERY_SERIALIZE_BUF_SIZE BATTERY_SERIALIZE_V3_SIZE` to `... BATTERY_SERIALIZE_V4_SIZE`.
In `battery_serialize_wire_size`, add as the first check:
```c
    if (version >= 4) return BATTERY_SERIALIZE_V4_SIZE;
```

**Step 3: Commit** (compiles; behavior unchanged until pack/unpack handle it)

```bash
git add include/battery_sdk/battery_types.h src/transport/battery_serialize.h
git commit -m "feat(v4): add soh_pct_x100 packet field, V4 size, conditional version"
```

---

### Task 2: Serialize/deserialize v4 (TDD)

**Files:**
- Modify: `tests/test_serialize.c`
- Modify: `src/transport/battery_serialize.c`

**Step 1: Write failing tests** — add to `tests/test_serialize.c`. First a v4 packet helper (after `make_v3_packet`):

```c
static struct battery_telemetry_packet make_v4_packet(void)
{
    struct battery_telemetry_packet pkt = make_v3_packet();
    pkt.telemetry_version = 4;
    pkt.soh_pct_x100 = 8750;  /* 87.50% */
    return pkt;
}
```

Tests (and register each in `main` with `RUN_TEST`):

```c
void test_wire_size_v4_is_34(void)
{
    TEST_ASSERT_EQUAL_UINT8(34, battery_serialize_wire_size(4));
}

void test_v4_roundtrip(void)
{
    struct battery_telemetry_packet src = make_v4_packet();
    struct battery_telemetry_packet dst;
    uint8_t buf[BATTERY_SERIALIZE_BUF_SIZE];
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_serialize_pack(&src, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_serialize_unpack(buf, 34, &dst));
    TEST_ASSERT_EQUAL_UINT8(4, dst.telemetry_version);
    TEST_ASSERT_EQUAL_UINT16(8750, dst.soh_pct_x100);
    /* v3 fields still intact */
    TEST_ASSERT_EQUAL_INT32(src.current_ma_x100, dst.current_ma_x100);
    TEST_ASSERT_EQUAL_INT32(src.coulomb_mah_x100, dst.coulomb_mah_x100);
}

void test_pack_v4_buffer_too_small(void)
{
    struct battery_telemetry_packet pkt = make_v4_packet();
    uint8_t buf[33];  /* one short of 34 */
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_INVALID_ARG,
                          battery_serialize_pack(&pkt, buf, sizeof(buf)));
}

void test_v3_unpack_leaves_soh_zero(void)
{
    /* A 32-byte v3 buffer must decode with soh_pct_x100 == 0. */
    struct battery_telemetry_packet src = make_v3_packet();
    struct battery_telemetry_packet dst;
    uint8_t buf[BATTERY_SERIALIZE_BUF_SIZE];
    battery_serialize_pack(&src, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(BATTERY_STATUS_OK,
                          battery_serialize_unpack(buf, 32, &dst));
    TEST_ASSERT_EQUAL_UINT16(0, dst.soh_pct_x100);
}
```

**Step 2: Run — verify fail**
```bash
export PATH="/opt/homebrew/bin:/usr/bin:/bin:/opt/nordic/ncs/toolchains/e5f4758bcf/bin:$PATH"
cd tests && cmake -S . -B build && cmake --build build --target test_serialize && ./build/test_serialize
```
Expected: v4 tests FAIL (soh not packed; wire_size already returns 34 from Task 1 so that one passes).

**Step 3: Implement** in `battery_serialize.c`. In `pack`, after the v3 block:

```c
    /* v4 extension */
    if (pkt->telemetry_version >= 4) {
        put_u16_le(&buf[32], pkt->soh_pct_x100);
    }
```

In `unpack`, after the v3 block:

```c
    /* v4 extension */
    if (pkt->telemetry_version >= 4 && buf_len >= BATTERY_SERIALIZE_V4_SIZE) {
        pkt->soh_pct_x100 = get_u16_le(&buf[32]);
    } else {
        pkt->soh_pct_x100 = 0;
    }
```

**Step 4: Run — verify pass** (`./build/test_serialize`), then full `ctest --test-dir build` (no regressions).

**Step 5: Commit**
```bash
git add tests/test_serialize.c src/transport/battery_serialize.c
git commit -m "feat(v4): pack/unpack soh_pct_x100 at offset 32 (TDD green)"
```

---

### Task 3: Populate SoH in telemetry assembly

**Files:** Modify `src/telemetry/battery_telemetry.c`

**Step 1:** Add a guarded include near the top includes:

```c
#if defined(CONFIG_BATTERY_SOC_SOH)
#include <battery_sdk/battery_soh.h>
#endif
```

After the `#endif` that closes the `CONFIG_BATTERY_CURRENT_SENSE` (v3) block, add:

```c
    /* State of Health — best-effort (v4) */
#if defined(CONFIG_BATTERY_SOC_SOH)
    (void)battery_soh_get_pct_x100(&packet->soh_pct_x100);
#endif
```

(`packet->telemetry_version` is already `BATTERY_TELEMETRY_VERSION`, which is 4 when SoH is enabled — set at the top of `battery_telemetry_collect`.)

**Step 2: Build the existing telemetry test** to confirm no break (it doesn't define `CONFIG_BATTERY_SOC_SOH`, so the block compiles out):
```bash
cd tests && cmake --build build --target test_telemetry && ./build/test_telemetry
```
Expected: PASS unchanged.

**Step 3: Commit**
```bash
git add src/telemetry/battery_telemetry.c
git commit -m "feat(v4): emit soh_pct_x100 in telemetry when SoH enabled"
```

---

### Task 4: BLE MTU for 34-byte v4

**Files:** Modify `app/boards/nucleo_l476rg_ble_current.conf`, `app/prj.conf`, `app/boards/esp32c3_devkitm.conf`

**Step 1:** In each, change the MTU trio (currently 35/39/39) to:

```conf
# v4 telemetry is 34 bytes; a notification needs ATT_MTU >= 34 + 3 (ATT header).
CONFIG_BT_L2CAP_TX_MTU=37
CONFIG_BT_BUF_ACL_TX_SIZE=41
CONFIG_BT_BUF_ACL_RX_SIZE=41
```
Update the existing explanatory comment above it to reference v4/34 bytes.

**Step 2: Commit**
```bash
git add app/boards/nucleo_l476rg_ble_current.conf app/prj.conf app/boards/esp32c3_devkitm.conf
git commit -m "fix(ble): MTU 35->37 / ACL 39->41 to carry 34-byte v4 notifications"
```

---

### Task 5: Gateway decoder v4 (TDD)

**Files:** Modify `gateway/gateway/decoder.py`, `gateway/tests/test_decoder.py`

**Step 1: Write failing test** in `gateway/tests/test_decoder.py` (mirror an existing v3 test; build a 34-byte buffer):

```python
import struct
from gateway.decoder import decode_packet, WIRE_SIZE_V4

def test_decode_v4_includes_soh():
    # v3 layout + uint16 soh (8750 = 87.50%)
    data = struct.pack("<BIiiHBIIiiH",
        4, 1000, 3700, 2500, 8500, 2, 0,  # version..flags
        5,                                  # cycle_count
        280, 22000,                         # current_ma_x100, coulomb_mah_x100
        8750)                               # soh_pct_x100
    assert len(data) == WIRE_SIZE_V4 == 34
    out = decode_packet(data)
    assert out["version"] == 4
    assert out["soh_pct"] == 87.5
    assert out["coulomb_mah"] == 220.0  # v3 fields still decoded

def test_decode_v3_soh_defaults_zero():
    data = struct.pack("<BIiiHBIIii", 3, 1000, 3700, 2500, 8500, 2, 0, 5, 280, 22000)
    out = decode_packet(data)
    assert out["soh_pct"] == 0.0
```

**Step 2: Run — verify fail**
```bash
cd gateway && /Users/aliapin/.pyenv/versions/3.11.14/bin/python3 -m pytest tests/test_decoder.py -q
```
Expected: FAIL (`WIRE_SIZE_V4` undefined / 34 not handled).

**Step 3: Implement** in `decoder.py`:
- Add `WIRE_SIZE_V4 = 34`, `_WIRE_FMT_V4 = "<BIiiHBIIiiH"`, `_WIRE_STRUCT_V4 = struct.Struct(_WIRE_FMT_V4)`.
- Add a `soh_x100` variable defaulting to 0 in the v1/v2/v3 branches.
- Add the v4 branch FIRST in `decode_packet`:
```python
    if len(data) == WIRE_SIZE_V4:
        (version, ts_ms, mv, temp_x100, soc_x100, ps, flags,
         cycles, current_x100, coulomb_x100, soh_x100) = _WIRE_STRUCT_V4.unpack(data)
    elif len(data) == WIRE_SIZE_V3:
        (...) = _WIRE_STRUCT_V3.unpack(data)
        soh_x100 = 0
    # ... v2/v1 also set soh_x100 = 0
```
- Update the error message to include `WIRE_SIZE_V4`.
- Add to the returned dict: `"soh_pct": soh_x100 / 100.0,`.

**Step 4: Run — verify pass**, then full gateway suite:
```bash
cd gateway && /Users/aliapin/.pyenv/versions/3.11.14/bin/python3 -m pytest -q
```
Expected: all pass (was 73; +2).

**Step 5: Commit**
```bash
git add gateway/gateway/decoder.py gateway/tests/test_decoder.py
git commit -m "feat(gateway): decode 34-byte v4 packets with soh_pct"
```

---

### Task 6: InfluxDB writer + Grafana panel

**Files:** Modify `gateway/gateway/influxdb_writer.py`, `gateway/grafana/ibattery-dashboard.json`

**Step 1:** In `influxdb_writer.py`, add a field to the `Point` chain (after `coulomb_mah`):
```python
            .field("soh_pct", decoded.get("soh_pct", 0.0))
```

**Step 2:** If `gateway/tests/test_writer.py` asserts the encoded point's fields, add a `soh_pct` assertion mirroring the `coulomb_mah` one; run `pytest tests/test_writer.py -q`.

**Step 3:** In `gateway/grafana/ibattery-dashboard.json`, add a timeseries panel modeled on the existing "Remaining Charge (mAh)" panel: duplicate that panel object, give it a new unique `id`, title `"State of Health (%)"`, unit `percent` (0–100), and a Flux/InfluxQL query selecting the `soh_pct` field from `battery_telemetry` (copy the existing panel's query and swap the field name). Place it after the Remaining Charge panel (adjust `gridPos.y`). Validate JSON parses:
```bash
python3 -c "import json; json.load(open('gateway/grafana/ibattery-dashboard.json')); print('json ok')"
```

**Step 4: Commit**
```bash
git add gateway/gateway/influxdb_writer.py gateway/grafana/ibattery-dashboard.json gateway/tests/test_writer.py
git commit -m "feat(cloud): persist soh_pct to InfluxDB + Grafana State of Health panel"
```

---

### Task 7: Firmware build, hardware E2E, docs

**Files:** Modify `docs/RELEASE_NOTES.md`, `CLAUDE.md` (wire-format section), `docs/SDK_API.md` (wire format note)

**Step 1: Compile-check** the v4 firmware:
```bash
export PATH="/opt/homebrew/bin:/usr/bin:/bin:/opt/nordic/ncs/toolchains/e5f4758bcf/bin:$PATH"
export ZEPHYR_BASE="/opt/nordic/ncs/v3.2.2/zephyr"
export ZEPHYR_SDK_INSTALL_DIR="/opt/nordic/ncs/toolchains/e5f4758bcf/opt/zephyr-sdk"
west build -b nucleo_l476rg app -d /tmp/build-soh-v4 --pristine -- \
  -DSHIELD=x_nucleo_idb05a1 \
  -DEXTRA_CONF_FILE=boards/nucleo_l476rg_ble_current.conf \
  -DCONFIG_BATTERY_SOC_SOH=y \
  -DZEPHYR_EXTRA_MODULES="/opt/nordic/ncs/v3.2.2/modules/hal/stm32"
```
Expected: exit 0. Confirm `.config` has `CONFIG_BT_L2CAP_TX_MTU=37`.

**Step 2: Hardware E2E** (controller-run; BLE from iTerm per CLAUDE.md): flash, bring up the docker stack, run `ibattery-gateway run` in iTerm, and verify via InfluxDB that a `soh_pct` field arrives on the `battery_telemetry` measurement (expected value 100 until a deep discharge cycle) and the Grafana "State of Health" panel renders. Capture a short evidence log to `docs/captures/`.

**Step 3: Docs** — update the Wire Format section in `CLAUDE.md` and `docs/SDK_API.md` to add the v4 row (34 bytes, `soh_pct_x100` at offset 32); add a RELEASE_NOTES "Unreleased" line folding this into the Phase 8d entry (now end-to-end). Note the gateway is back-compatible v1–v4.

**Step 4: Commit**
```bash
git add docs/RELEASE_NOTES.md CLAUDE.md docs/SDK_API.md docs/captures/
git commit -m "docs(v4): document wire v4 / SoH cloud path end-to-end"
```

---

## Out of scope (per design)

NVS persistence, partial-excursion learning, current-offset auto-calibration. No version tag/release (separate decision).

## Definition of done

- Host suites green (serialize v4 + back-compat); gateway suites green (decoder v4 + writer).
- SoH-off firmware byte-identical (still v3); v1–v3 still decode.
- v4 firmware compiles with MTU 37; hardware E2E shows `soh_pct` in InfluxDB + Grafana.
- Docs updated. No release.
