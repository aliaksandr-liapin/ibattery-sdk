# Hardware Troubleshooting Guide

Diagnostic playbook for the iBattery SDK's hardware bring-up,
focused on the **INA219 current sensor** I2C subsystem.

This guide documents the systematic methodology used to debug
the INA219 connection on the nRF52840-DK during Phase 8a
hardware validation. It is reusable for any I2C peripheral.

---

## Required equipment

- **USB logic analyzer** (HiLetgo / Saleae clone, ~$10)
  - 8 channels, 24 MHz sample rate, FX2 chipset
  - [HiLetgo on Amazon](https://www.amazon.com/HiLetgo-Analyzer-Channel-Compatible-Officially/dp/B077LSG5P2)
- **PulseView / sigrok-cli** (free, open-source)
  - macOS: `brew install sigrok-cli libsigrokdecode`
- **fx2lafw firmware files** placed at `/opt/homebrew/share/sigrok-firmware/`
- **Spare male-male jumper wires** to tap I2C lines without breaking the circuit

---

## Capture script

```bash
./tools/i2c-analyzer/capture.sh [seconds]
```

The script auto-detects the analyzer, captures during user-triggered
reset, decodes I2C transactions, and saves the raw `.sr` trace for
PulseView inspection.

Channel mapping:
- **D0 (CH0)** → SCL
- **D1 (CH1)** → SDA
- **D2 (CH2)** → INA219 Vcc (optional — power-sag detection)
- **GND** → board GND (common reference)

---

## Four-phase debug methodology

### Phase 1: Verify electrical layer

Goal: prove the bus signals reach the probes.

1. **Idle voltage check.** With the bus quiescent, capture each line.
   - SDA and SCL should both read **HIGH** (3.3V).
   - If either reads LOW, the pull-up resistors are not active or the
     probe isn't electrically connected to the line.

2. **Vcc verification.** Probe the INA219's Vcc pin row.
   - Must read HIGH continuously.
   - If LOW or flickering, check power-header wiring and breadboard contacts.

3. **Probe wire health.** A surprising number of cheap analyzer probe
   wires have broken internal crimps. Test each wire by clipping it
   to a known-HIGH signal — if it doesn't show HIGH, the wire is bad.

### Phase 2: Verify master signaling

Goal: confirm the MCU's I2C peripheral correctly drives the bus.

1. **Trigger I2C activity.** The SDK's `battery_hal_current_init()`
   only attempts I2C at boot. To capture it, press the RESET button
   (or run `west flash`) while the analyzer captures.

2. **Decode the trace.** With both SCL and SDA captured:
   ```bash
   sigrok-cli -i /tmp/capture.sr -P i2c:scl=D0:sda=D1
   ```

3. **Expected pattern (chip not responding):**
   ```
   Start
   Address read: 40
   NACK
   Stop
   ```
   The master sends START, the 7-bit address `0x40`, the R/W bit,
   then receives no ACK from the slave, then sends STOP.

4. **Expected pattern (chip responding):**
   ```
   Start
   Address write: 40
   ACK
   Data: 00
   ACK
   Data: 80
   ACK
   Data: 00
   ACK
   Stop
   ```
   Each ACK is the slave pulling SDA low during the 9th clock pulse.

### Phase 3: Identify the slave failure

If Phase 2 shows NACK on the address byte, scan the entire I2C bus
to confirm the chip isn't at a different address:

```bash
# Flash the i2cscan firmware (CONFIG_I2C_SHELL=y)
west build -b nrf52840dk/nrf52840 app -d build-scan --pristine -- \
    -DEXTRA_CONF_FILE=boards/nrf52840dk_nrf52840_i2cscan.conf
west flash -d build-scan --runner jlink

# Open the Zephyr shell and run the scan
screen /dev/cu.usbmodem* 115200
uart:~$ i2c scan i2c@40003000
```

Possible outcomes:

| Result | Diagnosis |
|--------|-----------|
| Device at 0x40 | Original wiring problem, chip is alive |
| Device at 0x41/0x44/0x45 | A0/A1 address-select pads are misconfigured |
| **Zero devices** | Chip is dead or electrically disconnected from the I2C lines |
| Many devices | Bus is shorted somehow (rare) |

### Phase 4: Isolate the root cause

Once Phase 3 shows zero devices but Phase 1 confirms bus electrical:

1. **Swap to a different INA219 board** (rule out chip-level damage).
2. **Try the other I2C address pin assignment** — swap SDA and SCL
   wires at the master side (rules out silkscreen labeling errors
   on cheap clone boards).
3. **Replace with a verified-genuine board** — Adafruit's official
   INA219 (product #904) from a trusted source. Cheap clones from
   Asian vendors are known to ship with chip-level defects in some
   batches.

---

## Known issues with cheap INA219 clones

Verified during v0.8.0 hardware bring-up:

- **HiLetgo 2-pack INA219 (~$8)** — both chips in the pack failed
  to respond at any I2C address. Bus signaling was confirmed
  electrically correct via logic analyzer; pull-up resistors on
  the breakout boards worked; both chips silently refused to ACK.
- **Likely cause:** factory defect in a specific manufacturing
  batch, or ESD damage during packaging/handling.
- **Workaround:** order from Adafruit directly, Amazon "Sold by
  Adafruit Industries", or Mouser. The $5-10 price difference is
  cheap insurance against multi-hour debugging.

---

## Captured trace from v0.8.0 diagnostic session

Decoded I2C from a real session showing the failure mode:

```
i2c-1: Start
i2c-1: 1
i2c-1: 0
i2c-1: 0
i2c-1: 0
i2c-1: 0
i2c-1: 0
i2c-1: 0
i2c-1: 1
i2c-1: Read
i2c-1: Address read: 40
i2c-1: NACK
i2c-1: Stop
```

Interpretation:
- Master drives address bits `10000001` correctly (= `0x40 << 1 | R/W=1`)
- Bus electrical layer is fine — bits clocked cleanly
- Slave fails to pull SDA low on the 9th bit → NACK
- Master terminates with STOP

This trace alone proves the SDK firmware and the nRF52840-DK I2C
peripheral are correct. The fault is on the slave side.

---

## RESOLVED (v0.8.2): root cause was breadboard column misalignment

The "0 devices found" failures from v0.8.0/v0.8.1 were **not** chip defects.
The logic analyzer pinpointed the real cause:

**One I2C jumper was plugged into a different breadboard column than its
INA219 pin.** On a breadboard, only holes in the *same vertical column* are
connected. The nRF's SCL wire was one column off from the INA219's SCL pin,
so the clock signal left the nRF but dead-ended in an unconnected column.

### The signature that revealed it

Capturing both lines during a scan:

```
D3 (one line): 951 transition-blocks   → master actively driving it
D4 (other):      0 transition-blocks   → pull-up present (idle HIGH),
                                          but NO master signal
```

A line that idles HIGH (has a pull-up = chip pin connected) but shows zero
master activity means: **the chip's pin is on that breadboard column, but
the MCU's wire is not.** The two ends never meet. Re-aligning the wire to
share the INA219 pin's exact column immediately produced ACKs:

```
Start
Address write: 40
ACK            ← chip responds once both lines are truly connected
```

### Lesson: breadboard column alignment

For every connection, the MCU jumper and the peripheral pin must be in the
**same numbered column** (the vertical group of 5 connected holes). Stack
them: peripheral pin in row `a`, jumper in row `b`, probe in row `c` — all
in the same column. A wire in a different region of the breadboard, however
close it looks, is electrically isolated.

### Secondary issue: "analyzer sees ACK, firmware reads NACK"

After fixing alignment, the analyzer captured clean ACKs but the nRF's
`i2c scan` still intermittently reported 0 devices. This is a classic
**signal-integrity symptom**: worn breadboard contacts add series resistance,
so when the chip pulls SDA low for the ACK, the voltage at the MCU's pin
doesn't reach a clean logic-low at the exact sampling instant. The analyzer
(continuous sampling, high input impedance) catches the low; the MCU's
one-shot sample misses it.

Fix: a **permanent connection** — soldered header, or a fresh breadboard
with unworn contacts. The nRF52840 TWIM is already at its minimum 100 kHz,
so there is no slower-clock software workaround.

### Cheap-clone postscript

The two HiLetgo INA219 boards blamed earlier were most likely fine — they
were victims of the same column-alignment error, not defective. Final
confirmation used a genuine Adafruit INA219 (#904), but the lesson stands:
**verify wiring with a logic analyzer before condemning a chip.**

---

## Why we shipped v0.8.1 without on-target validation

The full software stack (HAL, coulomb counter, SoC estimator,
telemetry v3, gateway decoder) is verified by **80+ host tests**
(16 C suites, 65 Python suites). Logic-analyzer captures further
prove the firmware sends correct I2C transactions on real hardware.

The remaining gap — end-to-end current measurement from a working
INA219 — requires only a known-good INA219 board. Any genuine
Adafruit (or other reputable vendor) INA219 will close that gap
with zero code changes, because the SDK is wire-compatible with
any device implementing the TI INA219 I2C protocol.

---

## Quick reference: signal voltages

For a healthy I2C bus with the INA219 connected:

| Condition | SDA | SCL |
|-----------|-----|-----|
| Idle | HIGH (3.3V) | HIGH (3.3V) |
| START condition | falling edge | HIGH |
| Address transmit | bit transitions | clock pulses (100 kHz default) |
| Slave ACK | held LOW for 1 bit period | clock pulse |
| Slave NACK | stays HIGH | clock pulse |
| STOP condition | rising edge | HIGH |

If your captures don't match this pattern, refer back to Phase 1.
