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
