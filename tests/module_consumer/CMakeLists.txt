# Minimal Zephyr application that consumes ibattery-sdk as a MODULE
# (root CMakeLists.txt + Kconfig.ibattery via zephyr/module.yml), NOT via the
# app's target_sources path. Build-only smoke test that the module-consumption
# path — the SDK's primary distribution mode — compiles and links.
#
# Build (no flash), with the SDK repo added as an extra module:
#   west build -b nucleo_l476rg tests/module_consumer -d build-mod --pristine -- \
#     -DZEPHYR_EXTRA_MODULES="<sdk-repo-root>" \
#     -DEXTRA_DTC_OVERLAY_FILE="<sdk-repo-root>/app/boards/nucleo_l476rg.overlay"

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ibattery_module_consumer)

target_sources(app PRIVATE src/main.c)
