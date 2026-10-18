..
   Copyright (c) 2026 Muhammad Waleed Badar
   SPDX-License-Identifier: GPL-3.0-only

zbuddy
######

.. image:: images/zbuddy-banner.svg
   :alt: zbuddy: a desk pet for Claude Desktop on Zephyr RTOS
   :width: 720
   :align: center

A desk pet for `Claude Desktop <https://claude.com/download>`_ running on
`Zephyr RTOS <https://zephyrproject.org>`_: a port of Anthropic's
`claude-desktop-buddy <https://github.com/anthropics/claude-desktop-buddy>`_
with the Hardware Buddy protocol packaged as a Zephyr module. The pet
sleeps when nothing is happening, wakes when sessions start, gets visibly
impatient when an approval is pending, and lets you approve or deny from
the device.

.. figure:: images/zbuddy-ui.png
   :alt: The buddy on native_sim: the clawd crab during a session, an
         approval prompt, the pet statistics, the clock face, an ASCII
         species and the pairing screen
   :width: 840
   :align: center

   On ``native_sim`` at the 320x480 of the target display: the clawd crab
   during a session, an approval prompt, the pet statistics, the clock
   face, an ASCII species and the pairing screen.

Features
********

- Speaks the Claude Desktop Hardware Buddy protocol over Bluetooth LE
  (Nordic UART Service, LE Secure Connections with a passkey on the
  display): heartbeat snapshots, transcript lines, permission prompts, time
  sync, name and owner, status, and the character folder push.
- Shows a pet with seven moods: the built-in clawd crab, any character
  pack pushed from the desktop (GIFs or text frames), or one of nineteen
  animated ASCII species.
- Approve or deny a pending tool call on the touch screen; the
  RGB LED blinks and the buzzer chirps while a prompt waits.
- Keeps score: approvals, denials, response speed (mood), tokens (level),
  naps (energy), persisted in the settings subsystem.
- Runs on the VIEWE UEDX32480035E-WB-A (ESP32-S3, 3.5" touch display) and
  on ``native_sim``, where a desktop simulator script stands in for Claude.
- On Linux, where there is no Claude Desktop, a bridge follows Claude Code
  (the CLI) through its hooks and answers its permission prompts from the
  device.
  The layout adapts to the display: portrait or landscape, from 240x320 up.
- The protocol, the stats and the species are a portable C library
  (``lib/zbuddy``) with a Zephyr module wrapper and unit tests.

.. toctree::
   :maxdepth: 2
   :caption: Contents

   getting_started
   buddy
   hardware
   linux
   protocol
   library
   architecture
   troubleshooting
   licence
