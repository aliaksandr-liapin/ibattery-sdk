# INA219 I2C Diagnostic Tools

Captures and decodes the I2C bus during boot using a HiLetgo / Saleae-clone
USB logic analyzer. Used to diagnose why the INA219 isn't responding to
register writes on ESP32-C3 / nRF52840-DK.

## Prerequisites (one-time setup)

```bash
brew install sigrok-cli libsigrokdecode

# Firmware files for the FX2 chip in the HiLetgo analyzer
mkdir -p /opt/homebrew/share/sigrok-firmware
cd /opt/homebrew/share/sigrok-firmware
curl -sL https://sigrok.org/download/binary/sigrok-firmware-fx2lafw/sigrok-firmware-fx2lafw-bin-0.1.7.tar.gz \
    | tar xz --strip-components=1
```

## Wiring

| Analyzer Channel | Probe to | Purpose |
|-----------------|----------|---------|
| **D0**          | **SCL** (nRF52: P0.27, ESP32-C3: GPIO3) | Clock line |
| **D1**          | **SDA** (nRF52: P0.26, ESP32-C3: GPIO1) | Data line |
| **D2**          | **INA219 Vcc** (optional)              | Power-sag detection |
| **GND**         | **GND** (any GND on board)              | Reference |

Use the tiny grabber clips — they hold onto exposed wire ends and breadboard tie points.

## Capture

```bash
# Plug analyzer into USB. Wait 2-3 seconds for firmware upload.
./capture.sh                    # default 3 second capture
./capture.sh 5                  # 5 second capture
```

The script will:
1. Confirm the analyzer is detected
2. Start capturing for the specified duration
3. Prompt you to press **RESET on the nRF52840-DK** to trigger boot I2C activity
4. Save raw trace to `/tmp/ina219-capture-<timestamp>.sr`
5. Decode and print all I2C transactions to stdout

## What to look for in the decoded output

| Pattern | What it means |
|---------|---------------|
| `START`, `0x40 ADDRESS WRITE NACK` | Chip address not ACK'd — chip dead, wrong wiring, or pull-up too weak |
| `START`, `0x40 ADDRESS WRITE ACK`, `0x00 DATA WRITE NACK` | Chip alive, address OK, but register write rejected — power-supply sag or chip locked up |
| `START`, `0x40 ADDRESS WRITE ACK`, `0x00 DATA WRITE ACK`, `0x80 DATA WRITE ACK` | Full successful write — chip is working! |
| No transactions at all | Master never drives the bus — Zephyr driver init failure |
| Garbled / unaligned bits | I2C timing problem — pull-ups too weak or capacitance too high |

## Inspect the raw trace visually

The `.sr` file can be opened in PulseView for waveform inspection
(rise-time analysis, voltage levels, glitch detection):

```bash
# PulseView is not in Homebrew — download from:
# https://sigrok.org/wiki/Downloads
```

Without PulseView, the CLI decode is usually enough to identify the root cause.

## Common findings and fixes

**Symptom: Slow rise on SDA/SCL high transitions (>3µs)**
→ Pull-up resistors too weak. Add external 2.2kΩ-4.7kΩ pull-ups from SDA to Vcc
  and SCL to Vcc. Or shorten breadboard wires.

**Symptom: Vcc dips during writes**
→ Power supply not stable. Add a 100nF ceramic capacitor across INA219 Vcc/GND
  pins close to the chip.

**Symptom: Address ACK but data NACK**
→ Chip is wedged. Try cycling power to the INA219 specifically (not just the MCU).

**Symptom: No transactions at all**
→ Zephyr I2C peripheral not initialized. Check devicetree status="okay" and
  CONFIG_I2C=y in board config.
