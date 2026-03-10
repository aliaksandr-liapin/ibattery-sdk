# Battery Profiles — Research & LUT Design

This document records the research behind each voltage-to-SoC lookup table shipped in the SDK: what data was collected, how table points were chosen, and the known limitations of each profile.

---

## How LUT-Based SoC Estimation Works

The SDK estimates state-of-charge by mapping a measured open-circuit voltage (OCV) to a percentage via a piecewise-linear lookup table. Given a voltage reading, the interpolation engine finds the two nearest table entries and linearly interpolates between them.

**Key properties:**
- Integer-only math (no FPU required)
- O(n) lookup where n = number of table entries (typically 9-11)
- SoC expressed in 0.01% units (`soc_pct_x100`): 10000 = 100.00%, 0 = 0.00%
- Table entries sorted by voltage descending (highest voltage first)
- Clamped at boundaries: above max → 100%, below min → 0%
- Each entry costs 4 bytes flash (two `uint16_t` fields)

**Accuracy depends on:**
1. How well the table matches the real discharge curve
2. Number and placement of table points (more points in non-linear regions = less interpolation error)
3. Measurement conditions matching table assumptions (temperature, discharge rate, cell age)

---

## Profile 1: CR2032 Primary Lithium (3.0 V)

### Chemistry

CR2032 is a primary (non-rechargeable) lithium manganese dioxide coin cell. Nominal voltage 3.0 V, typical capacity 210-240 mAh.

### Discharge Characteristics

The CR2032 discharge curve has two distinct regions:

1. **Gradual slope** (3000-2600 mV): Covers ~70% of capacity. Voltage drops slowly and roughly linearly, making this region well-suited to LUT interpolation with moderate point density.

2. **Accelerating drop** (2600-2000 mV): Covers the remaining ~30%. Voltage falls increasingly fast as the cell is exhausted. Higher point density is needed here but the practical value is limited — at these voltages the device should be in low-power or shutdown mode.

### Data Sources

- Energizer CR2032 datasheet (discharge curves at various loads)
- Empirical measurements on nRF52840-DK with fresh Energizer CR2032

### Table Points (9 entries, 36 bytes flash)

| mV   | SoC%  | Region | Rationale |
|------|-------|--------|-----------|
| 3000 | 100%  | Full   | Nominal fresh cell voltage |
| 2900 |  90%  | Slope  | Linear region, 100 mV spacing |
| 2800 |  70%  | Slope  | Voltage starts curving, SoC drops faster per mV |
| 2700 |  50%  | Slope  | Midpoint of usable range |
| 2600 |  30%  | Slope  | End of gradual region |
| 2500 |  20%  | Knee   | Transition to steeper drop |
| 2400 |  10%  | Drop   | Low battery warning zone |
| 2200 |   5%  | Drop   | Near-empty, 200 mV gap captures steep curve |
| 2000 |   0%  | Cutoff | Below this voltage most MCUs brown out |

### Known Limitations

- Based on room-temperature, low-current discharge (~1 mA average). High-pulse loads (BLE TX bursts) cause voltage sag that temporarily understates SoC.
- CR2032 capacity drops significantly below 0 °C. The table does not compensate for temperature.
- Self-discharge and cell age are not accounted for.
- The 3000 mV = 100% assumption is conservative; fresh cells can read 3.1-3.2 V under light load.

---

## Profile 2: LiPo Single-Cell (3.7 V Nominal)

### Chemistry

Lithium polymer (LiCoO2/graphite), single cell. Nominal voltage 3.7 V, charge cutoff 4.2 V, discharge cutoff 3.0 V. This is the most common rechargeable chemistry in consumer IoT, wearables, and hobbyist electronics.

### Discharge Characteristics

The LiPo single-cell OCV curve is distinctly non-linear with three regions:

