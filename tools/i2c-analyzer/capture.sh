#!/usr/bin/env bash
#
# Capture I2C bus activity using a HiLetgo / Saleae-clone USB logic analyzer
# and decode it with sigrok. Used to diagnose the INA219 communication issue.
#
# Channels:
#   D0 → SCL (clock)
#   D1 → SDA (data)
#   D2 → INA219 Vcc (power-supply sag detection)  [optional]
#   GND → GND
#
# Usage:
#   ./capture.sh [seconds]            # default: 3
#
# Output:
#   /tmp/ina219-capture-<timestamp>.sr  — raw trace (open in PulseView)
#   stdout                              — decoded I2C transactions
#

set -e

SECONDS=${1:-3}
SAMPLE_RATE=4000000          # 4 MHz — 40x oversampling for 100 kHz I2C
SAMPLE_COUNT=$((SAMPLE_RATE * SECONDS))
TS=$(date +%Y%m%d-%H%M%S)
TRACE=/tmp/ina219-capture-${TS}.sr

echo "─── INA219 I2C Capture ─────────────────────────────────────"
echo "Sample rate:  ${SAMPLE_RATE} Hz"
echo "Duration:     ${SECONDS} sec  (${SAMPLE_COUNT} samples)"
echo "Trace file:   ${TRACE}"
echo "Channels:     D0=SCL, D1=SDA, D2=Vcc(optional)"
echo "────────────────────────────────────────────────────────────"

# 1. Confirm the analyzer is plugged in
if ! sigrok-cli --scan 2>&1 | grep -qiE "fx2|saleae"; then
    echo
    echo "ERROR: No fx2lafw / Saleae-clone logic analyzer detected."
    echo "  - Plug the analyzer into USB"
    echo "  - Wait 2-3 seconds for firmware upload"
    echo "  - Re-run this script"
    echo
    echo "Devices currently seen by sigrok-cli:"
    sigrok-cli --scan 2>&1 | sed 's/^/  /'
    exit 1
fi

DEVICE_INFO=$(sigrok-cli --scan 2>&1 | grep -E "fx2|saleae" | head -1)
echo
echo "Found analyzer: ${DEVICE_INFO}"
echo
echo ">>> Press RESET on the nRF52840-DK NOW <<<"
echo "    (capture starts in 1 second, runs for ${SECONDS}s)"
sleep 1

# 2. Capture to .sr file (openable in PulseView later)
sigrok-cli \
    --driver fx2lafw \
    --config samplerate=${SAMPLE_RATE} \
    --channels D0,D1,D2 \
    --samples ${SAMPLE_COUNT} \
    --output-file "${TRACE}" \
    --output-format srzip

echo
echo "─── Capture complete ───────────────────────────────────────"
echo "Raw trace saved: ${TRACE}"
echo
echo "─── Decoded I2C transactions ───────────────────────────────"

# 3. Decode and print I2C transactions
sigrok-cli \
    --input-file "${TRACE}" \
    --protocol-decoders i2c:scl=D0:sda=D1 \
    --protocol-decoder-annotations i2c=address-read:address-write:data-read:data-write:ack:nack:start:start-repeat:stop \
    2>&1 | head -200

echo
echo "─── Done ──────────────────────────────────────────────────"
echo "Open trace in PulseView (when available):"
echo "  pulseview ${TRACE}"
echo
echo "Or rerun decode only:"
echo "  sigrok-cli -i ${TRACE} -P i2c:scl=D0:sda=D1"
