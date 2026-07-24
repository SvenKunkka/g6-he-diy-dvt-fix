cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(g6_he_replacement)

target_include_directories(app PRIVATE include)

target_sources(app PRIVATE
  src/g6_config.c
  src/g6_input_guard.c
  src/g6_hw_nrf54.c
  src/g6_usb.c
  src/main.c
)