```
Voltage (mV)
4200 ┤ ●
4150 ┤  ●                          Region 1: Initial drop
4060 ┤    ●                         4200-4050 mV → 100%-85%
3980 ┤      ●                       Fast voltage fall after full charge
3920 ┤        ●
3870 ┤          ●                   Region 2: Flat plateau
3830 ┤            ●                 4050-3700 mV → 85%-10%
3790 ┤              ●               Most usable energy (~75% of capacity)
     │                ·             Small voltage change = large SoC change
3700 ┤                  ●           Region 3: Steep cliff
     │                    ·         3700-3000 mV → 10%-0%
3500 ┤                      ●       Rapid voltage collapse
     │                        ·     Cell is nearly empty
3000 ┤                          ●   Hard cutoff — going below risks damage
     └────────────────────────────
       100  90  80  60  40  20  0   SoC (%)
```

**Region 1 — Initial drop (4200-4050 mV, ~100%-85%):** After a full charge at 4.2 V the cell quickly drops 100-150 mV under load. This region is traversed fast and covers only ~15% of capacity.

**Region 2 — Flat plateau (4050-3700 mV, ~85%-10%):** The voltage changes only ~350 mV while delivering roughly 75% of the cell's total capacity. This is both the most useful region and the hardest for voltage-based SoC estimation — a 10 mV measurement error can translate to 5-10% SoC error.

**Region 3 — Steep cliff (3700-3000 mV, ~10%-0%):** Voltage drops rapidly. A cell at 3.5 V has only ~3% remaining and at 3.0 V is at the hard cutoff. Discharging below 3.0 V risks permanent capacity loss.

### Data Sources

Multiple sources were cross-referenced to build a consensus curve. No single source was used in isolation because published curves vary with discharge rate, temperature, and cell age.

| Source | Key Data Points | Notes |
|--------|----------------|-------|
| Grepow LiPo voltage guide | Full percentage-to-voltage mapping table | Manufacturer data, well-cited in hobbyist community |
| RC community resting-voltage tables | OCV measurements at various SoC levels | Practical measurements across many cell brands |
| Adafruit Li-Ion/LiPoly documentation | Charge/discharge curves, safe operating limits | Widely used reference for maker/IoT projects |
| General lithium-ion electrochemistry | Three-region curve shape, 4.2 V/3.0 V bounds | Textbook knowledge, validates source data |

### Consensus Findings

Cross-referencing the sources produced consistent agreement on these key points:

- **4.20 V = 100%** — universal charge termination voltage
- **4.10-4.15 V = 90-95%** — initial fast drop after removing charger
- **3.80-3.85 V ≈ 50%** — rough midpoint of usable capacity
- **3.70 V ≈ 10%** — onset of steep cliff, low-battery threshold
- **3.50 V ≈ 3%** — critically low, should trigger shutdown
- **3.00 V = 0%** — hard cutoff, below this risks cell damage

The sources diverged most in the plateau region (3.8-4.0 V) where small voltage differences correspond to large SoC swings. This is an inherent limitation of voltage-based SoC estimation for LiPo cells.

### Table Point Selection Strategy

With 11 points (44 bytes flash), the goal was to minimise worst-case interpolation error across all three curve regions:

**Plateau region (7 points, 60-80 mV spacing):** Dense spacing because the curve is nearly flat — small voltage measurement errors produce large SoC errors. More points reduce the maximum interpolation error between any two adjacent entries.

**Knee/cliff region (3 points, 90-200 mV spacing):** The curve is steep so voltage changes rapidly with SoC. Wider spacing is acceptable because interpolation error per-mV is naturally lower. An extra point at 3500 mV captures the cliff shape.

**Endpoints (2 points):** Fixed at 4200 mV (full charge) and 3000 mV (cutoff).

### Table Points (11 entries, 44 bytes flash)

| mV   | SoC%  | Region       | Rationale |
|------|-------|-------------|-----------|
| 4200 | 100%  | Full charge  | CC/CV charge termination voltage |
| 4150 |  95%  | Initial drop | Captures fast initial voltage fall |
| 4060 |  85%  | Initial drop | Transition into plateau |
| 3980 |  75%  | Plateau      | Upper plateau, 60 mV spacing |
| 3920 |  65%  | Plateau      | Mid-upper plateau |
| 3870 |  55%  | Plateau      | Center of plateau, ~50% marker |
| 3830 |  45%  | Plateau      | Mid-lower plateau |
| 3790 |  30%  | Plateau end  | Plateau-to-knee transition |
| 3700 |  10%  | Knee         | Low battery threshold, cliff onset |
| 3500 |   3%  | Steep cliff  | Critically low, 200 mV from cutoff |
| 3000 |   0%  | Cutoff       | Hard discharge limit |

