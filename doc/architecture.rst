..
   Copyright (c) 2026 Muhammad Waleed Badar
   SPDX-License-Identifier: GPL-3.0-only

Architecture
############

Layout
******

::

   lib/zbuddy/             the module: protocol, stats, manifests, species
     zbuddy_json.c         JSON scanner
     zbuddy_proto.c        lines, snapshot, commands, acks, folder push
     zbuddy_stats.c        the persisted record and what is derived from it
     zbuddy_character.c    manifest.json
     zbuddy_art.c          drawing helpers, species registry
     species/*.c           nineteen species, seven animations each
   include/zbuddy/         the public headers
   app/src/
     main.c                init and the loop
     buddy.c               the pet: moods, screens, menus, inputs
     ui.c                  LVGL screens from the model
     art_canvas.c          the ASCII canvas (LVGL canvas, unscii_8, shown 2x)
     character.c           packs: GIFs in memory, LVGL's GIF widget
     builtin_pack.c        the clawd crab (generated)
     storage.c             LittleFS, the folder push receiver
     persist.c             the settings subsystem
     bridge.c              receive ring, send fan-out
     bridge_ble.c          NUS peripheral, pairing
     bridge_uart.c         the UART transport
     hw.c                  button, LED, buzzer, backlight
     clock.c               wall clock from the time sync
     host.c                native_sim options
   tests/lib/zbuddy/       ztest suite of the module
   tools/                  bridge_sim.py, mkpack.py, port_species.py, scenarios
   characters/clawd/       the sample pack

Threads
*******

Everything of the pet runs on the main thread (priority 12): the loop
drains the receive ring into the parser, runs the state machine, refreshes
the LVGL widgets and calls ``lv_timer_handler()``, then sleeps at least
5 ms so that the Bluetooth host, the input thread (touch), the log and
the shell threads get the processor. Bytes arrive from the Bluetooth RX
thread and the UART interrupt into a spin-locked ring buffer. Lines are
sent from the main thread; a notification that finds no buffer is retried
after a short sleep.

The pet
*******

``buddy.c`` is a port of the original device's loop without the drawing.
Each iteration: the protocol poll (timeouts, demo), the base mood from the
snapshot, a one-shot mood when one is active (dizzy, heart, celebrate),
the prompt arrival (chirp, home screen), the clock mode, the LED and the
idle timeout. Inputs are functions (``buddy_tap_pet()``,
``buddy_long_press()``, ``buddy_pick()``...) called by the LVGL events and
the button. ``ui.c`` reads the model and sets what changed; widgets are
created once and shown or hidden.

Drawing
*******

The ASCII species print on a 160x88 RGB565 canvas with LVGL's 8x8
``unscii`` font, doubled with nearest-neighbour scaling for the pixel look
of the original 135x240 device. GIFs are decoded by LVGL's GIF widget from
memory: a pack's files are read into the heap (PSRAM on the ESP32-S3) when
it is loaded, so the states switch without touching the flash. The
built-in pack is the same data as C arrays.

Storage
*******

Pushed packs go to ``/lfs/characters/<name>/`` on the ``characters``
partition. The settings subsystem (NVS on the ``storage`` partition) keeps
the names, the species choice, the preferences, the stats record and the
Bluetooth bonds. On ``native_sim`` both are in the flash simulator file.
