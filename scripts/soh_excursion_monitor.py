#!/usr/bin/env python3
"""Background serial monitor/logger for the SoH excursion.
Logs timestamped telemetry to a capture file and flags SoH/anchor transitions.
"""
import sys, time, re, glob, serial

LOGFILE = sys.argv[1] if len(sys.argv) > 1 else "/tmp/soh-excursion.log"
DURATION = int(sys.argv[2]) if len(sys.argv) > 2 else 1200  # seconds

ports = [p for p in sorted(glob.glob('/dev/cu.usbmodem*')) if 'F6BD' not in p]
PORT = ports[0] if ports else '/dev/cu.usbmodem11203'

p = serial.Serial()
p.port = PORT; p.baudrate = 115200; p.timeout = 1; p.dtr = True; p.rts = True
p.open(); time.sleep(0.3); p.reset_input_buffer()

rx = re.compile(r'V=(\d+) mV.*?SOC=([\d.]+)%.*?PWR=(\d+).*?I=([-\d.]+) mA.*?Q=([-\d.]+) mAh.*?SOH=([\d.]+)')
prev_soh = None
end = time.time() + DURATION
with open(LOGFILE, 'w') as f:
    f.write(f"# SoH excursion capture — port {PORT} — start epoch {int(time.time())}\n")
    f.flush()
    while time.time() < end:
        line = p.readline()
        if not line:
            continue
        s = line.decode('utf-8', 'replace').rstrip()
        stamp = time.strftime('%H:%M:%S')
        f.write(f"{stamp} {s}\n"); f.flush()
        m = rx.search(s)
        if m:
            soh = float(m.group(6))
            if prev_soh is not None and abs(soh - prev_soh) >= 0.01:
                f.write(f"{stamp} >>> SOH CHANGED {prev_soh:.2f}% -> {soh:.2f}% <<<\n"); f.flush()
            prev_soh = soh
p.close()
