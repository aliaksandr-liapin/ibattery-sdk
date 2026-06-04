# Bench Prep — Faded SoH over BLE → Grafana (end-to-end)

> **Goal of the test:** drive a real full→empty voltage excursion so the device
> *learns* a **sub-100% State of Health**, and prove that faded value travels
> **firmware → BLE → gateway → InfluxDB → Grafana** in a single run.
>
> This is the last genuine end-to-end gap: SoH-learning-on-hardware (over serial)
> and BLE→Grafana (at 100%) have each been proven *separately* — never a real
> sub-100% SoH all the way to the dashboard in one shot.

**Board:** NUCLEO-L476RG · **Rig:** PPK2 external-ADC voltage sense + INA219 ·
**Wire format:** v4 (34 B, `soh_pct_x100`).

---

## 0. Pre-flight status (already done — no action)

- ✅ **Firmware built + flashed** to the NUCLEO with the extADC + BLE + SoH image
  (`build-stm32-extadc`, config `boards/nucleo_l476rg_ble_extadc.conf`).
  Verified emitting **v4** telemetry with the external-ADC voltage source active
  (serial showed `[v4 …] … SOH=100.00%`, voltage read from A2 not VREFINT).
- ✅ Host tests (23), gateway tests (113), build/config drift guard all green.

> If the board has been reflashed since, rebuild + reflash:
> ```bash
> export PATH="/opt/homebrew/bin:/usr/bin:/bin:/opt/nordic/ncs/toolchains/e5f4758bcf/bin:$PATH"
> export ZEPHYR_BASE="/opt/nordic/ncs/v3.2.2/zephyr"
> export ZEPHYR_SDK_INSTALL_DIR="/opt/nordic/ncs/toolchains/e5f4758bcf/opt/zephyr-sdk"
> cd <repo>
> west build -b nucleo_l476rg app -d build-stm32-extadc --pristine -- \
>   -DSHIELD=x_nucleo_idb05a1 \
>   -DEXTRA_CONF_FILE=boards/nucleo_l476rg_ble_extadc.conf \
>   -DEXTRA_DTC_OVERLAY_FILE=boards/nucleo_l476rg_extadc.overlay \
>   -DZEPHYR_EXTRA_MODULES="/opt/nordic/ncs/v3.2.2/modules/hal/stm32"
> west flash -d build-stm32-extadc --runner openocd
> ```

---

## 1. Bill of materials (check each off before you start)

| ✓ | Item | Spec | Notes |
|---|---|---|---|
| ☐ | NUCLEO-L476RG | — | + ST-Link USB cable (data, not charge-only) |
| ☐ | X-NUCLEO-IDB05A1 BLE shield | — | mounted on Arduino headers |
| ☐ | INA219 current sensor | — | I2C already wired (SDA=PB9/D14, SCL=PB8/D15) |
| ☐ | **PPK2** (Nordic Power Profiler Kit II) | 0.8–5.0 V, ≤1 A | emulates the discharging cell |
| ☐ | **2 × equal resistors** | **1 k–22 kΩ**, ±5% OK, ¼ W | the ÷2 voltage divider — **R1 must equal R2**. 10 k/10 k recommended |
| ☐ | **1 × load resistor** | 47–100 Ω, **≥0.5 W** | sets discharge current; smaller Ω = faster |
| ☐ | breadboard + jumper wires | — | — |
| ☐ | multimeter | — | for the pre-power continuity/voltage checks |
| ☐ | Mac running **iTerm** | — | for the BLE gateway (see §4) |

> **No capacitor needed (cap-free divider).** The original spec used 100 k/100 k
> + a 100 nF cap; the cap was only there to stabilize the ADC against the
> divider's high 50 kΩ source impedance. A **lower-impedance divider removes the
> need for it**: 10 k/10 k = 5 kΩ tap impedance, which the STM32 sample-and-hold
> reads cleanly on its own. Any **equal pair** keeps the ÷2 ratio, so **no config
> change** (`CONFIG_BATTERY_VOLTAGE_DIVIDER_RATIO=2` still holds). Avoid going
> below ~1 kΩ (wastes PPK2 current for no benefit). The median voltage filter
> (`CONFIG_BATTERY_VOLTAGE_FILTER_MEDIAN=y`) is recommended belt-and-suspenders.

**Power dissipation sanity (load resistor):** at 3 V across 100 Ω → 30 mA, 0.09 W
(fine for ¼ W). Across 47 Ω → 64 mA, 0.19 W (still fine). A ¼ W part is adequate;
≥0.5 W gives margin. **Divider current** at 10 k/10 k ≈ 150 µA @ 3 V — negligible,
and tapped *before* the INA219 shunt so it never touches the current reading.

---

## 2. Wiring the rig

Pin is **A2 / PA4 (ADC1_IN9)** — *not* A0/A1 (the BLE shield owns A0=IRQ, A1=SPI CS).

**Cap-free divider** — 10 k/10 k (any equal pair 1 k–22 kΩ), no capacitor:

