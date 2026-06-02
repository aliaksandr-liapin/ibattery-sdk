# Hardware Wiring Guide

## nRF52840-DK (PCA10056)

### CR2032 Coin Cell (simplest setup)

No wiring needed — insert CR2032 into the coin cell holder, set power switch to **VDD**.

```
CR2032 (+) ──── VDD rail ──── nRF52840 SAADC (internal)
CR2032 (-) ──── GND
```

### LiPo + TP4056 Charger

```
LiPo 3.7V ──── TP4056 HW-373 B+/B-
                TP4056 OUT+ ──── nRF52840-DK VDD
                TP4056 OUT- ──── nRF52840-DK GND
                TP4056 CHRG ──── P0.28 (GPIO input, pull-up)
                TP4056 STDBY ──── P0.29 (GPIO input, pull-up)
```

### NTC Thermistor (optional, default temp source)

```
VDD ──── [10K resistor] ────┬──── [10K NTC B3950] ──── GND
                            │
                         P0.03 (AIN1)
```

---

## NUCLEO-L476RG (STM32)

### Basic Setup (USB powered, no battery)

No wiring needed — plug USB into ST-Link connector. VDD measured internally via VREFINT.

### NTC Thermistor (optional)

```
3V3 ──── [10K resistor] ────┬──── [10K NTC B3950] ──── GND
                            │
                          PA0 (Arduino A0, ADC1 Ch5)
```

**Note**: NTC on PA0 conflicts with X-NUCLEO-IDB05A2 BLE shield (IRQ pin). Use die temp sensor when shield is mounted.

### TP4056 Charger (optional)

```
TP4056 CHRG ──── PC6 (Morpho connector CN7 pin 2)
TP4056 STDBY ──── PC7 (Morpho connector CN7 pin 4)
```

### BLE Shield (X-NUCLEO-IDB05A2)

Plug shield directly onto NUCLEO Arduino headers. No wiring needed. Shield uses:
- SPI: MOSI/MISO/SCK (Arduino SPI)
- CS: PA1 (Arduino A1)
- IRQ: PA0 (Arduino A0)
- RESET: PA8 (Arduino D7)

### External ADC voltage sense + PPK2 SoH excursion rig

This rig drives a **full→empty voltage excursion with real current** so State
of Health actually *learns* a sub-100% value on hardware (and, with v0.12.0,
proves it persists across a power-cycle). The Nordic **PPK2** emulates a
discharging cell — it's a programmable source (0.8–5.0 V, ≤1 A) that also
measures current — so no slow real-battery discharge is needed. Build with
`CONFIG_BATTERY_VOLTAGE_EXTERNAL_ADC=y`, which reads battery voltage from a
divider on **A2 / PA4 (ADC1_IN9)** instead of VDD/VREFINT.

> Why A2 and not A0/A1: the BLE shield uses **A0 = IRQ** and **A1 = SPI CS**.
> A2/PA4 is free with the shield mounted.

**Bill of materials** (all from the bench kit): NUCLEO-L476RG + X-NUCLEO-IDB05A1
BLE shield + INA219 (already wired), **PPK2**, **2× 100 kΩ** resistors, **1×
load resistor** (47–100 Ω, ≥0.5 W), **1× 100 nF** ceramic cap, breadboard +
jumpers.

**Connections**

```
  PPK2 (Source Meter mode, VOUT = emulated cell +)
    │
    ├─ VOUT ─┬─────────────────────────► INA219  VIN+        (current path)
    │        │                            INA219  VIN- ─► [Load R 47–100Ω] ─┐
    │        │                                                              │
    │        └─ [100kΩ R1] ──┬── A2 / PA4 (ADC1_IN9)   (voltage sense tap)  │
    │                        │                                              │
    │                      [100nF]                                          │
    │                        │                                              │
    │                     [100kΩ R2]                                        │
    │                        │                                              │
    └─ GND ──────────────────┴───────────────► NUCLEO GND ◄────────────────┘
                                               INA219 GND (common ground!)

  INA219 VCC ◄── NUCLEO 3V3      SDA ◄──► PB9 / D14      SCL ◄──► PB8 / D15
  (INA219 I2C is already wired from the current-sense build)
```

Pin reference (NUCLEO Arduino headers):

| Signal | NUCLEO pin | Notes |
|---|---|---|
| Battery-V sense | **A2 / PA4** (CN8) | divider midpoint → ADC1_IN9 |
| GND (common) | any GND (CN6) | **tie PPK2 GND + INA219 GND + divider bottom here** |
| INA219 VCC | 3V3 (CN6) | sensor logic supply |
| INA219 SDA / SCL | D14/PB9, D15/PB8 | I2C1 (already wired) |

**Key points**
- The **÷2 divider keeps A2 ≤ 2.5 V** even at the PPK2's 5 V max (ADC ref =
  VDDA = 3.3 V), so the pin is never over-driven. Firmware multiplies by
  `CONFIG_BATTERY_VOLTAGE_DIVIDER_RATIO` (=2) to recover the cell voltage.
