cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(fw_v0)

target_sources(app PRIVATE app/src/main.c src/core_modules/battery_adc.c)
