# Copyright (c) 2026 Muhammad Waleed Badar
# SPDX-License-Identifier: GPL-3.0-only

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zbuddy_app LANGUAGES C)

target_sources(app PRIVATE
  src/main.c
  src/buddy.c
  src/clock.c
  src/ui.c
  src/art_canvas.c
  src/character.c
  src/storage.c
  src/persist.c
  src/bridge.c
  src/hw.c
)

# The built-in character (tools/mkpack.py), shown until a pack is pushed
target_sources_ifdef(CONFIG_ZBUDDY_APP_BUILTIN_CHARACTER app PRIVATE src/builtin_pack.c)

# Transports: BLE (the desktop app) and a UART (tools/bridge_sim.py, tests)
target_sources_ifdef(CONFIG_BT app PRIVATE src/bridge_ble.c)
target_sources_ifdef(CONFIG_ZBUDDY_APP_UART_BRIDGE app PRIVATE src/bridge_uart.c)
# native_sim: command line options (screenshots, scripted bridge traffic)
target_sources_ifdef(CONFIG_BOARD_NATIVE_SIM app PRIVATE src/host.c)
