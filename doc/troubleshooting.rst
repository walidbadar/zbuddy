..
   Copyright (c) 2026 Muhammad Waleed Badar
   SPDX-License-Identifier: GPL-3.0-only

Troubleshooting
###############

The desktop does not list the device
   Developer mode must be on in Claude Desktop. The device advertises
   ``Claude-XXXX``; the info screen (tap the pet twice, then tap the page to
   ``BLUETOOTH``) shows the name and the address, and ``advertising`` or
   ``connected``. A board without the Espressif blobs builds without
   Bluetooth: ``west blobs fetch hal_espressif``.

Pairing fails
   The device is DisplayOnly and insists on LE Secure Connections; type the
   six digits shown. A stale bond on either side makes the link drop right
   after connecting: forget the device on the desktop and ``unpair`` (or
   factory reset) on the buddy.

Connected, but the pet sleeps
   The desktop sends heartbeats only while the Hardware Buddy window is
   open and developer mode is on. The info ``CLAUDE`` page shows the age
   of the last message; after 30 s the pet goes to sleep.

A pack push fails with ``need ...K, have ...K``
   The pack does not fit the 4 MB partition next to LittleFS's metadata.
   ``gifsicle --lossy=80 -O3 --colors 64`` typically halves a pack.

``char_begin`` times out on the desktop
   The storage did not mount (the log says why); the device then stays
   silent on purpose.

Logs appear twice on native_sim
   Both the UART and the native log backends were on; the board config
   turns the UART one off.

The simulator cannot open the display
   ``native_sim`` needs SDL2 and a display server; ``--shot`` still needs
   a window.

Where is the flash of the simulator?
   ``--flash=<file>`` (default ``flash.bin`` in the current directory);
   delete it to start fresh.
