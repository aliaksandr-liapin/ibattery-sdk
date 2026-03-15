# Phase 5b — Manual Test Plan

## Prerequisites

- nRF52840-DK with CR2032 (or LiPo + TP4056)
- Gateway running: `ibattery stream` or `ibattery run`
- InfluxDB running on localhost:8086
- nRF Connect mobile app (optional, for BLE packet inspection)

---

## T1: Wire Format v2 — 24-byte packets

### T1.1 — Gateway receives v2 packets
1. Flash firmware: `west build -b nrf52840dk/nrf52840 app -d build-app --pristine && west flash -d build-app`
2. Run: `ibattery stream`
3. **EXPECT**: packets show `v2` prefix (not `v1`)
4. **EXPECT**: `cyc=N` appears in output (N >= 0)

### T1.2 — Cycle count field in stream output
1. With `ibattery stream` running, observe multiple packets
2. **EXPECT**: `cyc=0` or `cyc=N` appears after flags field
3. If cycle_count is 0, the `cyc=` field is hidden — this is correct for fresh boot

### T1.3 — nRF Connect raw packet size
1. Open nRF Connect mobile, connect to iBattery
2. Read the telemetry characteristic (UUID 12340002-...)
3. **EXPECT**: characteristic value is **24 bytes** (was 20 in v1)
4. First byte should be `0x02` (version 2)

---

## T2: Cycle Counter

### T2.1 — Cycle count increments on charge cycle
1. Run `ibattery stream`
2. Simulate a charge cycle:
   - Connect TP4056 charger (or simulate via GPIO if using charger scaffold)
   - Wait for CHARGING state in stream output
   - Wait for CHARGED state
3. **EXPECT**: `cyc=N` increments by 1 after CHARGING → CHARGED transition

### T2.2 — Cycle count persists across reboot
1. Note the current cycle count from stream output: `cyc=N`
2. Press RESET on the nRF52840-DK
3. Resume `ibattery stream`
4. **EXPECT**: cycle count is still `N` (persisted in NVS flash)

### T2.3 — Cycle count does NOT increment on partial charge
1. Start charging (CHARGING state appears)
2. Disconnect charger before CHARGED state
3. **EXPECT**: cycle count stays the same (no increment)

---

## T3: CONFIG_BATTERY_CHEMISTRY

### T3.1 — CR2032 build (default)
1. Verify `app/prj.conf` has `CONFIG_BATTERY_CHEMISTRY_CR2032=y`
2. Build and flash
3. Run `ibattery stream`
4. **EXPECT**: SoC readings are reasonable for CR2032 (~3.0V → ~50-60%)
5. **EXPECT**: SoC is NOT near 0% (that was the Phase 5a bug with LiPo tables)

### T3.2 — LiPo build
1. Change `app/prj.conf`:
   ```
   # CONFIG_BATTERY_CHEMISTRY_CR2032 is not set
   CONFIG_BATTERY_CHEMISTRY_LIPO=y
   ```
2. Build and flash (with LiPo battery connected)
3. Run `ibattery stream`
4. **EXPECT**: SoC readings match LiPo voltage curve (~4.2V = 100%, ~3.0V = 0%)
5. **EXPECT**: Temperature compensation is active (gated on LIPO)

---

## T4: Gateway InfluxDB Pipeline

### T4.1 — cycle_count written to InfluxDB
1. Run: `ibattery run`
2. Let a few packets flow
3. Query InfluxDB:
   ```
   from(bucket: "ibattery")
     |> range(start: -5m)
     |> filter(fn: (r) => r._field == "cycle_count")
     |> last()
   ```
4. **EXPECT**: `cycle_count` field exists with integer value

### T4.2 — v2 packet decoded correctly in InfluxDB
1. Query all fields from latest telemetry:
   ```
   from(bucket: "ibattery")
     |> range(start: -1m)
     |> filter(fn: (r) => r._measurement == "battery_telemetry")
     |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
     |> last()
   ```
2. **EXPECT**: all fields present: `voltage_v`, `temperature_c`, `soc_pct`, `power_state`, `cycle_count`

---

## T5: Analytics CLI Commands

### T5.1 — Health score
```bash
ibattery analytics health
```
**EXPECT**: Health report with score 0-100, or "Insufficient data" message

### T5.2 — Anomaly detection
```bash
ibattery analytics anomalies --window 60
```
**EXPECT**: Lists anomalies or "No anomalies detected"

### T5.3 — RUL estimation
```bash
ibattery analytics rul
```
**EXPECT**: RUL report with status, current health, current cycles, or "Insufficient data"

### T5.4 — Cycle analysis
```bash
ibattery analytics cycles
```
**EXPECT**: Cycle report with total cycles, capacity fade, temperature stats, or "Insufficient data"

---

## T6: Grafana Dashboard

### T6.1 — Import dashboard
1. Open Grafana → Dashboards → Import
2. Upload `gateway/grafana/ibattery-dashboard.json`
3. Select InfluxDB data source
4. **EXPECT**: Dashboard loads with 11 panels, no errors

### T6.2 — Panels populate with data
1. With `ibattery run` active, wait 30-60 seconds
2. **EXPECT**: Voltage, Temperature, SoC time series show live data
3. **EXPECT**: Cycle Count stat panel shows current count
4. **EXPECT**: SoC gauge shows current percentage

---

## T7: Backward Compatibility

### T7.1 — Gateway handles v1 packets gracefully
1. If you have an older firmware (v1, 20-byte), connect to gateway
2. Run `ibattery stream`
3. **EXPECT**: packets decode with `v1` prefix, `cycle_count` = 0, no errors

---

## Results Checklist

| Test | Pass | Notes |
|------|------|-------|
| T1.1 v2 packets in stream | | |
| T1.2 cyc= field visible | | |
| T1.3 24-byte in nRF Connect | | |
| T2.1 cycle increments | | |
| T2.2 persists across reboot | | |
| T2.3 no increment on partial | | |
| T3.1 CR2032 SoC correct | | |
| T3.2 LiPo SoC correct | | |
| T4.1 cycle_count in InfluxDB | | |
| T4.2 all fields in InfluxDB | | |
| T5.1 health CLI | | |
| T5.2 anomalies CLI | | |
| T5.3 rul CLI | | |
| T5.4 cycles CLI | | |
| T6.1 dashboard import | | |
| T6.2 panels populate | | |
| T7.1 v1 backward compat | | |
