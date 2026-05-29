# iBattery SDK — Claude Code Context

Embedded C SDK providing battery intelligence for IoT devices. Targets nRF52840, STM32L476, and ESP32-C3 (Zephyr RTOS) with CR2032 and LiPo support. Full pipeline: firmware → BLE → Python gateway → InfluxDB → Grafana.

## Current State

- **Version**: v0.10.2 (Phases 8a + 8b + 8c shipped; BLE-on-NUCLEO E2E validated with the X-NUCLEO-IDB05A1 shield in v0.10.1; v0.10.2 = docs/packaging fix)
- **GitHub**: https://github.com/aliaksandr-liapin/ibattery-sdk
- **License**: Apache 2.0
- **Platforms**: nRF52840-DK, NUCLEO-L476RG (STM32), ESP32-C3 DevKitM — all hardware-verified
- **Phase 8a status**: ✅ Hardware-validated end-to-end on NUCLEO-L476RG. Coulomb counter ticks correctly under load (Q-as-remaining semantics, one-shot anchor); gateway persists `current_ma` and `coulomb_mah` to InfluxDB; Grafana dashboard has dedicated panels.
- **Phase 8b status**: ✅ Shipped in v0.9.0 — median voltage filter + SoC slew-rate limiter.
- **Phase 8c status**: ✅ Shipped in v0.10.0 — voltage+coulomb signal fusion with current-adaptive α, opt-in via `CONFIG_BATTERY_SOC_FUSION` (default n). +56 bytes flash, 0 new RAM. Hardware-validated with 3 captures including a load-vs-rest demonstration with 10 toggles cleanly detected.
- **v0.10.1 status**: ✅ BLE-on-NUCLEO end-to-end validated — first time v3 (32-byte) telemetry traversed BLE on real hardware. Fixed three bugs: BLE MTU too small for v3 (27→35, all platforms), gateway name-matching unreliable on macOS (now service-UUID via `find_device_by_filter`), and firmware not re-advertising after disconnect (now deferred to workqueue). Real current/coulomb reach Grafana over BLE. Evidence: `docs/captures/2026-05-29-v0.10.1-ble-on-nucleo-e2e.log`.
- **Known hardware limitation**: nRF52840-DK PCA10056 SN 1050258557 has a per-unit GPIO defect on P0.26/P0.27 — see `docs/HARDWARE_TROUBLESHOOTING.md` "swap-the-MCU" section.
- **BLE testing on macOS**: bleak (scan/stream/run) must run from a Bluetooth-permitted app like **iTerm**, NOT Claude Code — Claude.app lacks the Bluetooth TCC grant and the bare pyenv python has no `NSBluetoothAlwaysUsageDescription`, so CoreBluetooth calls SIGABRT. Non-BLE checks (serial, InfluxDB, Grafana) work from anywhere.
- **Next milestone**: open. Phase 8 series is complete. Future candidates: Phase 8d (capacity-aging learning, coulomb drift correction), new feature areas, or focused polish.
- **Distribution**: PlatformIO registry + Zephyr module + GitHub Pages docs

## Build Commands

### PATH setup (required — homebrew git MUST come before Nordic toolchain)

```bash
export PATH="/opt/homebrew/bin:/usr/bin:/bin:/opt/nordic/ncs/toolchains/e5f4758bcf/bin:$PATH"
export ZEPHYR_BASE="/opt/nordic/ncs/v3.2.2/zephyr"
export ZEPHYR_SDK_INSTALL_DIR="/opt/nordic/ncs/toolchains/e5f4758bcf/opt/zephyr-sdk"
```

### Firmware (nRF52840-DK)

```bash
west build -b nrf52840dk/nrf52840 app -d build-nrf --pristine
west flash -d build-nrf
```

### Firmware (NUCLEO-L476RG, no BLE)

