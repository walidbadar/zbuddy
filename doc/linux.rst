..
   Copyright (c) 2026 Muhammad Waleed Badar
   SPDX-License-Identifier: GPL-3.0-only

Linux: Claude Code as the desktop
#################################

Claude Desktop, and with it the Hardware Buddy window, exists for macOS
and Windows (and a Debian/Ubuntu beta without that feature). On other
Linux systems the buddy can follow `Claude Code <https://code.claude.com>`_,
the command line tool, through its hooks: ``tools/claude_code_bridge.py``
is a small daemon that keeps the link to the device and sends the
heartbeats, and a hook command that Claude Code runs on its events.

Pair the device with the host
*****************************

The daemon talks Bluetooth through BlueZ with the ``bleak`` package
(``pip install bleak``). The device must be bonded with the host once, with
the passkey it shows (the board insists on authenticated pairing, so a
plain "connect" without a passkey agent fails with ``pairing failed (4)``
on its console):

.. code-block:: sh

   tools/bridge_sim.py pair Claude-B176                        # type the passkey when asked
   tools/bridge_sim.py pair Claude-B176 --console /dev/ttyACM0 # reads it from the console

The command removes a stale bond, scans, pairs through ``bluetoothctl``
with a keyboard agent and leaves the device bonded but *not trusted*:
BlueZ reconnects to a trusted device by itself as soon as it advertises,
which would keep the tools from reaching it. The desktop's Bluetooth
settings pair the same way, but trust the device; ``bluetoothctl untrust
<addr>`` undoes that. If a bond is removed on one side only, every
connection fails until both are paired again: ``unpair`` on the device or
a factory reset clears its side, ``bluetoothctl remove <addr>`` the host's.

Run the daemon
**************

.. code-block:: sh

   tools/claude_code_bridge.py daemon -p ble:Claude-B176

It sends the time, then a heartbeat every 10 s and on every change, and
prints the decisions taken on the device. ``-p`` also accepts a serial
port or the pseudo terminal of ``native_sim``. When the link drops it
reconnects with a growing delay.

To keep it running across logins as a systemd user service:

.. code-block:: sh

   tools/claude_code_bridge.py install-service -p ble:Claude-B176
   journalctl --user -u zbuddy-bridge -f          # its log
   systemctl --user stop zbuddy-bridge            # or disable --now

The unit runs the daemon with the Python that ran the command (it needs
``bleak``), restarts it five seconds after a failure and starts with your
session.

Register the hooks
******************

.. code-block:: sh

   tools/claude_code_bridge.py settings

prints the ``hooks`` block to merge into ``~/.claude/settings.json``: the
``hook`` command on ``SessionStart``, ``SessionEnd``, ``UserPromptSubmit``,
``PreToolUse``, ``PostToolUse``, ``Stop`` and ``Notification`` (a 5 s
timeout, they only forward the event), and on ``PermissionRequest`` with a
95 s timeout, since that one waits for your tap. ``/hooks`` inside Claude
Code lists what is registered.

What the pet sees
*****************

===================  =========================================================
Claude Code event    On the device
===================  =========================================================
SessionStart/End     the session count
UserPromptSubmit     the session is running; the prompt joins the transcript
PreToolUse           running; the tool name is the status message
Stop                 not running; the reply joins the transcript, the pet
                     celebrates, tokens are estimated from the reply length
PermissionRequest    the approval panel: tool and command; Approve or Deny
                     answers Claude Code, otherwise the terminal prompt
                     takes over after the hook's timeout
Notification         ``permission_prompt`` marks the session as waiting
===================  =========================================================

Several sessions are counted by their ``session_id``; a session idle for
two minutes is no longer "running". Three or more running sessions make
the pet busy, like with the desktop app.

Caveats
*******

- The hooks carry no token usage, so the level grows from an estimate
  (a quarter of the characters of each reply).
- While the ``PermissionRequest`` hook waits, Claude Code's own prompt may
  not be shown; the hook gives up after 90 s and leaves the usual prompt in
  place. Whether Claude Code shows its prompt during the hook, or only
  after, depends on the version: check with ``claude --debug``.
- Without the daemon running, the hooks return at once and change nothing.
- The device keeps one prompt at a time; others queue in the daemon.
