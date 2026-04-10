# iBattery SDK — Claude Code Context

Embedded C SDK providing battery intelligence for IoT devices. Targets nRF52840, STM32L476, and ESP32-C3 (Zephyr RTOS) with CR2032 and LiPo support. Full pipeline: firmware → BLE → Python gateway → InfluxDB → Grafana.

## Current State

- **Version**: v0.7.0 (Phase 7 — ESP32-C3 port, hardware-validated)
- **GitHub**: https://github.com/aliaksandr-liapin/ibattery-sdk
- **License**: Apache 2.0
- **Platforms**: nRF52840-DK, NUCLEO-L476RG (STM32), ESP32-C3 DevKitM — all hardware-verified
- **Next milestone**: Voltage divider validation + advanced SoC (coulomb counting)

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

11 test suites, Unity framework.

### Python gateway tests

```bash
cd gateway && pip install -e . && pytest
```

58 tests across 6 files.

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

**Rules**: No heap allocation. Integer-only math (no FPU). ~48 bytes static RAM (core). HAL abstraction for portability.

## Key Kconfig Options

| Option | Values | Default |
|--------|--------|---------|
| `CONFIG_BATTERY_CHEMISTRY` | `CR2032`, `LIPO` | `CR2032` |
| `CONFIG_BATTERY_TEMP_NTC` | `y/n` | `y` (external NTC on AIN1) |
| `CONFIG_BATTERY_TEMP_DIE` | `y/n` | `n` (on-chip die sensor) |
| `CONFIG_BATTERY_CHARGER_TP4056` | `y/n` | `n` (enable for TP4056 GPIO) |
| `CONFIG_BATTERY_TRANSPORT` | `y/n` | `y` (BLE notifications) |

## Wire Format

- **v1** (20 bytes): version, timestamp_ms, voltage_mv, temperature_c_x100, soc_pct_x100, power_state, status_flags
- **v2** (24 bytes): v1 + cycle_count (uint32 LE at offset 20)

Both formats accepted by gateway decoder (auto-detect by length).

## Known Quirks

- **SAADC channel re-setup**: nRF SAADC driver clobbers per-channel input-mux; both ADC HAL drivers call `adc_channel_setup()` before every `adc_read()`
- **Die temp sensor noise**: ~0.2-0.3°C jitter per 2s sample = ~6-9°C/min apparent rate. Temperature rate threshold set to 15°C/min to filter this.
- **CR2032 vs LiPo thresholds**: Anomaly detection uses chemistry-neutral thresholds (critical: 2.5V, low: 2.8V) to avoid false positives on CR2032 (~3.0V nominal)
- **BLE MTU**: Configured for 24 bytes (v2 wire format). Single connection only.

## Project Structure

```
app/              Zephyr application (main.c, prj.conf, Kconfig)
app/boards/       Per-board overlays and Kconfig (nrf52840dk, nucleo_l476rg)
include/          Public API headers (battery_sdk/*.h)
src/              Implementation (core/, core_modules/, hal/, intelligence/, telemetry/, transport/)
src/hal/helpers/  Platform-specific ADC config (battery_adc_platform.h)
tests/            C unit tests (Unity) + mocks/
gateway/          Python BLE gateway + analytics + Grafana dashboards
cloud/            Docker Compose (InfluxDB + Grafana)
docs/             ARCHITECTURE, SDK_API, TESTING, BATTERY_PROFILES, ROADMAP, RELEASE_NOTES
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
| `docs/RELEASE_NOTES.md` | Version history (v0.1.0 through v0.5.1) |
| `CONTRIBUTING.md` | Dev setup, code style, porting guide |
| `gateway/README.md` | Gateway CLI usage, analytics, architecture |
| `cloud/README.md` | Docker stack setup |