```bash
west build -b nucleo_l476rg app -d build-stm32 --pristine -- \
  -DZEPHYR_EXTRA_MODULES="/opt/nordic/ncs/v3.2.2/modules/hal/stm32"
west flash -d build-stm32 --runner openocd
```

### Firmware (NUCLEO-L476RG + BLE shield)

```bash
west build -b nucleo_l476rg app -d build-stm32-ble --pristine -- \
  -DSHIELD=x_nucleo_idb05a1 \
  -DEXTRA_CONF_FILE=boards/nucleo_l476rg_ble.conf \
  -DZEPHYR_EXTRA_MODULES="/opt/nordic/ncs/v3.2.2/modules/hal/stm32"
west flash -d build-stm32-ble --runner openocd
```

### Firmware (NUCLEO-L476RG + BLE shield + INA219 current sense)

```bash
# Real current_ma/coulomb_mah over BLE (lights up Live Current + Remaining Charge panels)
west build -b nucleo_l476rg app -d build-stm32-ble-cur --pristine -- \
  -DSHIELD=x_nucleo_idb05a1 \
  -DEXTRA_CONF_FILE=boards/nucleo_l476rg_ble_current.conf \
  -DZEPHYR_EXTRA_MODULES="/opt/nordic/ncs/v3.2.2/modules/hal/stm32"
west flash -d build-stm32-ble-cur --runner openocd
```

### Firmware (ESP32-C3 DevKitM — requires vanilla Zephyr workspace)

```bash
# From ~/zephyr-esp32 workspace (not NCS)
cd ~/zephyr-esp32 && source .venv/bin/activate
export ZEPHYR_BASE=~/zephyr-esp32/zephyr
export ZEPHYR_SDK_INSTALL_DIR=/opt/nordic/ncs/toolchains/e5f4758bcf/opt/zephyr-sdk
west build -b esp32c3_devkitm /path/to/ibattery-sdk/app -d build-esp32c3 --pristine
west flash -d build-esp32c3
```

### C unit tests (host, no hardware needed)

```bash
cd tests && mkdir -p build && cd build
cmake .. && make && ctest --output-on-failure
```

14 test suites, Unity framework (includes coulomb counter, current HAL stub, SoC coulomb).

### Python gateway tests

```bash
cd gateway && pip install -e . && pytest
```

65 tests across 6 files (includes v3 packet decoding).

### Gateway CLI

```bash
cd gateway && pip install -e .
ibattery-gateway scan          # Find BLE devices
ibattery-gateway stream        # Live BLE stream
ibattery-gateway run           # Stream + write to InfluxDB
ibattery-gateway analytics health
ibattery-gateway analytics anomalies
ibattery-gateway analytics rul
ibattery-gateway analytics cycles
```

**Important**: The CLI entry point is `ibattery-gateway`, NOT `ibattery`.

### Cloud stack

```bash
cd cloud && docker compose up -d    # InfluxDB + Grafana
# Grafana: http://localhost:3000 (admin/admin)
# InfluxDB: http://localhost:8086 (ibattery/ibattery123)
```

## Architecture

```
HAL (platform-specific)
  → Core modules (voltage filter, ADC, temperature)
    → Intelligence (SoC estimator, cycle counter, power manager)
      → Telemetry (packet assembly)
        → Transport (BLE GATT notifications)
```

**Rules**: No heap allocation. Integer-only math (no FPU). ~120 bytes static RAM (core + coulomb). HAL abstraction for portability.

## Key Kconfig Options

| Option | Values | Default |
|--------|--------|---------|
| `CONFIG_BATTERY_CHEMISTRY` | `CR2032`, `LIPO` | `CR2032` |
| `CONFIG_BATTERY_TEMP_NTC` | `y/n` | `y` (external NTC on AIN1) |
| `CONFIG_BATTERY_TEMP_DIE` | `y/n` | `n` (on-chip die sensor) |
| `CONFIG_BATTERY_CHARGER_TP4056` | `y/n` | `n` (enable for TP4056 GPIO) |
| `CONFIG_BATTERY_TRANSPORT` | `y/n` | `y` (BLE notifications) |
| `CONFIG_BATTERY_CURRENT_SENSE` | `y/n` | `n` (INA219 I2C current sensor) |
| `CONFIG_BATTERY_SOC_COULOMB` | `y/n` | `y` if CURRENT_SENSE (coulomb counting SoC) |
| `CONFIG_BATTERY_CAPACITY_MAH` | `int` | `220` (CR2032) / `1000` (LiPo) |