```
PPK2 (Source Meter mode, VOUT = emulated cell +)
  │
  ├─ VOUT ─┬───────────────────────────► INA219 VIN+      (current path)
  │        │                              INA219 VIN- ─► [Load R 47–100Ω] ─┐
  │        │                                                               │
  │        └─ [10kΩ R1] ──┬── A2 / PA4 (ADC1_IN9)  (voltage sense tap)     │
  │                       │   (no cap — 5kΩ tap impedance reads clean)     │
  │                    [10kΩ R2]                                           │
  │                       │                                                │
  └─ GND ─────────────────┴────────────────► NUCLEO GND ◄─────────────────┘
                                             INA219 GND  (COMMON GROUND!)

  INA219 VCC ◄── NUCLEO 3V3      SDA ◄──► PB9 / D14      SCL ◄──► PB8 / D15
  (INA219 I2C already wired from the current-sense build)
```

### Connection list (point-to-point)

Set up three breadboard nodes first, then every part/wire just lands on them:

- **[VOUT]** — PPK2 positive output rail
- **[GND]** — common ground rail (everything grounded ties here)
- **[SENSE]** — divider midpoint (one short row) that feeds A2

**Resistors**

| Part | One end → | Other end → |
|---|---|---|
| **R1** (10 kΩ) | **[VOUT]** | **[SENSE]** |
| **R2** (10 kΩ) | **[SENSE]** | **[GND]** |
| **RL** (load 47–100 Ω, ≥0.5 W) | **INA219 VIN−** | **[GND]** |

**Wires**

| Wire | One end → | Other end → |
|---|---|---|
| **W1** | PPK2 **VOUT** | **[VOUT]** |
| **W2** | **[VOUT]** | INA219 **VIN+** |
| **W3** | **[SENSE]** | NUCLEO **A2 / PA4** (CN8) |
| **W4** | PPK2 **GND** | **[GND]** |
| **W5** | NUCLEO **GND** (CN6) | **[GND]** |
| **W6** | INA219 **GND** | **[GND]** |

**INA219 logic** — already wired from the current-sense build; *verify, don't redo*:

| Wire | One end → | Other end → |
|---|---|---|
| **W7** | INA219 **VCC** | NUCLEO **3V3** (CN6) |
| **W8** | INA219 **SDA** | NUCLEO **D14 / PB9** |
| **W9** | INA219 **SCL** | NUCLEO **D15 / PB8** |

Current path: PPK2 **VOUT** → **VIN+** →(INA219 internal shunt)→ **VIN−** → **RL** →
**GND**. The divider hangs off **[VOUT]** *before* the shunt, so its ~150 µA never
touches the current reading.

### Step-by-step
1. **Power off** the PPK2 output before wiring.
2. Build the **divider**: PPK2 VOUT → R1 (10 kΩ) → node **M** → R2 (10 kΩ) → GND.
   Tap node **M** to **A2 / PA4**. (R1 = R2 = any equal pair 1 k–22 kΩ.)
3. Build the **current path**: PPK2 VOUT → INA219 **VIN+**; INA219 **VIN-** →
   load resistor → GND.
4. **Tie all grounds together**: PPK2 GND + INA219 GND + divider bottom (R2) +
   load-resistor return all to a **NUCLEO GND** pin (CN6). This common ground is
   **mandatory** — the ADC measures A2 relative to NUCLEO GND.
5. Confirm INA219 VCC ← NUCLEO 3V3, and SDA/SCL on D14/D15 (already there).

### Three things that bite if wrong
- **Common ground missing** → garbage / floating ADC readings.
- **R1 ≠ R2** → wrong divider ratio; firmware assumes ÷2
  (`CONFIG_BATTERY_VOLTAGE_DIVIDER_RATIO=2`).
- **Divider impedance too high** → noisy voltage. With no cap, keep R1 = R2 in the
  **1 k–22 kΩ** range (≤11 kΩ tap impedance). If you only have ~100 kΩ parts, either
  add a 10–100 nF cap A2→GND *or* enable the median filter and expect more jitter.

---

## 3. Pre-power checks (multimeter, before energizing)

| ✓ | Check | Expected |
|---|---|---|
| ☐ | Continuity: PPK2 GND ↔ NUCLEO GND ↔ INA219 GND ↔ R2 bottom | all connected (beep) |
| ☐ | R1 ≈ R2 | equal within a few % (e.g. both ~10 kΩ) |
| ☐ | No short VOUT ↔ GND | open (no beep) |
| ☐ | Load resistor in series with INA219 VIN- (not across VOUT→GND directly) | correct path |

Then energize at a **low voltage first** (e.g. PPK2 at 3.0 V) and measure node
**M** (A2): it should read **≈ VOUT / 2** (e.g. ~1.5 V). If A2 ≠ VOUT/2, fix the
divider before trusting anything downstream.

---

## 4. Software stack bring-up (before the excursion)

### 4.1 Cloud (InfluxDB + Grafana)
```bash
cd <repo>/cloud && docker compose up -d     # Rancher Desktop, NOT Docker.app
```
- Grafana: http://localhost:3000  (admin/admin)
- InfluxDB: http://localhost:8086 (ibattery/ibattery123)
- Open the **"State of Health (%)"** panel and leave it visible.

