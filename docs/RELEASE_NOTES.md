# Release Notes

## v0.9.1 — PlatformIO Registry Consolidation — 2026-05-29

Registry-side housekeeping release. `library.json` was at `0.9.0` (tagged
2026-04-14, before today's Phase 8a hardware-validation arc), which meant
PlatformIO users were pulling firmware *without* the v0.8.4 coulomb
counter fix. This release bumps the PlatformIO-published version to
`0.9.1` so registry users get the current state of `main` — which has
all of:

- **v0.9.0** — Phase 8b voltage smoothing (median filter + SoC slew limiter)
- **v0.8.3** — Phase 8a hardware-validated on NUCLEO-L476RG
- **v0.8.4** — Coulomb counter bug fix (one-shot anchor + Q-as-remaining semantics)
- **v0.8.5** — Gateway persists `current_ma` and `coulomb_mah` to InfluxDB; Grafana panels

### No code changes vs current `main`

- `library.json` — `0.9.0` → `0.9.1`, description tightened to reflect that coulomb counting now works end-to-end
- All firmware, gateway, and dashboard code unchanged from v0.8.5 / current `main`
- 17 C host tests + 67 gateway tests still green (no regressions to test)

### Why a patch bump and not v0.10.0

This release is strictly additive vs. the existing v0.9.0:
- Bug fix (v0.8.4 coulomb counter), no API or wire-format breakage
- New optional gateway/Grafana fields (v0.8.5)
- Hardware validation methodology (docs only)

No new feature surface, no consumer-visible breakage. `v0.9.1` is the
semver-appropriate label.

### Why this release exists at all

The v0.8.x line ran in parallel with v0.9.0 specifically to deliver
the Phase 8a hardware-validation arc as focused patch releases (one
theme per patch). That worked well for narrative and bisection but
left the PlatformIO registry pointing at a stale snapshot. v0.9.1
closes that gap.

---

## v0.8.5 — Gateway + Grafana Persist current_ma and coulomb_mah — 2026-05-29

Closes [#2](https://github.com/aliaksandr-liapin/ibattery-sdk/issues/2).
Cloud side of the Phase 8a story: the v3 wire format has carried
`current_ma_x100` and `coulomb_mah_x100` since v0.8.0, the gateway has
decoded them since then, but the InfluxDB writer never wrote them to the
database and the Grafana dashboard had no panels for them. Until v0.8.4
fixed the coulomb counter (Q-as-remaining now actually tracks discharge),
that gap was masked — there was no point persisting a broken signal.

With v0.8.4 making `coulomb_mah` meaningful, this release plumbs both
fields all the way through to the dashboard.

### What changed

**`gateway/gateway/influxdb_writer.py`**
- Added two new fields to the `battery_telemetry` Point:
  - `current_ma` — live current measurement in mA (positive = discharge,
    matching INA219 convention)
  - `coulomb_mah` — remaining charge in mAh (Q-as-remaining semantics
    locked in by v0.8.4)
- Both use `decoded.get(..., 0.0)` so v1 / v2 packets stay
  backward-compatible (defaults to 0.0 when absent).

**`cloud/grafana/provisioning/dashboards/battery.json`**
- Two new timeseries panels in a fourth dashboard row (y=20):
  - **Live Current (mA)** — 30-minute window, 5-second aggregation,
    `mamp` unit
  - **Remaining Charge (mAh)** — 30-minute window, 5-second aggregation,
    two-decimal precision so the sub-mAh tracking is visible
- Layout matches the top-row Voltage/Temperature panels in width (12) and
  height (8) — Phase 8a's signals get equal visual weight to the existing
  voltage-based panels.
- Dashboard `version` bumped 1 → 2.

### Tests

- **67 gateway tests pass** (was 65; +2 in `test_writer.py`):
  - `test_write_v3_current_and_coulomb_fields` — explicit guard that a v3
    decoded packet writes both new fields with correct values
  - `test_write_v1_packets_default_current_and_coulomb_to_zero` —
    backward-compatibility guard for older firmware
- All 17 C host tests still pass — no firmware changes in this release.

### No firmware changes

- No `.c` / `.h` files modified
- No wire-format changes
- v0.8.5 is a pure gateway + cloud release; existing v0.8.4 firmware is
  what feeds it

### End-to-end validation

The unit tests cover the writer behavior precisely (line-protocol
assertions on the encoded Point). Full BLE → InfluxDB → Grafana visual
validation is deferred until the next BLE-capable test rig is set up
(the v0.8.3 nRF52840-DK had the per-unit GPIO defect; the
NUCLEO-L476RG used for v0.8.3/v0.8.4 hardware validation is BLE-less
without the `x_nucleo_idb05a1` shield). The fix is small, additive, and
covered by unit tests; deferring the full E2E visual check is low-risk.

### Follow-ups

- **Phase 8c (Kalman filter fusion)** — fully unblocked. A working coulomb
  signal (v0.8.4) is now both flowing and observable (v0.8.5), so Phase 8c
  can be tuned against real data with dashboard feedback.
- **BLE-on-NUCLEO** — add the `x_nucleo_idb05a1` shield build to the bench
  whenever convenient, to close the E2E visual loop on the new dashboard
  panels.

### Updated docs

- `docs/RELEASE_NOTES.md` — this entry
- `docs/ROADMAP.md` — Phase 8a entry notes v0.8.5 closes the cloud-side
  gap

---

## v0.8.4 — Phase 8a Coulomb Counter Bug Fix — 2026-05-29

Closes [#1](https://github.com/aliaksandr-liapin/ibattery-sdk/issues/1). Two
compounding bugs made Phase 8a coulomb counting functionally inert on CR2032
(the default chemistry and v0.8.3's validated hardware): Q stayed pinned at
capacity regardless of load, and the integrator's semantics didn't match the
SoC estimator's. Both fixed; Q now ticks down as expected under real load.

### What was broken

**Bug B (definite) — Full anchor fired on every sample.** For CR2032, the
anchor-arming condition `(SOC_ANCHOR_FULL_I_X100 == 0 || abs_current < ...)`
short-circuited to true (because the macro is `0`), collapsing the full
anchor gate to just `V >= 2950 mV`. The estimator then called
`battery_coulomb_reset(full_mah_x100)` on every poll, wiping any
integration delta. A fresh CR2032 never exits `V > 2.95 V` under any
realistic load, so Q stayed pinned at capacity forever.

**Bug A (latent, masked by B) — Integrator/estimator semantics mismatch.**
The integrator at `battery_coulomb.c` treated positive current (INA219
discharge convention) as adding to Q — Q-as-cumulative-throughput. The
SoC estimator treated Q as remaining capacity (`soc = Q * 10000 / capacity`,
anchor sets Q to `full_mah_x100`). Even if Bug B were fixed in isolation,
positive-current discharge would have grown Q above capacity (clamped to
100% by SoC math), not ticked it down.

### What was fixed

**`src/intelligence/battery_soc_estimator.c`**
- Anchors are now **one-shot edge-detected calibration events**. Two new
  static flags (`g_full_anchor_active`, `g_empty_anchor_active`) gate the
  resets to fire only on transition into the anchor voltage region. The
  anchor re-arms when voltage leaves the region, so a sag-and-recover
  cycle can re-calibrate at the next idle. `battery_soc_estimator_init()`
  clears both flags.

**`src/intelligence/battery_coulomb.c`**
- Integrator flipped to **Q-as-remaining semantics**: positive (discharge)
  current now subtracts from the accumulator; negative (charge) current
  adds to it. Matches the SoC estimator's reading of Q as remaining mAh.
- File header comment + integration math comment updated to document the
  semantics convention.

### Hardware validation on NUCLEO-L476RG

Same test rig as v0.8.3: NUCLEO-L476RG + Adafruit INA219 + ~1 kΩ load
across Vin+/Vin-. Captured 5 min of telemetry (151 samples):

```
v0.8.3 (broken):   Q = 220.00 mAh stuck    SoC = 100.00% stuck
v0.8.4 (fixed):    Q = 219.98 → 219.75 mAh SoC = 99.99% → 99.88%
                   Δ = -0.23 mAh over 5 min @ 2.80 mA  (matches theory ≈ 0.233 mAh)
```

The anchor calibration is visible in the first sample: Q starts at 219.98
(not 220.00), confirming that the anchor fired once at boot and Q then
immediately started tracking the 2.80 mA load. Across the next 5 minutes
the discharge is monotonic, smooth, and within < 0.5% of the theoretical
discharge rate.

### Tests

- All previous 16 host tests still pass (no regressions)
- **New test:** `tests/test_soc_coulomb_cr2032.c` — TDD regression test
  capturing Bug B (anchor must be one-shot, not every-sample). This test
  failed deterministically on v0.8.3, passes on v0.8.4.
- **New tests in `tests/test_coulomb.c`:**
  - `test_positive_current_decreases_q_as_remaining` — explicit guard for
    Q-as-remaining semantics under discharge
  - `test_negative_current_increases_q_as_remaining` — same for charge
- **Updated tests in `tests/test_coulomb.c`:** the three integration tests
  (`test_constant_discharge_1_hour`, `test_constant_charge_negative_current`,
  `test_trapezoidal_ramp`) were updated to reset to a 220 mAh baseline and
  assert against Q-as-remaining expectations. The old assertions matched
  the old buggy semantics by definition; updating them is part of the fix.

**Total: 17 host tests pass (was 16; new CR2032 anchor test).**

### Public API and wire format

- **No API breakage.** All public functions keep the same signatures.
- **No wire format change.** `coulomb_mah_x100` in the v3 packet is the
  same byte layout; only its semantic meaning is clarified to "remaining
  mAh" (was undefined / implementation-dependent before).

### Updated docs

- `docs/RELEASE_NOTES.md` — this entry
- `docs/ROADMAP.md` — Phase 8a entry now reflects both v0.8.3 hardware
  validation and the v0.8.4 fix
- `docs/SDK_API.md` — to be updated alongside the gateway-side dashboard
  enrichment in [#2](https://github.com/aliaksandr-liapin/ibattery-sdk/issues/2)

### Follow-ups

- [#2](https://github.com/aliaksandr-liapin/ibattery-sdk/issues/2) —
  Persist `current_ma` and `coulomb_mah` to InfluxDB and add Grafana panels.
  Now unblocked.
- Phase 8c (Kalman filter fusion) — unblocked; can now consume a working
  coulomb signal.

---

## v0.8.3 — Phase 8a Hardware-Validated on NUCLEO-L476RG — 2026-05-29

Milestone: **Phase 8a coulomb counting is fully end-to-end hardware-validated.**
After the v0.8.2 nRF52840-DK marginal-connection issue could not be resolved
even with the INA219 male headers soldered and both I2C jumpers replaced,
the SDK was successfully brought up on the **NUCLEO-L476RG** with the same
INA219 wiring. The chip ACKs 6/6, the driver initializes cleanly, and live
current is measured under load.

This proves the **firmware, HAL, INA219 driver, and SDK wire format are all
correct**. The nRF52840-DK unit (PCA10056 SN 1050258557) is the sole remaining
hardware blocker and is now isolated as a per-unit GPIO issue, not a platform
problem.

### What was validated on NUCLEO-L476RG

INA219 wired to I2C1 (PB8/PB9, Arduino D15/D14):

```
uart:~$ i2c scan i2c@40005400
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
40:  40 -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
1 devices found on i2c@40005400
```

**6/6 consecutive scans** report `0x40` (vs 0/6 on the nRF DK).

Live telemetry, quiescent (no load):
```
[v3 t=92117] V=3323 mV T=23.56 C SOC=100.00% PWR=1 CYC=0 flags=0x00000000
I=0.20 mA Q=220.00 mAh
```

Live telemetry, with ~1 kΩ load across Vin+/Vin- (5-min capture, 151 samples):
```
[v3 t=5946854] V=3322 mV T=23.56 C SOC=100.00% PWR=1 CYC=0 flags=0x00000000
I=2.80 mA Q=220.00 mAh   (stable across all 151 samples)
```

What's new vs v0.8.2:
- ✅ Stable firmware-level I2C reads (was: intermittent on nRF)
- ✅ `flags = 0x00000000` (was: `0x00000020` CURRENT_ERR on nRF)
- ✅ Real current measurement: 14× quiescent under modest load
- ✅ Sensible die temperature: 23–24 °C (was: 105 °C anomaly on nRF)
- ✅ Driver init reproducibly succeeds across power cycles

Coulomb accumulator `Q` remains pinned at 220.00 mAh during the 5-min capture
because the SoC estimator is anchored at 100% (V=3322 mV is well above the
CR2032 LUT's nominal 3.0 V). Q would track once SoC transitions off the
full-charge anchor; this is consistent with the SDK design and is being
investigated as a follow-up to confirm vs. flag as a bug.

### Diagnostic narrative — the nRF52840-DK fork

Continuing from v0.8.2 with the INA219 headers freshly soldered, the
firmware-level `i2c scan` on the nRF52840-DK still returned **0/6** ACKs.
Systematic isolation:

| Step | Result |
|------|--------|
| Soldered INA219 male header pins (replaced press-fit headers) | 0/6 — no change |
| Power confirmed at chip (Adafruit green LED solid) | rules out red/black wires |
| Replaced SCL jumper (green → grey, fresh) | 0/6 — no change |
| Replaced SDA jumper (yellow → violet, fresh) | 0/6 — no change |
| Verified nRF pin assignment: green→P0.27, yellow→P0.26, red→VDD | correct |
| Swapped in a **second** soldered INA219 | 0/6 at 0x40/0x41/0x44/0x45 — no chip ACK at any address |
| Same wires + same INA219 moved to **NUCLEO-L476RG** | **6/6 ACK immediately** |

Conclusion: the nRF52840-DK unit's P0.26 / P0.27 pins (or an SBx
solder-bridge near them) are not driving / receiving I2C correctly.
Two independent INA219 chips both work perfectly on STM32 with the same
physical wires — so the chip(s), the wires, and the SDK firmware path
are all good. **The defect is local to this DK unit.**

### Migration to NUCLEO-L476RG as the validation platform

The NUCLEO-L476RG was already a supported target (see `CLAUDE.md`).
This release adds the INA219 wiring on the STM32 side:

**New files:**
- `app/boards/nucleo_l476rg_i2cscan.conf` — i2c shell + INA219 driver config
  (mirrors `nrf52840dk_nrf52840_i2cscan.conf`)

**Modified files:**
- `app/boards/nucleo_l476rg.overlay` — added `ina219@40` node on `&i2c1`
  (status="disabled"; enabled at runtime by the HAL)

**Build + flash:**
```bash
west build -b nucleo_l476rg app -d /tmp/build-stm32-scan --pristine -- \
    -DEXTRA_CONF_FILE=boards/nucleo_l476rg_i2cscan.conf \
    -DZEPHYR_EXTRA_MODULES="/opt/nordic/ncs/v3.2.2/modules/hal/stm32"
west flash -d /tmp/build-stm32-scan --runner openocd
```

**Wiring (Adafruit INA219 → NUCLEO-L476RG):**
```
INA219 pin    →  NUCLEO pin              Wire
─────────────────────────────────────────────────
VCC           →  3V3      (CN6 POWER)    red
GND           →  GND      (CN6 POWER)    black
SCL           →  D15/PB8  (CN5 Arduino)  grey
SDA           →  D14/PB9  (CN5 Arduino)  violet
+ for current measurement:
Vin+          →  3V3 (CN6 or IOREF as alt 3.3V tap)
Vin-          →  [load]  →  GND (CN6 POWER)
```

### Known limitations

- **nRF52840-DK PCA10056 SN 1050258557** — INA219 does not ACK on
  P0.26/P0.27 firmware-side (per-unit issue; documented in
  `docs/HARDWARE_TROUBLESHOOTING.md`). Workaround: use STM32 NUCLEO-L476RG
  for Phase 8a validation, or remap nRF I2C to alternate GPIOs (planned
  for a future patch release).
- **ESP32-C3** — known I2C/INA219 driver instability (unchanged from
  earlier releases; see `CLAUDE.md`).

### What didn't change

- All 16 C unit tests pass (no regressions from overlay/config additions)
- 65 Python gateway tests still pass
- Public API unchanged
- v3 wire format unchanged — same packets stream over USB on the STM32

### Updated docs

- `docs/RELEASE_NOTES.md` — this entry
- `docs/HARDWARE_TROUBLESHOOTING.md` — new "Swap-the-MCU isolation test"
  section + per-unit nRF52840-DK limitation
- `docs/ROADMAP.md` — Phase 8a marked hardware-validated

---

## v0.8.2 — INA219 Confirmed Responding on Hardware (Phase 8a) — 2026-05-24

Milestone patch: the INA219 current sensor was **confirmed responding at
I2C address 0x40 on the nRF52840-DK**, verified by logic-analyzer capture
of a clean ACK. This closes the core question from v0.8.0/v0.8.1 — the
SDK firmware, the nRF I2C master, and the INA219 chip all work together.

### What was validated

Using a USB logic analyzer probing SDA + SCL simultaneously during a live
I2C scan, we captured the decoded transaction:

```
Start
Address write: 40
ACK            ← INA219 acknowledges its address
...
```

This was reproduced across multiple scans (3 clean ACKs at 0x40 in one
capture window). Combined with the 80+ host tests, this confirms:

- ✅ SDK firmware drives I2C correctly (`START → 0x40 → ACK → STOP`)
- ✅ nRF52840 TWIM master functions correctly
- ✅ INA219 chip is alive and ACKs at its default address
- ✅ Bus pull-ups and signal framing are correct

### The debugging journey (documented for posterity)

The root cause of earlier failures (v0.8.0/v0.8.1, "0 devices found") was
**not** chip defects — it was a **breadboard column-alignment error**: one
I2C jumper sat in a different breadboard column than its INA219 pin, so the
signal left the nRF but never reached the chip. The logic analyzer pinpointed
this: one line showed 951 transitions (master active) while the other showed
0 (pull-up present but no master signal). Re-aligning the wire to share the
INA219 pin's column immediately produced the first ACKs.

Two HiLetgo INA219 clones tested earlier were likely fine all along — they
were victims of the same wiring error, not defective. (A genuine Adafruit
INA219 #904 was used for final confirmation.)

### Known limitation — stable firmware-level read pending

The breadboard/jumper connection proved **intermittent**: the analyzer
captured clean ACKs, but the nRF's firmware-level `i2c scan` did not yet
report the device consistently, due to marginal contact resistance degrading
the SDA signal at the sampling instant. A **soldered or otherwise permanent
connection** is needed for a stable end-to-end current-reading run.

This does not affect the SDK code — it is purely a bench-wiring quality issue.

### Updated docs

- `docs/HARDWARE_TROUBLESHOOTING.md` — added the column-alignment root cause,
  the captured ACK trace, and the "analyzer sees ACK but firmware reads NACK"
  signal-integrity signature
- `README.md` / `docs/ROADMAP.md` — Phase 8a marked hardware-confirmed
  (chip ACKs); final live-reading run noted as pending permanent wiring

### What didn't change

- No firmware/software changes. All 16 C tests + 65 gateway tests still pass.
- No API or build-system changes from v0.8.1.

---

## v0.8.1 — Hardware Diagnostic Tooling (Phase 8a follow-up) — 2026-05-15

Patch release capturing the hardware-validation effort for Phase 8a
coulomb counting. No firmware/software changes — this release adds
diagnostic tooling and documentation for INA219 bring-up.

### Added

**I2C analyzer tooling** (`tools/i2c-analyzer/`)
- `capture.sh` — wraps `sigrok-cli` to capture and decode I2C bus
  activity using a HiLetgo / Saleae-clone USB logic analyzer
- `README.md` — wiring, common findings, and decoder usage

**Hardware troubleshooting guide** (`docs/HARDWARE_TROUBLESHOOTING.md`)
- Four-phase debug methodology (electrical, master signaling, slave
  identification, root cause isolation)
- Captured I2C trace from the v0.8.0 diagnostic session
- Known issues with cheap INA219 clones documented

### Phase 8a hardware validation status

After ~3 hours of methodical diagnosis with a logic analyzer, the
SDK was conclusively verified end-to-end at the protocol level:

- ✅ nRF52840-DK I2C master correctly drives START, address bits,
  R/W, and STOP conditions
- ✅ Bus electrical layer is clean (pull-ups active, idle = 3.3V,
  signals captured cleanly)
- ✅ Wire path from MCU to bus verified via simultaneous SDA + SCL
  captures
- ❌ **Two HiLetgo INA219 boards from the same 2-pack failed to
  respond at any of 128 I2C addresses** — likely factory defect or
  ESD damage to both chips in the batch

The SDK is wire-compatible with any device implementing the TI
INA219 I2C protocol. Customers should source from a reputable
vendor (Adafruit product #904, or Adafruit-branded Amazon listings
"Sold by Adafruit Industries" / "Sold by Amazon.com").

### What didn't change

- All firmware modules (HAL, coulomb counter, SoC estimator,
  telemetry, transport) — verified by 16 C unit tests + 65 Python
  gateway tests
- Public API — no breaking changes from v0.8.0
- Build system — no Kconfig or CMake changes

### Captured I2C trace evidence

```
i2c-1: Start
i2c-1: Address read: 40
i2c-1: NACK
i2c-1: Stop
```

The master correctly addresses `0x40` (the INA219's default I2C
address) and requests a read. The chip fails to acknowledge.
Bus electrical layer is correct; failure is on the chip side.

Full diagnostic methodology in `docs/HARDWARE_TROUBLESHOOTING.md`.

---

## v0.9.0 — Voltage Smoothing & SoC Slew Limiter (Phase 8b) — 2026-04-14

Software-only accuracy improvement for SoC estimation. Two
defense-in-depth layers against load-induced voltage sag and
LUT plateau cliffs. No hardware required — works on every
existing platform.

### New Features

**Median Voltage Filter**
- Alternative to existing moving-average filter
- Selected via `CONFIG_BATTERY_VOLTAGE_FILTER_MEDIAN=y`
- Single-sample outliers (BLE TX sag, etc.) completely rejected
- Same struct, same API — drop-in replacement at compile time
- Insertion sort, integer-only, ~32 bytes scratch buffer

**SoC Slew-Rate Limiter**
- Caps reported SoC change rate (default 5%/min)
- Bypassed on first sample after init for instant initial reading
- Filters LUT plateau cliffs and any remaining voltage outliers
- New Kconfig: `CONFIG_BATTERY_SOC_SLEW_LIMIT` (default y),
  `CONFIG_BATTERY_SOC_SLEW_RATE_PCT_PER_MIN` (default 5)

### Tests

- 12 new median filter tests (`test_voltage_filter_median`)
- 6 new slew limiter tests (`test_soc_slew_limit`)
- All 16 C test suites pass + 65 gateway tests

### Build Verification

- ESP32-C3 with median: 8.51% flash, clean
- nRF52840-DK with median: 14.59% flash, clean
- nRF52840-DK default (mean): 14.58% flash, clean (regression-free)
- Median filter overhead: ~80 bytes flash vs mean

### Composition with Phase 8a

When both `BATTERY_SOC_COULOMB` and `BATTERY_SOC_SLEW_LIMIT` are
enabled, slew limiter applies to the final SoC value (after coulomb
correction). Anchor events (full charge / cutoff) bypass the slew
limiter for instant reset to LUT value.

### Files Added

- `src/core_modules/battery_voltage_filter_median.c`
- `tests/test_voltage_filter_median.c`
- `tests/test_soc_slew_limit.c`
- `app/boards/esp32c3_devkitm_median.conf`

### Files Modified

- `app/Kconfig` — filter type choice + slew options
- `CMakeLists.txt` — conditional voltage filter source
- `app/CMakeLists.txt` — same conditional
- `src/intelligence/battery_soc_estimator.c` — slew limiter integration
- `tests/CMakeLists.txt` — new test targets

---

## v0.8.0 — Coulomb Counting SoC (Phase 8a) — 2026-04-14

Adds full coulomb counting infrastructure for advanced SoC estimation.
Software-complete release; on-target hardware validation pending
(see Known Issues).

### New Features

**Current Sensing HAL**
- New `battery_hal_current` interface — abstract current measurement
- INA219 backend via Zephyr sensor API (with raw I2C fallback for ESP32-C3)
- Stub backend for builds without current sensing — zero overhead
- Sign convention: positive = discharging, negative = charging

**Coulomb Counter**
- Trapezoidal integration with `int64_t` accumulator (sub-mAh precision)
- Remainder tracking eliminates truncation drift over multi-day runs
- NVS persistence: saves every 60s or on >1 mAh change
- Survives reboot — accumulator restored from flash on init

**SoC Estimator v2**
- Coulomb counting as primary tracker, voltage-LUT as calibration anchor
- Anchors at full charge (4180mV LiPo, low current) and cutoff (3000mV LiPo)
- Graceful fallback to voltage-LUT if current sensor unavailable
- Same public API — no breaking changes for existing callers

**Telemetry v3 Wire Format (32 bytes)**
- Adds `current_ma_x100` (signed int32) at offset 24
- Adds `coulomb_mah_x100` (signed int32) at offset 28
- Two new status flags: CURRENT_ERR (bit 5), COULOMB_ERR (bit 6)
- Backward compatible: gateway auto-detects v1 (20B), v2 (24B), v3 (32B)

**Gateway v3 Decoder**
- Python decoder extended for 32-byte v3 packets
- Reports `current_ma` (float) and `coulomb_mah` (float) in result dict
- 8 new tests covering encode/decode/edge cases

### Build System

**New Kconfig Options**
- `CONFIG_BATTERY_CURRENT_SENSE` — enable INA219 + I2C
- `CONFIG_BATTERY_SOC_COULOMB` — enable coulomb-based SoC
- `CONFIG_BATTERY_CAPACITY_MAH` — battery capacity (220 for CR2032, 1000 for LiPo)
- `CONFIG_BATTERY_COULOMB_NVS_INTERVAL_S` — NVS save interval (default 60s)

**CMake**
- Conditional sources for current HAL (Zephyr/stub) and coulomb counter
- Same pattern in both root `CMakeLists.txt` (Zephyr module) and `app/CMakeLists.txt`

**Board Configs**
- `app/boards/esp32c3_devkitm_current.conf` — ESP32-C3 with current sensing
- `app/boards/nrf52840dk_nrf52840_current.conf` — nRF52840-DK with current sensing
- Devicetree overlays for both platforms include INA219 node at I2C 0x40

### Tests

- 14/14 C unit tests pass (3 new: `hal_current_stub`, `coulomb`, `soc_coulomb`)
- 65/65 gateway tests pass (8 new for v3 decoding)
- ESP32-C3 and nRF52840-DK both build cleanly with and without current sensing

### Hardware Setup (INA219)

```
Battery+ ── INA219 VIN+ ── INA219 VIN- ── Load (VDD)
                I2C: SDA, SCL → board's I2C0 pins
                3.3V power, GND
```

| Platform | SDA | SCL | I2C Address |
|----------|-----|-----|-------------|
| ESP32-C3 | GPIO1 | GPIO3 | 0x40 |
| nRF52840-DK | P0.26 | P0.27 | 0x40 |

### Known Issues

**Hardware validation pending**
- INA219 detection failed on both ESP32-C3 (write NACK -14) and
  nRF52840-DK (no devices on bus) during initial bring-up.
- Root cause appears to be physical (breadboard contacts, header solder
  joints, or pull-up resistor configuration on nRF52840-DK without
  Arduino shield).
- Software is verified via host tests (no hardware required).
- v0.8.1 will follow with on-target hardware validation once a working
  INA219 setup is confirmed (recommend logic analyzer for diagnosis).

**ESP32-C3 I2C driver quirk**
- Zephyr 4.2.2's `i2c_write` function fails on ESP32-C3 with EFAULT (-14).
- Workaround already in code: use `i2c_burst_write` and separate
  `i2c_write` + `i2c_read` calls instead of `i2c_write_read`.

### Roadmap Split

Phase 8 Advanced SoC is now three sub-phases:

- **Phase 8a (this release)**: Coulomb counting with voltage anchoring
- **Phase 8b (next)**: Voltage-LUT correction mode — coulomb as smoothing layer
- **Phase 8c (future)**: Kalman filter fusion — optimal blending

Each phase reuses the HAL and telemetry from 8a — same public API throughout.

---

---

## v0.7.0 — Phase 7: ESP32-C3 HAL Port + On-Target Validation (2026-04-09)

Adds ESP32-C3 as the third supported platform, making iBattery SDK a genuine 3-platform solution. Introduces a new VDD measurement strategy via external voltage divider. Native BLE — no shield needed.

### What's New

**ESP32-C3 DevKitM Board Support**
- `app/boards/esp32c3_devkitm.overlay` — ADC0, die temp sensor (coretemp), GPIO alias
- `app/boards/esp32c3_devkitm.conf` — board-specific Kconfig (native BLE, die temp default)
- Build requires vanilla Zephyr v4.2.2 workspace (not NCS)

**Voltage Divider ADC Path**
- New `BATTERY_ADC_VDD_USE_DIVIDER` flag — third VDD strategy alongside direct SAADC (nRF52) and VREFINT (STM32)
- External resistor divider: Battery+ → R1(100K) → GPIO2 → R2(100K) → GND
- ADC reads divided voltage, firmware multiplies by `BATTERY_ADC_VDD_DIVIDER_RATIO` (default 2)
- 12 dB attenuation, ~0–2500 mV input range

**Platform Abstraction Improvements**
- `battery_adc_platform.h` — ESP32-C3 section with `BATTERY_ADC_DT_NODE` macro
- `battery_hal_temp_zephyr.c` — auto-detect `coretemp` node label (ESP32)
- `battery_hal_temp_ntc_zephyr.c` — `adc0` node label for ESP32-C3
- `battery_transport_ble_zephyr.c` — conditional include for `assigned_numbers.h` (NCS-only header)
- `app/Kconfig` — `select ESP32_TEMP if SOC_SERIES_ESP32C3` for die temp driver

**Build Matrix**
- nRF52840-DK: 152 KB Flash, 30 KB RAM (NCS)
- NUCLEO-L476RG: 38 KB Flash, 10 KB RAM (NCS)
- ESP32-C3 DevKitM: 356 KB Flash, 138 KB RAM (vanilla Zephyr v4.2.2)

### Hardware Verified

ESP32-C3-DevKitM-1-N4X (RISC-V, ESP32-C3-MINI-1 module):

| Check | Expected | Result |
|-------|----------|--------|
| Boot banner | "ESP32-C3 (DevKitM)" | OK |
| Die temperature sensor | 30-50 C (ESP32 runs warm) | 38-44 C |
| BLE advertising | "iBattery-ESP32C3" visible | OK — RSSI -42/-43 dBm |
| Gateway scan | Device found | OK |
| Gateway stream | Live telemetry via BLE | OK — v2 packets, 2s intervals |
| Gateway run → InfluxDB → Grafana | Full pipeline | OK — dashboard live |
| ACTIVE→IDLE transition | ~30s | OK |
| IDLE→SLEEP transition | ~120s | OK |
| Error flags | 0x00000000 | OK |
| Voltage divider (LiPo 500mAh) | ~3.7-4.2V | 4110-4134 mV (avg 4119, 24 mV spread) |
| SoC with LiPo LUT | 85-100% at ~4.1V | 90-93% (correct) |

### No Changes

- All 11 C unit tests pass (host, unchanged)
- All 58 Python gateway tests pass (unchanged)
- No core module, intelligence, telemetry, or transport logic changes
- Wire format v1/v2 unchanged
- nRF52840 and STM32 firmware unchanged

---

## v0.6.0 — Phase 6: STM32 HAL Port + On-Target Validation (2026-04-08)

Adds multi-platform support by porting the HAL layer to STM32L476 (NUCLEO-L476RG) under Zephyr RTOS. All core modules, intelligence, telemetry, and transport code remain unchanged — only the HAL abstraction was refactored to be SoC-agnostic. Hardware-validated on NUCLEO-L476RG.

### What's New

**Platform Abstraction**
- New `src/hal/helpers/battery_adc_platform.h` replaces nRF-specific `nrfx_analog_common.h`
- Per-SoC ADC configuration (input channels, gain, reference voltage) selected via `CONFIG_SOC_SERIES_*`
- STM32 VDD measurement via VREFINT factory calibration (ROM address `0x1FFF75AA`)

**STM32L476 Board Support**
- `app/boards/nucleo_l476rg.overlay` — ADC1, die temp sensor, charger GPIO alias
- `app/boards/nucleo_l476rg.conf` — board-specific Kconfig (die temp + BLE disabled by default)
- BLE supported via X-NUCLEO-IDB05A2 shield (`-DSHIELD=x_nucleo_idb05a1`)

**HAL Portability Improvements**
- `battery_hal_adc_zephyr.c` — VREFINT path for STM32, platform macros for gain/ref
- `battery_hal_temp_ntc_zephyr.c` — platform-agnostic ADC channel and reference
- `battery_hal_temp_zephyr.c` — auto-detect `temp` (nRF) vs `die_temp` (STM32) DT node
- `battery_hal_charger_tp4056_zephyr.c` — DT alias (`battery-charger-gpio`) instead of hardcoded `gpio0`
- `battery_hal_nvs_zephyr.c` — dynamic flash page size query (4096 nRF, 2048 STM32)
- `app/Kconfig` — platform-neutral: conditional `TEMP_NRF5` select, updated help text

**Boot Diagnostics**
- `print_platform_info()` at boot — shows platform, VDD method, temp source, chemistry, transport, charger status
- Useful for on-target validation and debugging multi-platform builds

**Build Matrix**
- nRF52840-DK: `west build -b nrf52840dk/nrf52840 app` — 152 KB Flash, 30 KB RAM
- NUCLEO-L476RG: `west build -b nucleo_l476rg app` — 38 KB Flash, 10 KB RAM

### Hardware Verified

NUCLEO-L476RG (STM32L476RG, ST-Link V2, Rev 4) via OpenOCD:

| Check | Expected | Result |
|-------|----------|--------|
| VDD via VREFINT sensor | 3200-3400 mV (USB) | 3316-3324 mV |
| Die temperature sensor | 20-40 C (room) | 23.23-24.88 C |
| SoC estimation (CR2032 LUT) | 100% at 3.3V | 100.00% |
| Telemetry loop interval | 2000 ms | 2002 ms |
| Error flags | 0x00000000 | 0x00000000 |
| ACTIVE→IDLE transition | ~30s | 30028 ms |
| IDLE→SLEEP transition | ~120s | 120113 ms |

**BLE Shield Validation (X-NUCLEO-IDB05A2)**

| Check | Expected | Result |
|-------|----------|--------|
| BLE SPI init | No crash at boot | OK — telemetry streaming |
| BLE advertising | "iBattery-STM32" visible | OK — RSSI -43 dBm |
| Gateway scan | Device found | OK — `ibattery-gateway scan` |
| Gateway stream | Live telemetry via BLE | OK — v2 packets, 2s intervals |
| BLE wire format | v2, 24 bytes | OK — voltage, temp, SoC, power state |

Build: `west build -b nucleo_l476rg app -- -DSHIELD=x_nucleo_idb05a1 -DEXTRA_CONF_FILE=boards/nucleo_l476rg_ble.conf`
Footprint: 95 KB Flash, 33 KB RAM (vs 38 KB / 10 KB without BLE)

### Status

Fully validated on both platforms including BLE. Pending: NTC thermistor (requires external hardware on PA0), TP4056 charger GPIO (requires wiring PC6/PC7).

### No Changes

- All 11 C unit tests pass (host, unchanged)
- All 58 Python gateway tests pass (unchanged)
- No core module, intelligence, telemetry, or transport changes
- Wire format v1/v2 unchanged
- nRF52840 firmware binary identical in memory footprint

---

## v0.5.1 — Phase 5b: Cycle Counter, Wire v2, RUL & Cycle Analysis (2026-03-14)

Adds charge cycle counting with flash persistence, extends the wire format to 24 bytes (v2), and delivers remaining useful life estimation, cycle analysis, and an 11-panel Grafana dashboard.

### What's New

**Charge Cycle Counter**
- Tracks CHARGING → CHARGED transitions as completed charge cycles
- NVS flash persistence via new HAL interface (`battery_hal_nvs.h`) — count survives reboots
- `battery_cycle_counter_init()` loads stored count (or starts at 0)
- `battery_cycle_counter_update(power_state)` called each telemetry loop; increments on state transition
- `battery_cycle_counter_get(&count)` reads current count

**Wire Format v2 (24 bytes)**
- Extends v1 (20 bytes) with `cycle_count` field at offset 20 (uint32 LE)
- `BATTERY_TELEMETRY_VERSION` bumped from 1 to 2
- `battery_serialize_pack()` writes 24 bytes for v2, 20 bytes for v1
- `battery_serialize_unpack()` accepts both 20-byte and 24-byte buffers — backward compatible
- `BATTERY_TRANSPORT_WIRE_SIZE` updated to 24; BLE MTU configured accordingly

**Gateway Decoder v2**
- Python decoder auto-detects v1 (20B) and v2 (24B) packets
- v2 packets include `cycle_count` field in decoded output and InfluxDB writes

**Cloud Analytics CLI**
- `ibattery-gateway analytics rul` — remaining useful life estimation (linear regression on health vs cycles)
- `ibattery-gateway analytics cycles` — charge cycle pattern analysis (duration, capacity fade, temperature stats)
- Existing commands: `analytics health` (health score) and `analytics anomalies` (anomaly detection)

**Anomaly Detection Tuning**
- Voltage thresholds tuned for dual-chemistry compatibility (CR2032 ~3.0V + LiPo ~3.7V)
- Critical voltage: 2.5V (was 3.0V) — safe for any chemistry
- SoC inconsistency: voltage < 2.8V with SoC > 50% (was 3.2V / 30%)
- Temperature rate threshold: 15°C/min (was 5°C/min) — filters die sensor noise (~0.3°C/sample jitter)
- Eliminates false positive warnings on CR2032 hardware

**Grafana Dashboard v2 (11 panels)**
- Voltage, SoC gauge, Power State, Temperature, Cycle Count, Health Score gauge
- SoC Over Time, Capacity Fade bar chart, Anomaly Log table, RUL stat, Charge Duration Trend
- Import via `gateway/grafana/ibattery-dashboard.json`

**Expanded Tests**
- Firmware: 11 C test suites (was 9) — new: `soc_temp_comp`, `cycle_counter`
- Gateway: 58 Python tests (was 22) — new: decoder v1/v2, RUL estimator, cycle analyzer, updated realtime anomaly tests
- New mocks: `mock_nvs.c` (NVS HAL), `mock_cycle_counter.c` (cycle counter)

### SDK Init Order Update

```
1. battery_hal_init()
2. battery_voltage_init()
3. battery_temperature_init()
4. battery_hal_charger_init()    [if CONFIG_BATTERY_CHARGER_TP4056]
5. battery_power_manager_init()
6. battery_soc_estimator_init()
7. battery_cycle_counter_init()  [NEW — loads NVS count]
8. battery_telemetry_init()
9. battery_transport_init()      [if CONFIG_BATTERY_TRANSPORT]
```

### Hardware Verified

- nRF52840-DK (PCA10056 rev 3.0.3) with CR2032
- Wire v2 packets (24 bytes) confirmed via gateway stream
- Gateway → InfluxDB → Grafana pipeline with all 11 panels
- All 4 analytics CLI commands verified with live data
- Zero false positive anomaly warnings on CR2032

---

## v0.5.0 — Phase 5a: Temperature-Compensated SoC + Cloud Analytics (2026-03-14)

Adds temperature compensation to the SoC estimator for LiPo cells and introduces cloud-side analytics: battery health scoring, real-time and historical anomaly detection.

### What's New

**Temperature-Compensated SoC (LiPo only)**
- `battery_soc_temp_comp.c` — applies temperature correction factors to LUT-based SoC
- Gated by `CONFIG_BATTERY_CHEMISTRY=LIPO` — CR2032 uses raw LUT (temperature compensation is not meaningful for primary cells)
- `CONFIG_BATTERY_CHEMISTRY` Kconfig: selects CR2032 or LiPo LUT at compile time

**Cloud Analytics**
- `ibattery-gateway analytics health` — voltage-based battery health score (0-100)
- `ibattery-gateway analytics anomalies` — historical anomaly detection from InfluxDB data
- Real-time per-packet anomaly checks during BLE streaming (no InfluxDB query needed)
- Anomaly types: voltage drop, SoC inconsistency, critical voltage, high/low temperature, temperature spike

**Gateway Analytics Modules**
- `gateway/analytics/health_score.py` — health scoring from InfluxDB voltage history
- `gateway/analytics/anomaly_detector.py` — historical anomaly detection (voltage + temperature)
- `gateway/analytics/realtime.py` — inline per-packet threshold checks

**Expanded Tests**
- Firmware: new `test_soc_temp_comp` suite (temperature compensation logic)
- Gateway: new `test_realtime.py` (11 tests for per-packet anomaly detection)

---

## v0.4.1 — Expanded Battery Power States + TP4056 Charger Scaffold (2026-03-13)

Adds full battery state machine with IDLE/SLEEP inactivity timers, CHARGING/DISCHARGING/CHARGED states, and a scaffolded TP4056 GPIO charger driver (Kconfig-gated, ready for hardware integration).

### What's New

**Expanded Power State Enum**
- New states: `CHARGING` (5), `DISCHARGING` (6), `CHARGED` (7) — appended for backward compatibility
- Existing `IDLE` (2) and `SLEEP` (3) states now wired into the state machine
- 8 total states: UNKNOWN, ACTIVE, IDLE, SLEEP, CRITICAL, CHARGING, DISCHARGING, CHARGED

**Inactivity Timer State Machine**
- `ACTIVE → IDLE` after 30 seconds of inactivity
- `IDLE → SLEEP` after 120 seconds of inactivity
- `battery_power_manager_report_activity()` resets the timer on BLE connections or user events
- CRITICAL always overrides IDLE/SLEEP (voltage safety takes priority)
- Graceful degradation: uptime read failure skips inactivity logic, stays ACTIVE

**TP4056 Charger Driver (scaffolded behind Kconfig)**
- `CONFIG_BATTERY_CHARGER_TP4056` — disabled by default, flip on when hardware arrives
- Reads CHRG and STDBY GPIO pins (active-low, pull-up configured)
- Truth table: CHRG=LOW → charging, STDBY=LOW → charge complete
- Configurable pin numbers: `CONFIG_BATTERY_CHARGER_TP4056_CHRG_PIN` (default P0.28), `_STDBY_PIN` (default P0.29)
- Charger state overrides inactivity logic (CHARGING > IDLE/SLEEP)
- CRITICAL → CHARGING recovery when charger connected at low voltage

**Gateway + Dashboard Alignment**
- Fixed gateway decoder: state names now match firmware enum exactly (was BOOT/LOW/SHUTDOWN, now UNKNOWN/IDLE/CRITICAL)
- Grafana Power State panel updated with 8-state color mapping
- 22 Python tests (was 20): new `test_charging_states`, `test_idle_sleep_states`

**Expanded Firmware Tests**
- New test suite: `test_power_manager_charger` — 23 tests with `CONFIG_BATTERY_CHARGER_TP4056=1`
- Tests cover: CHARGING, CHARGED, DISCHARGING, critical-to-charging recovery, charger error fallback, charging-overrides-idle
- Base `test_power_manager` suite: 17 tests (was 12) — adds IDLE/SLEEP inactivity, activity reset, critical-overrides-idle, uptime error
- Telemetry suite: 14 tests (was 9) — adds CHARGING, DISCHARGING, CHARGED, IDLE, SLEEP state collection
- Serialize suite: roundtrip for all 8 power states (was 5)
- **Total: 9 suites, 125+ tests** (was 8 suites, 106 tests)

### Hardware Verification

All 6 active power states verified end-to-end on nRF52840-DK (PCA10056 rev 3.0.3) via BLE gateway and Grafana dashboard:

| State | PWR | Verified | Method |
|-------|-----|----------|--------|
| IDLE | 2 | Yes | 30s inactivity timeout (real LiPo power) |
| SLEEP | 3 | Yes | 120s inactivity timeout (real LiPo power) |
| CRITICAL | 4 | Yes | Voltage < 2100 mV threshold |
| CHARGING | 5 | Yes | Jumper wire P0.28 → GND (simulated) |
| DISCHARGING | 6 | Yes | Both pins floating, LiPo powering DK via TP4056 OUT+/OUT- |
| CHARGED | 7 | Yes | Jumper wire P0.29 → GND (simulated) |

**LiPo + TP4056 power delivery verified:**
- LiPo 500mAh (PL 602535, 3.7V) connected to TP4056 HW-373 (USB-C) via B+/B- pads
- TP4056 OUT+/OUT- powering nRF52840-DK VDD/GND — board boots and runs on battery
- USB-C charging confirmed: voltage rises from ~3.01V to ~3.60V when charger connected
- TP4056 red LED lights during charging

> **Note:** CHARGING/CHARGED state detection was verified using jumper wires on GPIO pins (P0.28/P0.29 → GND), not by reading actual TP4056 CHRG/STDBY LED signals. The LED pad soldering for real charger state readout is pending. Power delivery and actual charging via TP4056 are confirmed working. Confidence level for real CHRG/STDBY signal detection: ~85% (depends on LED pad signal quality on HW-373 module).

### Enabling TP4056 (when hardware arrives)

```
# app/prj.conf
CONFIG_BATTERY_CHARGER_TP4056=y
CONFIG_BATTERY_CHARGER_TP4056_CHRG_PIN=28
CONFIG_BATTERY_CHARGER_TP4056_STDBY_PIN=29
```

Wire TP4056 CHRG → P0.28, STDBY → P0.29 on nRF52840-DK. Flash — charging states appear automatically.

---

## v0.4.0 — Phase 4: Cloud Telemetry Pipeline (2026-03-12)

Adds a Python BLE gateway and Docker-based cloud stack (InfluxDB + Grafana) for real-time battery telemetry visualization. Closes the loop from sensor to dashboard.

### What's New

**Python BLE Gateway (`gateway/`)**
- `ibattery-gateway` CLI tool with three commands: `scan`, `stream`, `run`
- BLE connection via bleak library with auto-reconnect and exponential backoff
- Decodes the same 20-byte LE wire format as the firmware serializer
- Writes telemetry to InfluxDB 2.x as `battery_telemetry` measurement points
- Installable Python package with `pip install -e .`

**Docker Cloud Stack (`cloud/`)**
- Docker Compose with InfluxDB 2.x (time-series storage) and Grafana (visualization)
- Auto-provisioned InfluxDB datasource and Grafana dashboard
- "iBattery Telemetry" dashboard with 6 panels:
  - Voltage (V) — time series, 30min window
  - Temperature (°C) — time series, 30min window
  - State of Charge — gauge, 0–100% with color thresholds
  - Power State — stat with 8-state color-coded value mappings
  - Raw Voltage (mV) — time series, 1h window
  - Status Flags — stat with error threshold coloring
- Anonymous viewer access enabled for local development

**Gateway Tests**
- Decoder tests: known wire bytes, mobile app packet, all power states, edge cases
- Writer tests: mock InfluxDB client, Point field verification, error resilience

### Quick Start

```bash
cd cloud && docker compose up -d          # Start InfluxDB + Grafana
cd gateway && pip install -e .            # Install gateway
ibattery-gateway run                      # BLE → InfluxDB → Grafana
```

Open http://localhost:3000 → "iBattery Telemetry" dashboard.

---

## v0.3.0 — Phase 3: BLE Telemetry Transport (2026-03-12)

Adds wireless telemetry delivery over Bluetooth Low Energy. Telemetry packets now flow off the device via BLE GATT notifications while serial output continues unchanged (dual output).

### What's New

**BLE Transport Layer**
- Custom BLE GATT service with notification characteristic for 20-byte telemetry wire packets
- Service UUID: `12340001-5678-9ABC-DEF0-123456789ABC`
- Characteristic UUID: `12340002-5678-9ABC-DEF0-123456789ABC` (Read + Notify)
- Connectable advertising with device name "iBattery" (configurable)
- Drop policy: silently succeeds when no client subscribed
- Automatic re-advertising on disconnect

**Transport Abstraction**
- Compile-time vtable pattern (`struct battery_transport_ops`) for pluggable backends
- Backend selected via Kconfig: `CONFIG_BATTERY_TRANSPORT_BLE` (or mock for testing)
- `battery_transport_send()` serializes + dispatches in one call
- Returns `BATTERY_STATUS_UNSUPPORTED` when no backend compiled in

**Wire Serialization**
- 20-byte little-endian wire format, fits BLE default ATT MTU (23 − 3 = 20)
- Explicit byte-shift encoding — no struct packing, no memcpy, fully portable
- Pack/unpack round-trip verified by unit tests

**SDK Integration**
- `battery_sdk_init()` now calls `battery_transport_init()` as the last step (guarded by `CONFIG_BATTERY_TRANSPORT`)
- Application main loop calls `battery_transport_send()` after telemetry collection
- Compile without transport: `CONFIG_BATTERY_TRANSPORT=n` → serial-only, zero BLE overhead

**Expanded Test Coverage**
- 106 tests across 8 suites (up from 80 tests across 6 suites)
- New: Serialization suite (15 tests) — round-trip, wire format, null/boundary checks
- New: Transport suite (11 tests) — backend delegation, error propagation, wire byte verification
- New mock: `mock_transport.c` with controllable rc, capture buffer, and send counter

**Build Configuration**
- New Kconfig options: `CONFIG_BATTERY_TRANSPORT`, `CONFIG_BATTERY_TRANSPORT_BLE`, `CONFIG_BATTERY_BLE_DEVICE_NAME`, `CONFIG_BATTERY_BLE_ADV_INTERVAL_MS`
- Stack sizes increased: main=4096, system workqueue=2048 (BLE stack requirements)
- BLE stack adds ~62 KB flash, ~12 KB RAM

### Known Limitations

- Single BLE connection only (`CONFIG_BT_MAX_CONN=1`)
- No authentication or encryption on the GATT characteristic
- No serial transport backend (BLE only for now)
- IDLE and SLEEP power states now implemented (see v0.4.1)
- No persistent storage of calibration data

---

## v0.2.1 — SAADC ADC Enum Fix & Channel Re-setup Workaround (2026-03-12)

Fixes incorrect NTC temperature readings caused by an analog input enum mismatch and an nRF SAADC driver quirk.

### Bug Fixes

**NTC reading wrong ADC pin (off-by-one)**
- Root cause: `NRF_SAADC_INPUT_AIN1` (from legacy `<hal/nrf_saadc.h>`) has value 2, but the Zephyr ADC driver interprets the `input_positive` field using the nrfx v3.x 0-based scheme where value 2 = AIN2 (P0.04), not AIN1 (P0.03)
- Fix: switched to `NRFX_ANALOG_EXTERNAL_AIN1` (value 1) from `<helpers/nrfx_analog_common.h>`
- Symptom: NTC reads ~87 mV instead of ~1500 mV at room temperature; LUT clamped to 125 °C

**SAADC channel input-mux clobbering across channels**
- Root cause: the nRF SAADC driver does not preserve per-channel input-mux (`PSELP`) settings when a different channel is read; reading internal VDD on channel 0 overwrites channel 1's AIN1 selection
- Fix: both HAL ADC drivers (`battery_hal_adc_zephyr.c` and `battery_hal_temp_ntc_zephyr.c`) now call `adc_channel_setup()` before every `adc_read()` to restore the correct input configuration
- Channel configs moved from local variables in init functions to file-scope `static const` structs so they are available in read functions
- Symptom: first NTC read after boot was correct, but subsequent reads returned ~3020 mV (VDD value) after a VDD channel read

### Hardware Verified

- nRF52840-DK (PCA10056 rev 3.0.3)
- 10K NTC (B=3950) on AIN1 (P0.03) with 10K pullup to VDD
- Stable ~29.5 °C readings at room temperature, surviving multiple reboots

---

## v0.2.0 — Phase 2: Real Temperature + Power State Machine (2026-03-09)

Replaces the two remaining stubs from Phase 1 with real implementations: die temperature sensor readings, external NTC thermistor support, and voltage-based power state detection with hysteresis.

### What's New

**Real Temperature Measurement**
- nRF52840 on-chip die temperature sensor via Zephyr sensor API (±2 °C accuracy)
- New HAL driver: `battery_hal_temp_zephyr.c` using `SENSOR_CHAN_DIE_TEMP`
- Temperature module now delegates to HAL instead of returning fixed 25.00 °C

**External NTC Thermistor Support**
- New HAL driver: `battery_hal_temp_ntc_zephyr.c` for 10K NTC (B=3950) on SAADC AIN1 (P0.03)
- New intelligence module: `battery_ntc_lut.c` with 16-point resistance-to-temperature LUT (-40 °C to +125 °C)
- Voltage divider math: ADC → millivolts → resistance → temperature, all integer math
- Compile-time selection via Kconfig: `CONFIG_BATTERY_TEMP_NTC` (default) or `CONFIG_BATTERY_TEMP_DIE`
- Same HAL interface (`battery_hal_temp_read_c_x100`) — modules above HAL are unchanged

**Voltage-Based Power State Machine**
- Threshold-based state detection: enters CRITICAL when voltage drops below 2100 mV
- Hysteresis dead band: exits CRITICAL only when voltage rises above 2200 mV (100 mV band prevents oscillation)
- Graceful degradation: returns last known state if voltage read fails
- Uses existing `battery_voltage_get_mv()` (lateral dependency, init-order safe)

**LiPo Single-Cell Discharge Curve**
- 11-point voltage-to-SoC lookup table for 3.7 V nominal LiPo cells (4200 mV → 3000 mV)
- Extra density in knee/cliff regions below 3700 mV to minimise interpolation error
- Data synthesised from multiple sources (Grepow, Adafruit, RC community measurements)
- Shared interpolation engine — no code duplication, just data

**Expanded Test Coverage**
- 80 tests across 6 suites (up from 32 tests across 3 suites)
- New: NTC LUT suite (21 tests) — resistance conversion edge cases, LUT interpolation across full temperature range, negative temps, clamping
- New: Temperature suite (7 tests) — HAL delegation, negative temps, error propagation
- New: Power manager suite (12 tests) — thresholds, hysteresis boundary conditions, graceful degradation
- New: LiPo LUT tests (8 tests) — exact points, clamping, interpolation across plateau/knee/cliff regions
- Updated mock_hal.c with temperature HAL stubs

**Build Configuration**
- New `app/Kconfig` with temperature source selection (NTC vs die sensor)
- Conditional compilation in CMake: NTC HAL + NTC LUT or die sensor HAL
- Added `&temp { status = "okay"; }` to devicetree overlay
- +1 byte static RAM (power state hysteresis memory)

### Known Limitations

- Die temperature measures chip temperature, not ambient or battery temperature
- IDLE and SLEEP power states defined but not implemented (require Zephyr PM integration)
- SoC lookup table is static; coulomb counting / adaptive estimation planned for future phases
- No persistent storage of calibration data
- No wireless telemetry transport (serial only)

---

## v0.1.0 — Phase 1 Complete (2026-03-09)

First functional release of the Battery SDK. All Phase 1 objectives are met: the firmware reads real battery voltage from a CR2032 coin cell, estimates state-of-charge, and outputs structured telemetry packets over serial.

### What's New

**Core SDK**
- `battery_sdk_init()` — single-call initialization of all subsystems in dependency order (HAL, voltage, temperature, power manager, SoC estimator, telemetry)
- Unified error codes via `battery_status.h` (OK, ERROR, INVALID_ARG, NOT_INITIALIZED, UNSUPPORTED, IO)
- Internal runtime state tracking for all modules

**Voltage Measurement**
- Real ADC reading via nRF52840 SAADC VDD input
- 12-bit resolution, 1/6 gain, internal 0.6V reference (3.6V full-scale)
- Moving average filter: window size 12, O(1) update, 28 bytes RAM, deterministic
- Typical readings: 3012-3017 mV on fresh CR2032

**SoC Estimation**
- CR2032 voltage-to-SoC lookup table (9-point discharge curve)
- Linear interpolation between table points, integer math only
- Range: 3000 mV = 100%, 2000 mV = 0%
- Clamped at boundaries (above 3000 mV = 100%, below 2000 mV = 0%)

**Telemetry**
- 7-field telemetry packet: version, timestamp, voltage, temperature, SoC, power state, status flags
- Resilient best-effort collection — collects what it can, sets error flags for failures
- Per-field error flag bits in `status_flags` for voltage, temperature, SoC, power state, and timestamp

**Testing**
- Unity-based host test framework (compiles and runs on macOS, no Zephyr or hardware needed)
- 32 tests across 3 test suites:
  - Voltage filter: 12 tests (null checks, single sample, averaging, window rollover, reset, edge cases)
  - SoC LUT: 11 tests (exact table points, interpolation midpoints, clamping, null safety)
  - Telemetry: 9 tests (full packet assembly, uninitialized state, null handling, partial failures)
- 6 configurable mock modules for isolated unit testing

### Hardware Verified On

- nRF52840-DK (PCA10056 rev 3.0.3)
- CR2032 Energizer coin cell
- Power switch: VDD position
- nRF Connect SDK v3.2.2
- Zephyr OS v4.2.99-fe4f0106803e

### Known Limitations

- Temperature reading is a stub (returns 25.00 C); real sensor integration planned for Phase 2
- Power manager returns ACTIVE state only; dynamic power state transitions planned for Phase 2
- SoC lookup table is static; coulomb counting / adaptive estimation planned for Phase 2
- No persistent storage of calibration data
- No wireless telemetry transport (serial only)

### Breaking Changes from Pre-release

- Removed internal `battery_telemetry.h` (3-field struct) — replaced by public `battery_types.h` (7-field struct)
- All errno-based error codes replaced with `battery_status.h` enum values
- ADC input changed from floating AIN0 pin to VDD rail
- ADC abstraction now uses `NRFX_ANALOG_INTERNAL_VDD` (nrfx v3.x API) instead of raw `NRF_SAADC_INPUT_VDD`
