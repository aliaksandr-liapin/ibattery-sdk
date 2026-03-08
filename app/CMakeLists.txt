cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(battery_sdk_app)

target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_sources(app PRIVATE
    src/main.c

    ../src/core/battery_sdk.c

    ../src/core_modules/battery_adc.c
    ../src/core_modules/battery_voltage.c
    ../src/core_modules/battery_temperature.c
    ../src/core_modules/battery_power_manager.c

    ../src/intelligence/battery_soc_estimator.c
    ../src/telemetry/battery_telemetry.c

    ../src/hal/battery_hal_zephyr.c
)