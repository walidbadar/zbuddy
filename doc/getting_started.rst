..
   Copyright (c) 2026 Muhammad Waleed Badar
   SPDX-License-Identifier: GPL-3.0-only

Getting started
###############

Requirements
************

- Zephyr 4.5 (the manifest pins ``v4.5.0-rc2``) and the Zephyr SDK.
- The ``lvgl``, ``littlefs``, ``mbedtls`` and ``hal_espressif`` modules,
  which the manifest pulls in; for the ESP32-S3 also the Espressif binary
  blobs: ``west blobs fetch hal_espressif``.
- For ``native_sim``, the SDL2 development package.
- Python 3 for the tools; Pillow only to regenerate the crab pack.

Workspace
*********

The repository is a west manifest repository and a Zephyr module, laid out
like Zephyr's `example-application
<https://github.com/zephyrproject-rtos/example-application>`_. Either
create a workspace from it:

.. code-block:: sh

   west init -m https://github.com/walidbadar/zbuddy --mr main zbuddy-workspace
   cd zbuddy-workspace && west update

or build it inside an existing workspace as an extra module:

.. code-block:: sh

   west build -b native_sim/native/64 -d build/native zbuddy/app -- \
       -DZEPHYR_EXTRA_MODULES=$PWD/zbuddy

native_sim
**********

.. code-block:: sh

   west build -b native_sim/native/64 -d zbuddy/build/native zbuddy/app -- \
       -DZEPHYR_EXTRA_MODULES=$PWD/zbuddy
   ./zbuddy/build/native/zephyr/zephyr.exe --flash=buddy.bin

An SDL window shows the 320x480 display; the mouse is the touch screen.
``--flash=<file>`` is the simulated flash that keeps the settings and a
pushed character pack between runs (``--flash_rm`` deletes it at exit).
At boot the simulator prints the pseudo terminal of the bridge::

   uart_1 connected to pseudotty: /dev/pts/5

That port is where Claude Desktop would be. ``tools/bridge_sim.py`` plays
its part (``-p ble:Claude-XXXX`` talks to a real board over Bluetooth from
a Linux host, see :doc:`linux`):

.. code-block:: sh

   tools/bridge_sim.py -p /dev/pts/5 scenario busy       # heartbeats every 10 s
   tools/bridge_sim.py -p /dev/pts/5 prompt Bash "west flash"
   tools/bridge_sim.py -p /dev/pts/5 push characters/clawd
   tools/bridge_sim.py -p /dev/pts/5 owner Walid
   tools/bridge_sim.py -p /dev/pts/5 status

The simulator build has more options for screenshots and scripted
sessions; see :doc:`buddy`.

ESP32-S3
********

The supported board is the VIEWE UEDX32480035E-WB-A (ESP32-S3, 8 MB PSRAM,
3.5" 320x480 touch display, RGB LED, buzzer).

.. code-block:: sh

   west blobs fetch hal_espressif
   west build -b uedx32480035e_wb_a/esp32s3/procpu -d zbuddy/build/esp32s3 zbuddy/app -- \
       -DZEPHYR_EXTRA_MODULES=$PWD/zbuddy
   west flash -d zbuddy/build/esp32s3

The board boots with the clawd crab and advertises as ``Claude-XXXX``.
See :doc:`hardware` for pairing it with the desktop app.

Tests
*****

.. code-block:: sh

   west twister -p native_sim/native/64 -T zbuddy/tests
   west twister -p native_sim/native/64 -T zbuddy/app      # build check

The library tests also run on ``qemu_cortex_m3``.