### Interpolation Accuracy

The piecewise-linear approach introduces error wherever the real curve deviates from a straight line between two adjacent table points. Worst-case error by region:

| Region | Max interpolation error | Notes |
|--------|------------------------|-------|
| Initial drop (4200-4060 mV) | ~2-3% SoC | Curve is slightly concave, linear is good enough |
| Plateau (4060-3790 mV) | ~3-5% SoC | Flatness makes voltage-based SoC inherently imprecise here |
| Knee (3790-3700 mV) | ~2-3% SoC | Steep but fairly linear |
| Cliff (3700-3000 mV) | ~1-2% SoC | Very steep, large mV per %SoC, low absolute error |

The plateau region is where voltage-based estimation is fundamentally weakest. Future improvements (coulomb counting, Kalman filter, temperature compensation) will primarily target this region.

### Known Limitations

- **OCV only:** The table assumes open-circuit or very-low-current measurements. Under load, voltage sag understates SoC. A 500 mAh cell drawing 100 mA can sag 50-100 mV.
- **Room temperature only:** LiPo capacity and voltage both decrease at low temperatures. At 0 °C the same cell might show 3.7 V at 20% SoC instead of 10%.
- **Cell age not modelled:** As LiPo cells age (>300 cycles), internal resistance increases and the plateau flattens further, reducing accuracy.
- **No hysteresis compensation:** Charge and discharge OCV curves differ slightly (charge curve is ~20-50 mV higher at same SoC). The table represents discharge only.
- **Generic profile:** This is a consensus curve for typical LiCoO2/graphite cells. Specific cells (different manufacturer, chemistry variant, or capacity) may deviate by 20-50 mV at certain SoC levels.

---

## Adding a New Battery Profile

To add a profile for a new chemistry or cell type:

1. **Research the discharge curve.** Cross-reference at least 2-3 sources (datasheets, community measurements, electrochemistry references). Prefer OCV data at room temperature and low discharge rates.

2. **Identify curve regions.** Most chemistries have distinct regions (plateau, knee, cliff). Understanding these guides point placement.

3. **Pick table points.** Place more points in non-linear regions where interpolation error is highest. Aim for 9-15 entries (36-60 bytes flash). Endpoints must match the cell's charge termination and discharge cutoff voltages.

4. **Add the data.** In `battery_soc_lut.c`:
   ```c
   static const battery_soc_lut_entry_t my_entries[] = {
       { voltage_mv, soc_pct_x100 },  /* highest voltage first */
       ...
       { voltage_mv, 0 },              /* cutoff */
   };

   const battery_soc_lut_t battery_soc_lut_my_chemistry = {
       .entries = my_entries,
       .count   = sizeof(my_entries) / sizeof(my_entries[0]),
   };
   ```

5. **Declare in header.** Add `extern const battery_soc_lut_t battery_soc_lut_my_chemistry;` to `battery_soc_lut.h`.

6. **Write tests.** Add tests to `test_soc_lut.c` covering:
   - Exact table points (at least top, bottom, and one midpoint)
   - Clamping above max and below min
   - Interpolation in each distinct curve region

7. **Document.** Add a section to this file with sources, rationale, and known limitations.

---

## Future Work

- **Temperature-compensated tables:** Ship multiple LUTs per chemistry (e.g., -10 °C, 0 °C, 25 °C, 45 °C) and interpolate between them based on measured temperature.
- **Lab-validated profiles:** Replace synthesised data with lab-measured discharge curves on specific cells at controlled discharge rates and temperatures.
- **LiFePO4 profile:** Flat 3.2-3.3 V plateau makes voltage-based SoC especially challenging — may need coulomb counting or combined estimator.
- **NiMH profile:** 1.2 V nominal, very flat discharge curve, similar challenges to LiFePO4.
- **Runtime profile selection:** Allow the application to select a battery profile at init time instead of compile-time hardcoding.