### 4.2 Gateway — **MUST run from iTerm**
> Claude Code / Claude.app lacks the macOS Bluetooth (TCC) grant — CoreBluetooth
> will SIGABRT. The bare pyenv python also has no `NSBluetoothAlwaysUsageDescription`.
> Run these from **iTerm**:
```bash
cd <repo>/gateway && pip install -e ".[dev]"
ibattery-gateway scan      # confirm "iBattery-STM32" is advertising
ibattery-gateway run       # stream + write to InfluxDB
```
- Entry point is **`ibattery-gateway`**, not `ibattery`.
- The scanner matches by **service UUID** (reliable on macOS), so the device name
  is informational.

### 4.3 GitHub (only if publishing results/captures)
```bash
gh auth switch -h github.com -u aliaksandr-liapin   # reverts to aliapin-maker each session
```

---

## 5. The excursion procedure

CR2032 profile thresholds: **full ≥ 2950 mV, empty ≤ 2000 mV**.
Firmware: full anchor arms SoH; empty anchor learns SoH = charge drawn between
anchors; learned value must land within **30–120% of rated** or it's rejected.

1. **Arm the full anchor** — PPK2 Source Meter ON at **~3.1 V**.
   Serial / gateway shows `V≈3100 mV`, `SOC≈100%`, `SOH=100.00%`.
2. **Confirm current flows** — `I≈` your load current (e.g. ~30 mA for 100 Ω),
   and `Q` (mAh) starts ticking **down**. Cross-check against the PPK2's own
   current readout (handy ground truth).
3. **Draw charge** — let it discharge. The more charge drawn between anchors, the
   lower the learned SoH. The prior validated run learned **83.30%**.
4. **Fire the empty anchor** — sweep PPK2 down to **~1.9 V**. SoH drops below 100%
   (= charge drawn between anchors) and is written to NVS.
5. **Watch Grafana** — the **State of Health (%)** panel shows the learned
   sub-100% value arriving over BLE. ✅ **this is the test passing.**
6. *(optional, bonus)* **Power-cycle / reset** the board → serial shows the
   learned `SOH=` on the first line (restored from flash, not 100%) → confirms
   persistence on a real voltage-driven excursion too.

### Make it faded *fast*
The default excursion can be slow. To reach a sub-100% value in minutes:
- Use a **smaller load resistor** (47 Ω → ~64 mA vs 100 Ω → ~30 mA), and/or
- Temporarily **lower `CONFIG_BATTERY_CAPACITY_MAH`** (e.g. 50) in the conf and
  rebuild/reflash — a full excursion then completes in minutes.
- Keep the learned value in the **30–120% of rated** window or it's rejected.

---

## 6. Acceptance criteria (what "pass" means)

- [ ] Gateway `scan` sees **iBattery-STM32** advertising over BLE.
- [ ] `run` streams **v4** packets (length 34, `soh_pct` present) into InfluxDB.
- [ ] During discharge: `I` ≈ load current, `Q` decreasing.
- [ ] After the empty anchor: SoH on the wire drops **below 100%**.
- [ ] Grafana **"State of Health (%)"** panel renders the faded value.
- [ ] *(bonus)* learned SoH survives a board reset (NVS persistence).
- [ ] Save the serial + gateway logs to `docs/captures/<date>-soh-ble-grafana-e2e.log`.

---

## 7. Troubleshooting

| Symptom | Likely cause / fix |
|---|---|
| Voltage reading wrong / noisy | check 100 nF cap, common ground, R1=R2; measure A2 = VOUT/2 with a meter |
| External-ADC reads ~6–9% low | known divider/ADC-gain trim item; does **not** affect SoH (charge-based) |
| SoH stays 100% | didn't cross empty (≤2000 mV), or excursion rejected as implausible (outside 30–120% of rated) — draw more charge before dropping to empty |
| No current / Q not moving | INA219 VIN+/VIN- orientation, or load resistor not in the path |
| BLE not advertising / no scan hit | confirm shield seated; A0/A1/D7 left free for the shield; only A2 borrowed |
| Gateway SIGABRT on macOS | you're not in iTerm — CoreBluetooth needs the Bluetooth TCC grant |
| `ibattery: command not found` | entry point is **`ibattery-gateway`** |
| Grafana empty | InfluxDB creds / `docker compose` up; check gateway `run` is actually writing |
| Serial port not found | `ls /dev/cu.usbmodem*`; close other terminals holding it; reseat USB |

---

## 8. Reference

- `docs/WIRING.md` → "External ADC voltage sense + PPK2 SoH excursion rig"
- `docs/SESSION_HANDOFF_2026-06-02.md` → next-HW-test rationale
- Prior validated captures:
  - `docs/captures/2026-06-01-soh-voltage-excursion-extadc-e2e.log` (learned 83.30%)
  - `docs/captures/2026-06-01-soh-nvs-persistence-e2e.log`
- Build config: `app/boards/nucleo_l476rg_ble_extadc.conf`
- Overlay: `app/boards/nucleo_l476rg_extadc.overlay` (`adc1_in9_pa4`)
