# Minimal Zephyr application that consumes ibattery-sdk as a MODULE and
# exercises the read-only Zephyr fuel_gauge driver (compatible
# "aliaksandr,ibattery-fuel-gauge", gated on CONFIG_BATTERY_FUEL_GAUGE_API).
# Build-only smoke test that the fuel_gauge driver compiles + links through the
# module-consumption path (root CMakeLists.txt + Kconfig.ibattery via
# zephyr/module.yml), NOT via the app's target_sources path.
#
# Build (no flash), with the SDK repo added as an extra module. The consumer's
# own app.overlay provides the fuel_gauge DT node; the board overlay below is
# the platform overlay (ADC etc.):
#   west build -b esp32c3_devkitm tests/module_consumer_fuel_gauge -d build-mod-fg --pristine -- \
#     -DZEPHYR_EXTRA_MODULES="<sdk-repo-root>" \
#     -DEXTRA_DTC_OVERLAY_FILE="<sdk-repo-root>/app/boards/esp32c3_devkitm.overlay"

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ibattery_module_consumer_fuel_gauge)

target_sources(app PRIVATE src/main.c)
