..
   Copyright (c) 2026 Muhammad Waleed Badar
   SPDX-License-Identifier: GPL-3.0-only

Hardware
########

VIEWE UEDX32480035E-WB-A
************************

The board target is ``uedx32480035e_wb_a/esp32s3/procpu``: an ESP32-S3
with 16 MB flash and 8 MB octal PSRAM, a 3.5" 320x480 ST7796S display with
a CHSC6540 capacitive touch controller, a WS2812 RGB LED, an active
buzzer, a BOOT button and a USB serial console. The application uses:

===================  ========================================================
Part                 Use
===================  ========================================================
display and touch    the LVGL screens, upright (320x480)
BOOT button          not used: GPIO0 is also the RGB LED's data line
RGB LED              blinks orange while an approval is pending
buzzer               chirps on a prompt, a button, a passkey
Bluetooth LE         the link to Claude Desktop
flash                ``characters`` partition (4 MB LittleFS) for pushed
                     packs, ``storage`` partition for the settings and bonds
uart0                the protocol on the header pins, for ``tools/bridge_sim.py``
===================  ========================================================

The board overlay (``app/boards/uedx32480035e_wb_a_esp32s3_procpu.overlay``)
removes the second image slots of the default partition table to make room
for the character partition; the application does not use MCUboot.

Memory: LVGL's pool (512 KB) and the GIFs of a pushed pack live in PSRAM;
the Bluetooth stack, the ASCII canvas and the file system in internal RAM,
which is about 87 % used.

Pairing with Claude Desktop
***************************

The Bluetooth API of Claude Desktop is only available with developer mode
on (Help > Troubleshooting > Enable Developer Mode) and is not an
officially supported product feature.

1. Flash the board; it advertises as ``Claude-XXXX`` (the last two bytes of
   its address).
2. In Claude Desktop open Developer > Hardware Buddy and pick the device.
   On macOS the app asks for Bluetooth permission on first use.
3. The device shows a six digit passkey; type it on the desktop. The link
   is LE Secure Connections with bonding, so the next connection is
   automatic. The characteristics only work over an encrypted link.
4. The desktop sends a heartbeat on every change and a keepalive every
   10 s; after 30 s of silence the pet goes back to sleep.

``unpair`` from the desktop (or the factory reset) forgets the bonds.

Other boards
************

The application needs a display with touch (``zephyr,display`` and
``zephyr,touch`` chosen), Bluetooth LE, a flash partition labelled
``characters_partition`` and a ``storage_partition`` for the settings.
Optional: a ``led-strip`` alias (the attention blink), a ``buzzer0``
alias, a ``zbuddy-backlight`` alias (a regulator switched off with the
screen) and a ``zbuddy,bridge-uart`` chosen UART. The screens lay
themselves out from the resolution, portrait or landscape, from about
240x320 up.

native_sim with real Bluetooth
******************************

The simulator can use the Linux host's Bluetooth adapter through an HCI
user channel, which lets Claude Desktop on another machine pair with it
exactly as with a board:

.. code-block:: sh

   west build -b native_sim/native/64 -d build/native-bt zbuddy/app -- \
       -DZEPHYR_EXTRA_MODULES=$PWD/zbuddy \
       -DEXTRA_CONF_FILE=overlays/native_sim_bt.conf \\
       -DEXTRA_DTC_OVERLAY_FILE=overlays/native_sim_bt.overlay
   sudo btmgmt -i hci0 power off          # Zephyr takes the adapter over
   sudo ./build/native-bt/zephyr/zephyr.exe --bt-dev=hci0 --flash=buddy.bin

The adapter must be powered off in BlueZ first and the process needs the
rights to open a raw HCI socket (root, or ``CAP_NET_RAW`` and
``CAP_NET_ADMIN``).
