# Phase 1 Step 3 — Voltage Filtering

## Goal

Implement signal stabilization for ADC voltage measurements.

---

# Problem

Raw ADC readings can be noisy.

Noise sources include:

- floating ADC inputs
- EMI
- thermal noise
- analog leakage

---

# Solution

Implement a moving average filter inside the battery domain.

---

# Architecture
battery_voltage
↓
battery_voltage_filter
↓
battery_adc
↓
HAL


HAL remains responsible only for hardware access.

---

# Implementation

New module:
battery_voltage_filter


Files:
src/core_modules/battery_voltage_filter.c
src/core_modules/battery_voltage_filter.h


---

# Filter Algorithm

Moving average:
V_filtered = (V1 + V2 + ... + VN) / N


Current window:
N=12


---

# Hardware Validation

Platform:
nRF52840 DK
Zephyr RTOS


Example runtime output:
Voltage: 144 mV
Voltage: 148 mV
Voltage: 150 mV
Voltage: 147 mV


Result:

Noise significantly reduced.

---

# Outcome

Phase 1 Step 3 completed successfully.

Battery SDK now provides stable voltage readings suitable for SOC estimation.

---

# Next Step

Phase 1 Step 4:

SOC estimation based on voltage.
