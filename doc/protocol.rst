..
   Copyright (c) 2026 Muhammad Waleed Badar
   SPDX-License-Identifier: GPL-3.0-only

The protocol
############

The wire protocol is the one documented in ``REFERENCE.md`` of
claude-desktop-buddy. This page summarises what the device implements.

Transport
*********

The Nordic UART Service: service ``6e400001-b5a3-f393-e0a9-e50e24dcca9e``,
RX characteristic ``...0002`` (the desktop writes), TX characteristic
``...0003`` (the device notifies). Both require an encrypted link; the
device is DisplayOnly, so pairing is passkey entry with LE Secure
Connections and bonding. Messages are UTF-8 JSON objects, one per line,
terminated by ``\n``. Notifications fragment at the MTU, so the device
reassembles lines (up to ``CONFIG_ZBUDDY_LINE_MAX``, 4.5 KB).

The same lines work on a UART (``zbuddy,bridge-uart``), which is how
``tools/bridge_sim.py`` and the ``native_sim`` build talk.

Desktop to device
*****************

======================  =====================================================
Message                 Fields
======================  =====================================================
heartbeat snapshot      ``total``, ``running``, ``waiting``, ``completed``,
                        ``msg``, ``entries`` (lines), ``tokens``
                        (cumulative), ``tokens_today``, optional ``prompt``
                        with ``id``, ``tool``, ``hint``
turn event              ``evt: "turn"``, ``role``, ``content`` (the text
                        block becomes a transcript line)
time sync               ``time: [epoch_seconds, tz_offset_seconds]``
``cmd: "status"``       answered with the status ack below
``cmd: "name"``         ``name``: the pet's name
``cmd: "owner"``        ``name``: the owner's name
``cmd: "species"``      ``idx``: an ASCII species, ``255`` the installed pack
``cmd: "unpair"``       forget the bonds
``cmd: "char_begin"``   ``name``, ``total`` (bytes): start a pack
``cmd: "file"``         ``path``, ``size``: start a file of the pack
``cmd: "chunk"``        ``d``: base64 data
``cmd: "file_end"``     close the file
``cmd: "char_end"``     all files sent: load the pack
======================  =====================================================

Device to desktop
*****************

Every command gets ``{"ack": "<cmd>", "ok": true|false, "n": <counter>}``,
with ``"error"`` when refused; ``n`` is the bytes written so far for
``chunk`` and ``file_end``. The status ack carries
``data: {name, owner, sec, sys: {up, heap, fsFree, fsTotal}, stats: {appr,
deny, vel, nap, lvl}}`` (and ``bat`` on a board with a battery gauge).
A decision is ``{"cmd": "permission", "id": "<prompt id>", "decision":
"once"|"deny"}``.

Folder push
***********

The transfer is strictly sequential: the desktop waits for each ack. The
device refuses a pack larger than ``CONFIG_ZBUDDY_CHARACTER_MAX_BYTES``
(1.8 MB) or than the free space plus what wiping the installed pack
reclaims (``need 600K, have 400K``), and file paths with directories or
``..``. Only one pack is kept; ``char_begin`` wipes the previous one.
Without a storage the device does not ack ``char_begin`` and the desktop
times out.
