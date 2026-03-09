cmake_minimum_required(VERSION 3.14)
project(ibattery_sdk_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── Fetch Unity test framework ──────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.0
)
FetchContent_MakeAvailable(unity)

# ── Paths ────────────────────────────────────────────────────────────────────
set(SDK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(SDK_INCLUDE ${SDK_ROOT}/include)
set(SDK_SRC ${SDK_ROOT}/src)

# ── Voltage filter test ──────────────────────────────────────────────────────
add_executable(test_voltage_filter
    test_voltage_filter.c
    ${SDK_SRC}/core_modules/battery_voltage_filter.c
)

target_include_directories(test_voltage_filter PRIVATE
    ${SDK_INCLUDE}
    ${SDK_SRC}/core_modules
    ${unity_SOURCE_DIR}/src
)

target_link_libraries(test_voltage_filter PRIVATE unity)

# ── SoC LUT test ─────────────────────────────────────────────────────────────
add_executable(test_soc_lut
    test_soc_lut.c
    ${SDK_SRC}/intelligence/battery_soc_lut.c
)

target_include_directories(test_soc_lut PRIVATE
    ${SDK_INCLUDE}
    ${SDK_SRC}/intelligence
    ${unity_SOURCE_DIR}/src
)

target_link_libraries(test_soc_lut PRIVATE unity)

# ── Telemetry test (with mocks) ──────────────────────────────────────────────
add_executable(test_telemetry
    test_telemetry.c
    ${SDK_SRC}/telemetry/battery_telemetry.c
    mocks/mock_hal.c
    mocks/mock_voltage.c
    mocks/mock_temperature.c
    mocks/mock_soc.c
    mocks/mock_power.c
    mocks/mock_sdk_state.c
)

target_include_directories(test_telemetry PRIVATE
    ${SDK_INCLUDE}
    ${SDK_SRC}
    ${SDK_SRC}/core
    ${SDK_SRC}/telemetry
    ${unity_SOURCE_DIR}/src
)

target_link_libraries(test_telemetry PRIVATE unity)

# ── Temperature test ─────────────────────────────────────────────────────────
add_executable(test_temperature
    test_temperature.c
    ${SDK_SRC}/core_modules/battery_temperature.c
    mocks/mock_hal.c
    mocks/mock_sdk_state.c
)

target_include_directories(test_temperature PRIVATE
    ${SDK_INCLUDE}
    ${SDK_SRC}
    ${SDK_SRC}/core
    ${SDK_SRC}/hal
    ${unity_SOURCE_DIR}/src
)

target_link_libraries(test_temperature PRIVATE unity)

# ── Power manager test ───────────────────────────────────────────────────────
add_executable(test_power_manager
    test_power_manager.c
    ${SDK_SRC}/core_modules/battery_power_manager.c
    mocks/mock_voltage.c
    mocks/mock_sdk_state.c
)

target_include_directories(test_power_manager PRIVATE
    ${SDK_INCLUDE}
    ${SDK_SRC}
    ${SDK_SRC}/core
    ${SDK_SRC}/core_modules
    ${unity_SOURCE_DIR}/src
)

target_link_libraries(test_power_manager PRIVATE unity)

# ── CTest integration ────────────────────────────────────────────────────────
enable_testing()
add_test(NAME voltage_filter COMMAND test_voltage_filter)
add_test(NAME soc_lut COMMAND test_soc_lut)
add_test(NAME telemetry COMMAND test_telemetry)
add_test(NAME temperature COMMAND test_temperature)
add_test(NAME power_manager COMMAND test_power_manager)
