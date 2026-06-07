# Use Cases, How-Tos & Edge Cases

A practical guide to *what iBattery does for you automatically*, *what you (the
integrator) must do yourself*, and *what it deliberately doesn't do (yet)*.

If you read only one section, read **[Developer responsibilities and edge
cases](#developer-responsibilities-and-edge-cases)** — it lists the things that
will surprise you if you don't know them.

---

## Mental model: two different questions

| Question | Name | How it's answered | Needs |
|---|---|---|---|
| "How full is the battery *right now*?" | **State of Charge (SoC)** | voltage → lookup curve, optionally refined by current integration | voltage (always); current sensor (optional, more accurate) |
| "How *worn out* is the battery?" | **State of Health (SoH)** | learns real usable capacity from one full→empty discharge | current sensor **+** a full→empty cycle |

SoC is like a fuel needle. SoH is like noticing the fuel *tank itself* has
shrunk with age. They are independent.

---

## How-To: bring it up on your board

The **SDK logic and your app code are identical on every board.** You select the
board at build time and provide its small config; only the *physical* setup
differs.

| | nRF52840 | STM32L4 | ESP32-C3 |
|---|---|---|---|
| Battery-voltage sensing | built-in (reads supply directly) | built-in (internal reference) | **external resistor divider required** on a GPIO |
| Wireless (BLE) | built-in | needs an **add-on BLE shield** | built-in |
| Build toolchain | Nordic NCS | Nordic NCS | vanilla Zephyr workspace |
| Per-board config | board overlay | board overlay | board overlay |

**Steps (any board):**
1. Add iBattery as a Zephyr module (see the project [README](https://github.com/aliaksandr-liapin/ibattery-sdk#use-as-a-zephyr-module-in-your-own-project) "Use as a Zephyr module" section) and set `CONFIG_BATTERY_SDK=y`.
2. Pick your chemistry: `CONFIG_BATTERY_CHEMISTRY_CR2032` (default) or `CONFIG_BATTERY_CHEMISTRY_LIPO`.
3. Handle the board's one physical quirk (ESP32-C3: wire the divider; STM32: add the BLE shield if you want wireless). See [WIRING.md](WIRING.md).
4. Call `battery_sdk_init()` once, then `battery_telemetry_collect()` on your cadence. That's the whole core API.
5. *(Optional)* enable current sensing + SoH (below) if you want capacity/health, not just charge level.

---

## Use cases

### A. Fresh primary coin cell (CR2032)
- **Automatic:** reports SoC (~100% when fresh) from voltage; raises the **CRITICAL** power state as voltage nears the ~2 V cutoff.
- **What you get:** a fuel gauge + a low-battery signal so you can swap before the device dies.
- **Note:** a coin cell is single-use — no charging to manage. SoH starts at 100% (assumed new) and won't produce a *measured* number until a full→empty discharge (which for a primary cell is end-of-life), so for coin cells the value is the fuel gauge + early warning, not health learning.

### B. Used / partly-spent coin cell
- **Automatic:** because voltage is already lower, SoC reads **below 100%** immediately and you hit the CRITICAL warning sooner.
- **⚠️ You must know:** iBattery **cannot** tell a used cell's true remaining capacity the moment you insert it, and a health value learned from a *previous* cell does **not** apply to this one. **If SoH is enabled, call `battery_soh_reset()` on a battery swap** (see edge cases) — otherwise it keeps reporting the old battery's health and blends the next measurement into it.

### C. Permanent USB power
- **Automatic:** voltage reads the supply rail (~3.3 V), so SoC sits at ~100% and the state is steady. Nothing breaks; it just reports "full."
- **When it's useful:** with a **backup/rechargeable battery + charger**, it detects CHARGING → CHARGED, counts cycles, and tracks the backup's health for when power drops. SoH learning correctly never fires while the pack is kept topped up (no full→empty cycle).
- **Pure USB, no battery:** iBattery isn't really needed — it'll just report full.
- **⚠️ Note:** the SDK does **not auto-detect** "I'm on USB." Power-source type is configured, not sensed (see edge cases).

### D. Rechargeable cell (LiPo) — where SoH shines
- This is the intended home for health learning: a battery that **lives in the device and ages in place over many cycles.**
- Enable `CONFIG_BATTERY_CHEMISTRY_LIPO`, current sensing, and SoH (below). Over real full→empty excursions it learns the cell's fading capacity and persists it across reboots.
- With a TP4056 charger wired, it also tracks charge/discharge state and cycle count.

### Enabling current sensing + SoH (for B/D)
```
CONFIG_BATTERY_CURRENT_SENSE=y     # INA219 current sensor (see WIRING.md)
CONFIG_BATTERY_SOC_COULOMB=y       # charge counting
CONFIG_BATTERY_SOC_SOH=y           # State-of-Health learning
CONFIG_BATTERY_CAPACITY_MAH=<rated mAh of your cell>
```

---

## Developer responsibilities and edge cases

These are the "you must handle this" items. **None of them is a bug** — they're
deliberate boundaries of an on-device, integer-only library.

### 🔧 Battery swap → auto-detected for primary cells; manual otherwise
Learned health persists in flash and is **smoothed (moving-average)** into the
next measurement, and the SDK has **no per-cell identity.** If a swap goes
unnoticed it keeps reporting the **old** cell's health and **blends** the next
learned value with it (wrong for a fresh cell).

**Auto-detection (`CONFIG_BATTERY_SWAP_DETECT`, default on for CR2032).** On boot
the SDK compares SoC against a value it persisted before power-off; a large
upward jump (default ≥25%) means a fresh cell was inserted while powered off, so
it **auto-resets** learned SoH to rated and raises the
`BATTERY_TELEMETRY_FLAG_BATTERY_SWAPPED` status flag (mirrors a fuel-gauge IC's
reset-indicator on insertion). Tunable via
`CONFIG_BATTERY_SWAP_SOC_THRESHOLD_PCT_X100`.

**Still call `battery_soh_reset()` yourself when** auto-detection can't see it:
- **Rechargeable (LiPo):** `SWAP_DETECT` is **off by default** — a cell charged
  while powered off looks identical to a swap under the SoC-jump rule, so
  charge-vs-swap disambiguation is deferred (Phase 2). Reset on a known swap.
- **Hot-swap** (swapped while powered) — not detected this phase.
- **Similarly-depleted replacement** — a used cell with no upward SoC jump won't trip detection.

`battery_soh_reset()` (resets learned capacity to rated, disarms) remains the
explicit escape hatch; the SDK provides the API and, for primary cells, now also
triggers it automatically.

### 🔧 SoH only learns from a full→empty excursion (with a current sensor)
Health is *measured*, not guessed: it needs `CONFIG_BATTERY_CURRENT_SENSE` **and**
the battery to travel from the full-voltage region to the empty threshold once.
Until that happens it reports the last known / initial value (100% on a fresh
init). No current sensor → no SoH.

### 🔧 "Time remaining" (minutes-to-empty) is opt-in and needs the current sensor
A runtime-to-empty estimate **now exists**: enable opt-in
`CONFIG_BATTERY_RUNTIME_TO_EMPTY` (default off; requires
`CONFIG_BATTERY_SOC_COULOMB`, i.e. the INA219 current sensor). It reports minutes
to empty from the smoothed discharge current and the coulomb-counter remaining
charge, via `battery_runtime_to_empty_min()`, the Zephyr `fuel_gauge`
`RUNTIME_TO_EMPTY` property, and wire v5 (`runtime_to_empty_min`). It reports
**"not available"** whenever the cell is idle or charging, since no draw means no
meaningful estimate — so treat a missing value as expected, not an error.
Separately, the cloud `analytics rul` command estimates **remaining charge
*cycles*** (only for rechargeable cells that actually cycle — it returns
"insufficient data" for a coin cell). See [Roadmap](ROADMAP.md).

### 🔧 No human-language notifications on-device
The device emits a **power-state code** (ACTIVE / IDLE / SLEEP / **CRITICAL** /
CHARGING / DISCHARGING / CHARGED) and status flags — *signals*, not messages like
"plan a battery swap." Turning them into a user notification is your app's job
(or use the gateway's anomaly/health analytics).
- On-device, the only low-battery state is **CRITICAL** (one threshold).
- The separate **LOW (2.8 V)** vs **CRITICAL (2.5 V)** warnings live in the
  **gateway** anomaly detector, not on the device.

### 🔧 Power-source type is configured, not sensed
There's no automatic "USB vs battery" or "rechargeable vs primary" detection.
Chemistry is a build-time choice. You can *infer* a charger setup from CHARGING→
CHARGED transitions (only if a TP4056's status pins are wired), but the SDK won't
conclude "I'm on permanent power" for you.

### 🔧 SoC from voltage alone is approximate (esp. coin cells)
A coin cell's voltage curve is fairly flat, so voltage-only SoC is a rough guide.
Add a current sensor (coulomb counting + fusion) for materially better accuracy.

### 🔧 Per-board physical quirks (recap)
- **ESP32-C3:** must wire an external resistor divider for battery voltage.
- **STM32:** needs an add-on BLE shield for wireless.
- **nRF52840:** most plug-and-play.

---

## See also
- [SDK API Reference](SDK_API.md) — function signatures incl. `battery_soh_reset()` and the Zephyr `fuel_gauge` driver
- [Hardware Wiring Guide](WIRING.md) — per-board wiring incl. the INA219 current sensor and the divider
- [Roadmap](ROADMAP.md) — planned improvements to the gaps called out above (time-remaining, swap handling, power-source detection)
