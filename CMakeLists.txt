# iBattery SDK — Zephyr Module CMakeLists.txt
#
# This file is used when ibattery-sdk is consumed as a Zephyr module
# (via west.yml or ZEPHYR_EXTRA_MODULES). The app/ directory contains
# the sample application CMakeLists.txt.

if(CONFIG_BATTERY_SDK)

zephyr_library()

zephyr_library_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core_modules
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hal
    ${CMAKE_CURRENT_SOURCE_DIR}/src/intelligence
    ${CMAKE_CURRENT_SOURCE_DIR}/src/telemetry
    ${CMAKE_CURRENT_SOURCE_DIR}/src/transport
)

zephyr_library_sources(
    src/core/battery_sdk.c
    src/core_modules/battery_adc.c
    src/core_modules/battery_power_manager.c
    src/core_modules/battery_temperature.c
    src/core_modules/battery_voltage.c
    src/core_modules/battery_voltage_filter.c
    src/intelligence/battery_soc_estimator.c
    src/intelligence/battery_soc_lut.c
    src/intelligence/battery_cycle_counter.c
    src/telemetry/battery_telemetry.c
    src/hal/battery_hal_zephyr.c
    src/hal/battery_hal_adc_zephyr.c
)

zephyr_library_sources_ifdef(CONFIG_BATTERY_SOC_TEMP_COMP
    src/intelligence/battery_soc_temp_comp.c
)

zephyr_library_sources_ifdef(CONFIG_BATTERY_TRANSPORT
    src/transport/battery_transport.c
    src/transport/battery_serialize.c
)

zephyr_library_sources_ifdef(CONFIG_BATTERY_TRANSPORT_BLE
    src/transport/ble/battery_transport_ble_zephyr.c
)

zephyr_library_sources_ifdef(CONFIG_BATTERY_CHARGER_TP4056
    src/hal/battery_hal_charger_tp4056_zephyr.c
)

# NVS HAL: real Zephyr NVS or stub
if(CONFIG_BATTERY_CYCLE_COUNTER)
    zephyr_library_sources(src/hal/battery_hal_nvs_zephyr.c)
else()
    zephyr_library_sources(src/hal/battery_hal_nvs_stub.c)
endif()

# Current sensing HAL: Zephyr sensor API or stub
if(CONFIG_BATTERY_CURRENT_SENSE)
    zephyr_library_sources(src/hal/battery_hal_current_zephyr.c)
else()
    zephyr_library_sources(src/hal/battery_hal_current_stub.c)
endif()

# Coulomb counter
zephyr_library_sources_ifdef(CONFIG_BATTERY_SOC_COULOMB
    src/intelligence/battery_coulomb.c
)

# Temperature HAL: NTC thermistor or internal die sensor
if(CONFIG_BATTERY_TEMP_NTC)
    zephyr_library_sources(
        src/hal/battery_hal_temp_ntc_zephyr.c
        src/intelligence/battery_ntc_lut.c
    )
else()
    zephyr_library_sources(
        src/hal/battery_hal_temp_zephyr.c
    )
endif()

zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

endif() # CONFIG_BATTERY_SDK
