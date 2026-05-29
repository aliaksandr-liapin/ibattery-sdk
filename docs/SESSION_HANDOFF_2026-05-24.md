# Session Handoff — Phase 8a Hardware Validation

**Date:** 2026-05-24
**Last release:** v0.8.2 (shipped, tagged, GitHub release published)
**Status:** Hardware validation **partially complete** — chip ACKs proven on
the wire (logic-analyzer captured), stable firmware-level reads pending
a permanent connection.

This document captures the full context of tonight's hardware bring-up
session so a fresh chat / future-you can pick it up cleanly.

---

## The state right now

### What is conclusively verified

1. **Adafruit INA219 (genuine, #904) responds at I2C address 0x40 on nRF52840-DK.**
   Logic-analyzer captures decoded the full transaction:
   ```
   Start
   Address write: 40
   ACK            ← chip pulls SDA low
   Stop
   ```
   This was reproduced multiple times (3 ACKs in one capture window).

2. **The nRF52840-DK I2C master (TWIM0) drives the bus correctly.**
   START, address bits, R/W, clock, and STOP all decoded as expected on
   both SDA (P0.26) and SCL (P0.27).

3. **The SDK firmware path works.** Both the `build-nrf-current` (real SDK
   with INA219 HAL) and `build-nrf-scan` (with `CONFIG_I2C_SHELL=y` for
   diagnostics) generate correct I2C traffic at boot / on command.

4. **Bus pull-ups work.** Both SDA and SCL idle at 3.3V (the Adafruit board's
   onboard 10kΩ pull-ups are present and powered).

5. **The chip has clean power.** Adafruit "on" LED is solid green; VCC is
   3.3V from the nRF VDD pin.

### What is NOT yet stable

**Firmware-level `i2c scan` does NOT consistently report 0x40**, even though
the analyzer captures clean ACKs on the wire in the same scans. Across
multiple test configurations tonight, the shell scan returned "0 devices
found on i2c@40003000" essentially every time, while the analyzer occasionally
caught the ACK.

This is the **signature of a marginal/intermittent physical connection**:
the chip pulls SDA low, the analyzer (continuous sampling, high input
impedance) catches the low, but the nRF's I2C peripheral samples SDA at a
specific clock-phase instant and reads it as high (NACK), because the
voltage at the nRF pin doesn't drop low enough at the sampling moment.

The most likely cause is **series resistance from worn breadboard contacts**
on the SDA path. Worn contacts add 100Ω-several kΩ of resistance, which
combined with bus capacitance slows the falling edge of the chip's ACK
pulldown enough that the nRF samples too early.

---

## Everything tried tonight (so we don't repeat it)

These were all attempted with the same result (analyzer sees ACK occasionally,
firmware reads NACK consistently):

| Attempt | Outcome |
|---------|---------|
| Re-seat all 4 jumper wires (same holes) | No change |
| Press wires/board down firmly | No change |
| Move analyzer probes (verify probes work) | Probes confirmed good (one was broken — green wire); amber + CH4 probe are good |
| Swap SDA/SCL at nRF side (rule out swap) | No change |
| Try second HiLetgo INA219 chip | No change |
| Try genuine Adafruit INA219 (#904) | No change in shell scan, but **analyzer captured 3 clean ACKs** in one window |
| Remove analyzer probes (rule out probe loading) | No change |
| Add dedicated GND wire (rule out ground issue) | No change |
| Try direct female-to-male jumpers (no breadboard) | No change |
| Fresh reflash of firmware | No change |

The one thing that **DID** produce ACKs (briefly) was the **breadboard
column-alignment fix**: discovering one I2C jumper was in a different
breadboard column than its INA219 pin. Re-aligning the wire to share the
INA219 pin's exact column produced the first ACKs. The connection then
degraded back to flaky.

---

## Critical diagnostic: how to know it's working

When the chip is properly connected:

1. **Analyzer idle**: both SDA and SCL show flat HIGH (3.3V, pulled up)
2. **Analyzer during scan**: both lines show clock/data transitions
   (look for hundreds of transition-blocks on each)
3. **I2C decode**: shows `Address write: 40` followed by `ACK` (not NACK)
4. **Shell `i2c scan i2c@40003000`**: reports `1 devices found` and shows
   `40:` row with the address displayed instead of `--`

The shell scan being reliable across **6 of 6 consecutive scans** is the
acceptance criterion for "stable connection achieved."

---

## Tools and commands for the next session

### Hardware setup (when reconnecting)

```
INA219 pin    →  nRF52840-DK pin    →  Wire color (suggested)
─────────────────────────────────────────────────────────────
VCC           →  VDD (power header) →  red
GND           →  GND (power header) →  black
SCL           →  P0.27              →  green
SDA           →  P0.26              →  yellow
```

Also keep a dedicated short GND jumper from INA219 GND directly to a
nRF GND pin on the GPIO header.

### Firmware builds

Two firmwares exist (both for nRF52840-DK):

```bash
# Setup
export PATH="/opt/homebrew/bin:/usr/bin:/bin:/opt/nordic/ncs/toolchains/e5f4758bcf/bin:$PATH"
export ZEPHYR_BASE="/opt/nordic/ncs/v3.2.2/zephyr"
export ZEPHYR_SDK_INSTALL_DIR="/opt/nordic/ncs/toolchains/e5f4758bcf/opt/zephyr-sdk"

# Diagnostic firmware (i2c shell, for scan command)
west build -b nrf52840dk/nrf52840 \
    /Users/aliapin/Downloads/project/ibattery-sdk/app \
    -d /tmp/build-nrf-scan --pristine -- \
    -DEXTRA_CONF_FILE=boards/nrf52840dk_nrf52840_i2cscan.conf
west flash -d /tmp/build-nrf-scan --runner jlink

# Real SDK firmware (current sensing, coulomb counting)
west build -b nrf52840dk/nrf52840 \
    /Users/aliapin/Downloads/project/ibattery-sdk/app \
    -d /tmp/build-nrf-current --pristine -- \
    -DEXTRA_CONF_FILE=boards/nrf52840dk_nrf52840_current.conf
west flash -d /tmp/build-nrf-current --runner jlink
```

### Quick I2C scan via shell

```python
# /Users/aliapin/.pyenv/versions/3.11.14/bin/python3
import serial, time, re
ser = serial.Serial('/dev/cu.usbmodem0010502585571', 115200, timeout=2)
time.sleep(1); ser.read(ser.in_waiting or 1)
ser.write(b'\r\ni2c scan i2c@40003000\r\n')
time.sleep(3)
data = ser.read(8192)
text = re.sub(r'\x1b\[[0-9;]*[mGKHJ]|\x08', '', data.decode('utf-8','replace'))
for line in text.split('\n'):
    if 'devices found' in line or line.strip().startswith('40:'):
        print(line.strip())
ser.close()
```

### Logic analyzer capture (if needed again)

```bash
# Probes: D3=SDA, D4=SCL (the GND analyzer probe goes to circuit GND;
# we found amber wire on CH3 and a separate probe on CH4 to be working;
# the green/yellow/red/white wires in the HiLetgo analyzer ribbon were
# found to have broken crimps)

TS=$(date +%H%M%S)
sigrok-cli --driver fx2lafw --config samplerate=2000000 \
    --channels D3,D4 --samples 16000000 \
    --output-file /tmp/cap-${TS}.sr --output-format srzip

# Decode
sigrok-cli -i /tmp/cap-${TS}.sr -P i2c:scl=D4:sda=D3 2>&1 | head -50
```

### Counting transitions (to confirm both lines active)

```bash
sigrok-cli -i /tmp/cap.sr -O bits 2>&1 | grep "^D3:" | \
    grep -vcE "(0{8} ){7}0{8}$|(1{8} ){7}1{8}$"
# Returns the number of "transition-blocks" — should be > 0 on both lines
# during an active scan
```

---

## The exact procedure to finish Phase 8a (when hardware is stable)

Once the wiring is solid (soldered or fresh breadboard) and `i2c scan`
reliably reports `0x40` across 6 of 6 scans:

1. **Flash the real SDK firmware:**
   ```bash
   west flash -d /tmp/build-nrf-current --runner jlink
   ```

2. **Open serial monitor:**
   ```bash
   screen /dev/cu.usbmodem0010502585571 115200
   ```

3. **Expected output:** Telemetry lines should now show
   - `I=x.xx mA` (live current, was always 0.00 before)
   - `Q=x.xx mAh` (coulomb accumulator increasing)
   - `flags=0x00000000` (no error flags; previously was `0x00000020` =
     CURRENT_ERR)

4. **Connect a load through INA219 VIN+ / VIN-** (e.g., the LiPo to the
   nRF) and watch current change with load.

5. **Capture a successful I2C trace** with the logic analyzer
   (`tools/i2c-analyzer/capture.sh`) showing
   `START → 0x40 → ACK → register → ACK → data → ACK → STOP` —
   this is the gold-standard evidence for the v0.8.3 release notes.

6. **Run the full host test suite** to confirm nothing regressed:
   ```bash
   cd /Users/aliapin/Downloads/project/ibattery-sdk/tests
   rm -rf build && mkdir build && cd build && cmake .. && make -j4 && ctest
   ```

7. **Ship v0.8.3** with the full live-reading capture:
   - Update `docs/RELEASE_NOTES.md` v0.8.3 entry
   - Update `README.md` and `docs/ROADMAP.md` — Phase 8a fully done
   - Commit, `git tag -a v0.8.3 -m "..."`, push, `gh release create v0.8.3`

8. **Then begin Phase 8c** (Kalman filter fusion) with real current data
   to tune against.

---

## Lessons captured for the next session

1. **Breadboard column alignment is the silent killer.** A wire that
   *looks* connected to a peripheral pin may be in a different breadboard
   column entirely. Verify with the logic analyzer: a line that idles HIGH
   (has a pull-up = peripheral pin present) but shows 0 master transitions
   is the smoking gun for misalignment.

2. **"Analyzer sees ACK, firmware reads NACK" = signal integrity.** Not a
   contradiction; it's the classic symptom of slow edges from contact
   resistance or bus capacitance. The analyzer's continuous sampling
   catches what the MCU's one-shot sample misses.

3. **Cheap analyzer probe ribbons have ~50% bad wires.** From the HiLetgo
   USB Logic Analyzer pack used here, the red/green/yellow/white wires
   had broken internal crimps. Amber and one other were good. Always
   verify each probe on a known signal before trusting it.

4. **Bench breadboard contacts wear out fast** with repeated plug/unplug
   cycles. After a day of debugging in the same holes, expect intermittent
   contact. Solder or fresh holes are the only reliable fixes.

5. **The nRF52840 TWIM minimum I2C speed is 100 kHz.** There is no slower
   software setting on this MCU; signal-integrity fixes must be physical
   (stronger pull-ups, shorter wires, soldered joints, lower capacitance).

---

## What's already in the repo (no need to redo)

- `tools/i2c-analyzer/capture.sh` — sigrok-cli wrapper
- `tools/i2c-analyzer/README.md` — usage guide
- `docs/HARDWARE_TROUBLESHOOTING.md` — 4-phase debug methodology + column
  alignment lesson + signal-integrity signature
- `docs/RELEASE_NOTES.md` — v0.8.0/v0.8.1/v0.8.2 entries with full context
- `app/boards/nrf52840dk_nrf52840_current.conf` — SDK firmware config
- `app/boards/nrf52840dk_nrf52840_i2cscan.conf` — diagnostic firmware config

---

## When you (or the new chat) pick this up

Open with: *"Phase 8a hardware validation — chip is proven responding at
0x40 (analyzer-verified, v0.8.2 shipped). Stable firmware-level reads pending
permanent connection. See docs/SESSION_HANDOFF_2026-05-24.md for full context."*

That gives the new session everything it needs to skip the debug loop and
jump to either: (a) finish the live-reading run with a soldered INA219, or
(b) begin Phase 8c design while waiting for hardware.

---

## ✅ RESOLVED on 2026-05-29 (v0.8.3 → v0.8.4 → v0.8.5)

This document is preserved for historical record. The path forward turned
out to be neither (a) nor (b) as written above:

- **Soldering didn't fix the nRF DK** (v0.8.3). Even with the INA219 male
  headers soldered and both I2C jumpers replaced with fresh wires, the
  nRF52840-DK PCA10056 SN 1050258557 still returned 0/6 ACKs. Swapping in
  a second INA219 chip also failed at every address. The defect is local
  to this DK unit — most likely damaged P0.26/P0.27 GPIOs from repeated
  plug-cycle wear.

- **Phase 8a closed via swap-the-MCU isolation** (v0.8.3). The same chip
  and same wires were moved to a **NUCLEO-L476RG** and ACKed 6/6
  immediately. STM32 became the validated Phase 8a platform.

- **Bug #1 discovered + fixed** (v0.8.4). The 5-min loaded-telemetry
  capture on STM32 revealed Q stayed pinned at 220.00 mAh despite a clear
  I=2.80 mA load — pointing to two compounding coulomb-counter bugs (full
  anchor fired every sample on CR2032; integrator semantics mismatched the
  SoC estimator). Both fixed; Q now ticks down 219.98 → 219.75 mAh over 5
  minutes (matches theory).

- **Cloud-side closed** (v0.8.5). Gateway now persists `current_ma` and
  `coulomb_mah` to InfluxDB; Grafana dashboard has dedicated "Live Current"
  and "Remaining Charge" panels.

### Evidence archived in this repo

- `docs/captures/2026-05-29-v0.8.3-q-pinned-bug-evidence.log` — pre-fix
  5-min capture showing Q stuck at 220.00 mAh
- `docs/captures/2026-05-29-v0.8.4-q-ticks-down-fix-evidence.log` —
  post-fix 5-min capture showing Q monotonically decreasing
- `docs/HARDWARE_TROUBLESHOOTING.md` — new "swap-the-MCU isolation"
  section with full diagnostic flow
- GitHub releases v0.8.3, v0.8.4, v0.8.5 — all with detailed notes
- Closed issues [#1](https://github.com/aliaksandr-liapin/ibattery-sdk/issues/1)
  and [#2](https://github.com/aliaksandr-liapin/ibattery-sdk/issues/2)