## Wire Format

- **v1** (20 bytes): version, timestamp_ms, voltage_mv, temperature_c_x100, soc_pct_x100, power_state, status_flags
- **v2** (24 bytes): v1 + cycle_count (uint32 LE at offset 20)
- **v3** (32 bytes): v2 + current_ma_x100 (int32 LE at offset 24) + coulomb_mah_x100 (int32 LE at offset 28)

All three formats accepted by gateway decoder (auto-detect by length).

## Known Quirks

- **SAADC channel re-setup**: nRF SAADC driver clobbers per-channel input-mux; both ADC HAL drivers call `adc_channel_setup()` before every `adc_read()`
- **Die temp sensor noise**: ~0.2-0.3°C jitter per 2s sample = ~6-9°C/min apparent rate. Temperature rate threshold set to 15°C/min to filter this.
- **CR2032 vs LiPo thresholds**: Anomaly detection uses chemistry-neutral thresholds (critical: 2.5V, low: 2.8V) to avoid false positives on CR2032 (~3.0V nominal)
- **BLE MTU**: `CONFIG_BT_L2CAP_TX_MTU=35` (= 32-byte v3 payload + 3-byte ATT header), ACL buffers 39. Earlier configs used 27, which silently capped notifications at 24 bytes and blocked v3 over BLE entirely. Single connection only.
- **ESP32-C3 I2C + INA219**: Zephyr INA219 driver fails at boot (I2C not stable). Raw I2C fallback implemented but breadboard contacts cause NACKs. Needs soldered connections or different breadboard.
- **nRF52840-DK I2C pull-ups**: Require Arduino power header GND connection to activate analog switch (SB32/SB33). Without this, I2C pull-ups are not enabled.

## Project Structure

```
app/              Zephyr application (main.c, prj.conf, Kconfig)
app/boards/       Per-board overlays and Kconfig (nrf52840dk, nucleo_l476rg, esp32c3_devkitm)
include/          Public API headers (battery_sdk/*.h)
src/              Implementation (core/, core_modules/, hal/, intelligence/, telemetry/, transport/)
src/hal/helpers/  Platform-specific ADC config (battery_adc_platform.h)
tests/            C unit tests (Unity) + mocks/
gateway/          Python BLE gateway + analytics + Grafana dashboards
cloud/            Docker Compose (InfluxDB + Grafana)
docs/             ARCHITECTURE, SDK_API, TESTING, WIRING, BATTERY_PROFILES, ROADMAP, RELEASE_NOTES
```

## Documentation Map

| Doc | What it covers |
|-----|----------------|
| `README.md` | Project overview, hardware, features, quick start |
| `docs/ARCHITECTURE.md` | Layer diagram, module responsibilities, data flow, memory budget |
| `docs/SDK_API.md` | Full public API reference with function signatures |
| `docs/TESTING.md` | Test procedures, suite descriptions, running instructions |
| `docs/BATTERY_PROFILES.md` | CR2032 + LiPo discharge curves, LUT design rationale |
| `docs/ROADMAP.md` | Business strategy, development priorities, monetization |
| `docs/RELEASE_NOTES.md` | Version history (v0.1.0 through v0.10.0) |
| `docs/plans/` | Design docs and implementation plans |
| `CONTRIBUTING.md` | Dev setup, code style, porting guide |
| `gateway/README.md` | Gateway CLI usage, analytics, architecture |
| `cloud/README.md` | Docker stack setup |