- The **100 nF cap A2→GND** is required: the 50 kΩ divider impedance is high
  for the STM32 ADC; the cap + the long sampling time the firmware sets give a
  stable reading.
- **Common ground is mandatory** — the ADC measures A2 relative to NUCLEO GND,
  so PPK2 GND must tie to it.
- The voltage tap is **before** the INA219 shunt; the ~15 µA divider current
  does not flow through the shunt, so the INA219 sees only the load current.

**PPK2 setup** (nRF Connect for Desktop → Power Profiler app): **Source Meter**
mode, set supply voltage, current limit ~1 A. The PPK2's own current readout is
a handy ground-truth to sanity-check the INA219/coulomb value.

**Build + flash**

```bash
west build -b nucleo_l476rg app -d build-stm32-extadc --pristine -- \
  -DSHIELD=x_nucleo_idb05a1 \
  -DEXTRA_CONF_FILE=boards/nucleo_l476rg_ble_extadc.conf \
  -DEXTRA_DTC_OVERLAY_FILE=boards/nucleo_l476rg_extadc.overlay \
  -DZEPHYR_EXTRA_MODULES="/opt/nordic/ncs/v3.2.2/modules/hal/stm32"
west flash -d build-stm32-extadc --runner openocd
```

**Excursion procedure** (CR2032 profile: full ≥ 2950 mV, empty ≤ 2000 mV)
1. PPK2 Source Meter ON at **~3.1 V**. Serial shows `V≈3100 mV`, `SOC≈100%`,
   `SOH=100.00%` — the full anchor has fired and SoH is armed.
2. Confirm current flows: `I≈` your load current, and `Q` (mAh) ticks **down**.
3. Let it draw for a while (the longer, the lower the learned SoH). To keep it
   quick, use a bigger load and/or a small `CONFIG_BATTERY_CAPACITY_MAH` (e.g.
   set it to 50 so an excursion finishes in minutes).
4. Sweep PPK2 down to **~1.9 V**. The empty anchor fires →
   `SOH` drops below 100% (learned = charge drawn between anchors) and is
   written to NVS.
5. **Power-cycle / reset** the board → serial shows the learned `SOH=` on the
   first line (restored from flash, not 100%). ✓ persistence proven on a real
   voltage-driven excursion.
6. *(optional)* Run the gateway from **iTerm** to see it on the Grafana
   "State of Health (%)" panel.

**Troubleshooting**
- *Voltage reading wrong/noisy* → check the 100 nF cap, the common ground, and
  that R1=R2 (ratio 2). Compare A2 voltage (should be cell/2) with a multimeter.
- *SOH stays 100%* → you didn't cross the empty threshold, or the excursion was
  rejected as implausible (learned must land in 30–120% of rated). Draw more
  charge before dropping to empty.
- *No current / Q not moving* → check the INA219 VIN+/VIN- orientation and the
  load resistor.
- *BLE stops working* → make sure A0/A1/D7 are left for the shield; only A2 is
  borrowed for sensing.

---

## ESP32-C3 DevKitM

### Battery Voltage Divider (required for real battery readings)

ESP32-C3 cannot read VDD directly. Use a resistor divider:

```
Battery+ ──── [100K R1] ────┬──── [100K R2] ──── GND
                            │
                          GPIO2 (ADC1 Ch2)

V_adc = V_batt / 2
Firmware multiplies by 2 automatically.
```

**Component list**: 2x 100K ohm resistors (1/4W, any tolerance)

**Range**: With 12 dB attenuation, ADC input range is ~0–2.5V, so battery range is 0–5V (covers LiPo 3.0–4.2V fully).

### NTC Thermistor (optional)

```
3V3 ──── [10K resistor] ────┬──── [10K NTC B3950] ──── GND
                            │
                          GPIO3 (ADC1 Ch3)
```

### TP4056 Charger (optional)

```
TP4056 CHRG ──── GPIO6 (input, pull-up)
TP4056 STDBY ──── GPIO7 (input, pull-up)
```

### BLE

Native — no external hardware needed. ESP32-C3 has built-in BLE 5.0.

---

## Pin Summary

| Function | nRF52840-DK | NUCLEO-L476RG | ESP32-C3 DevKitM |
|----------|-------------|---------------|-------------------|
| VDD/Battery | Internal SAADC | Internal VREFINT | GPIO2 (divider) |
| NTC temp | P0.03 (AIN1) | PA0 (A0) | GPIO3 |
| TP4056 CHRG | P0.28 | PC6 | GPIO6 |
| TP4056 STDBY | P0.29 | PC7 | GPIO7 |
| BLE | Native | Shield SPI | Native |
| Serial console | J-Link USB | ST-Link USB | USB-UART (CP2102) |
