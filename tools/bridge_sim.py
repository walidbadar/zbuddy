#!/usr/bin/env python3
# Copyright (c) 2026 Muhammad Waleed Badar
# SPDX-License-Identifier: GPL-3.0-only
"""Stand in for Claude Desktop: speak the hardware buddy protocol to a
device over a serial port or the native_sim pseudo terminal.

    tools/bridge_sim.py -p /dev/pts/5 monitor              # print what the device sends
    tools/bridge_sim.py -p /dev/pts/5 scenario busy        # heartbeats of a scenario
    tools/bridge_sim.py -p /dev/pts/5 prompt Bash "rm -rf build"
    tools/bridge_sim.py -p /dev/pts/5 turn assistant "All tests pass."
    tools/bridge_sim.py -p /dev/pts/5 push characters/pixel # install a character pack
    tools/bridge_sim.py -p /dev/pts/5 status | name Pixel | owner Walid | time | unpair
    tools/bridge_sim.py -p /dev/pts/5 species 4
    tools/bridge_sim.py pair Claude-B176 [--console /dev/ttyACM0]   # bond a board with this host

The port is the pseudo terminal native_sim prints at boot ("uart_1
connected to pseudotty: /dev/pts/N"), the UART of a board, or
"ble:<name or address>" for the Bluetooth link of a board (needs the
bleak package; the device must be paired with the host first, e.g. in the
desktop's Bluetooth settings or with bluetoothctl). Everything else is the
standard library.
"""
import argparse
import base64
import json
import os
import select
import sys
import termios
import time
import tty
from pathlib import Path

SCENARIOS = {
    "asleep": dict(total=0, running=0, waiting=0),
    "idle": dict(total=1, running=0, waiting=0, tokens=12000),
    "busy": dict(total=4, running=3, waiting=0, tokens=89000),
    "attention": dict(total=2, running=1, waiting=1, tokens=45000),
    "completed": dict(total=1, running=0, waiting=0, completed=True, tokens=142000),
}


NUS_RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  # desktop writes
NUS_TX = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  # device notifies


class AckMixin:
    def wait_ack(self, what, timeout=5):
        for line in self.lines(timeout):
            if self.verbose:
                print("<-", line)
            if line.startswith("{"):
                try:
                    obj = json.loads(line)
                except ValueError:
                    continue
                if obj.get("ack") == what:
                    return obj
        raise TimeoutError("no ack for %s" % what)


class BlePort(AckMixin):
    """The same interface as Port over the Nordic UART Service."""

    def __init__(self, target, verbose):
        import asyncio
        from bleak import BleakClient, BleakScanner

        self.asyncio = asyncio
        self.loop = asyncio.new_event_loop()
        self.buf = b""
        self.verbose = verbose
        self.pending = b""

        def known_address(name):
            """A device the host already knows (paired ones stop advertising
            once the host auto-connects to them)."""
            import subprocess
            try:
                out = subprocess.run(["bluetoothctl", "devices"], capture_output=True, text=True,
                                     timeout=5).stdout
            except Exception:
                return None
            for line in out.splitlines():
                parts = line.split(" ", 2)
                if len(parts) == 3 and parts[2] == name:
                    return parts[1]
            return None

        async def find(addr, name):
            if addr is not None:
                return await BleakScanner.find_device_by_address(addr, timeout=8)
            return await BleakScanner.find_device_by_name(name, timeout=8)

        async def connect():
            import subprocess
            addr = target if (":" in target and len(target) == 17) else known_address(target)
            dev = await find(addr, target)
            if dev is None and addr is not None:
                # BlueZ keeps a trusted device connected itself, and a
                # connected peripheral does not advertise: let it go first
                subprocess.run(["bluetoothctl", "disconnect", addr], capture_output=True,
                               timeout=10)
                await asyncio.sleep(1.5)
                dev = await find(addr, target)
            if dev is None:
                raise SystemExit("device %s not found (is it advertising?)" % target)
            client = BleakClient(dev)
            await client.connect()
            try:
                await client.pair()
            except Exception as e:  # already bonded, or not supported by the backend
                if verbose:
                    print("pair:", e)
            await client.start_notify(NUS_TX, lambda _h, data: self._on_notify(bytes(data)))
            return client

        print("connecting to", target)
        self.client = self.loop.run_until_complete(connect())
        print("connected, mtu", self.client.mtu_size)

    def _on_notify(self, data):
        self.pending += data

    def send(self, obj):
        line = json.dumps(obj, separators=(",", ":")) + "\n"
        if self.verbose:
            print("->", line.strip())
        data = line.encode()
        chunk = max(20, self.client.mtu_size - 3)

        async def write():
            for i in range(0, len(data), chunk):
                await self.client.write_gatt_char(NUS_RX, data[i:i + chunk], response=False)

        self.loop.run_until_complete(write())

    def close(self):
        try:
            self.loop.run_until_complete(self.client.disconnect())
        except Exception:
            pass

    def lines(self, timeout):
        end = time.monotonic() + timeout
        while True:
            left = end - time.monotonic()
            if left <= 0:
                return
            if not self.client.is_connected:
                raise ConnectionError("disconnected")
            self.loop.run_until_complete(self.asyncio.sleep(min(left, 0.1)))
            if self.pending:
                self.buf += self.pending
                self.pending = b""
            while b"\n" in self.buf:
                line, self.buf = self.buf.split(b"\n", 1)
                yield line.decode(errors="replace").rstrip("\r")

class Port(AckMixin):
    def __init__(self, path, verbose):
        self.fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        try:
            tty.setraw(self.fd)
            attrs = termios.tcgetattr(self.fd)
            attrs[4] = attrs[5] = termios.B115200
            termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
        except termios.error:
            pass
        self.buf = b""
        self.verbose = verbose

    def close(self):
        os.close(self.fd)

    def send(self, obj):
        line = json.dumps(obj, separators=(",", ":")) + "\n"
        if self.verbose:
            print("->", line.strip())
        data = line.encode()
        while data:
            _, w, _ = select.select([], [self.fd], [], 5)
            if not w:
                raise TimeoutError("device not reading")
            n = os.write(self.fd, data)
            data = data[n:]

    def lines(self, timeout):
        """Yield the complete lines received within timeout seconds."""
        end = time.monotonic() + timeout
        while True:
            left = end - time.monotonic()
            if left <= 0:
                return
            r, _, _ = select.select([self.fd], [], [], left)
            if not r:
                return
            try:
                chunk = os.read(self.fd, 4096)
            except BlockingIOError:
                continue
            if not chunk:
                return
            self.buf += chunk
            while b"\n" in self.buf:
                line, self.buf = self.buf.split(b"\n", 1)
                yield line.decode(errors="replace").rstrip("\r")


def cmd_monitor(port, args):
    print("monitoring, Ctrl-C to stop")
    while True:
        for line in port.lines(1):
            print(line)


def cmd_scenario(port, args):
    body = dict(SCENARIOS[args.name])
    body["msg"] = args.msg or "demo: " + args.name
    body["entries"] = args.entry or []
    if args.prompt:
        body["prompt"] = {"id": "p-%d" % int(time.time()), "tool": args.prompt[0],
                          "hint": args.prompt[1] if len(args.prompt) > 1 else ""}
    print("sending a heartbeat every 10 s, Ctrl-C to stop")
    while True:
        port.send(body)
        for line in port.lines(10):
            print("<-", line)
        if args.once:
            return


def cmd_prompt(port, args):
    pid = "p-%d" % int(time.time())
    body = dict(SCENARIOS["attention"], msg="approval pending",
                prompt={"id": pid, "tool": args.tool, "hint": args.hint})
    port.send(body)
    print("waiting for the decision on", pid)
    while True:
        for line in port.lines(10):
            print("<-", line)
            if line.startswith("{"):
                obj = json.loads(line)
                if obj.get("cmd") == "permission" and obj.get("id") == pid:
                    print("decision:", obj.get("decision"))
                    port.send(dict(SCENARIOS["idle"], msg="resumed"))
                    return
        port.send(body)


def cmd_turn(port, args):
    port.send({"evt": "turn", "role": args.role,
               "content": [{"type": "text", "text": args.text}]})


def cmd_simple(port, args):
    body = {"cmd": args.command}
    if args.command in ("name", "owner"):
        body["name"] = args.value
    if args.command == "species":
        body["idx"] = int(args.value)
    port.send(body)
    print(port.wait_ack(args.command))


def cmd_time(port, args):
    now = time.time()
    tz = -time.timezone if not time.localtime(now).tm_isdst else -time.altzone
    port.send({"time": [int(now), int(tz)]})


def cmd_push(port, args):
    folder = Path(args.folder)
    files = sorted(p for p in folder.iterdir() if p.is_file() and not p.name.startswith("."))
    total = sum(p.stat().st_size for p in files)
    name = folder.name
    manifest = folder / "manifest.json"
    if manifest.exists():
        name = json.loads(manifest.read_text()).get("name", name)
    print("pushing '%s': %d files, %d bytes" % (name, len(files), total))
    port.send({"cmd": "char_begin", "name": name, "total": total})
    ack = port.wait_ack("char_begin")
    if not ack.get("ok"):
        sys.exit("refused: %s" % ack.get("error"))
    done = 0
    for p in files:
        data = p.read_bytes()
        port.send({"cmd": "file", "path": p.name, "size": len(data)})
        port.wait_ack("file")
        for i in range(0, len(data), args.chunk):
            port.send({"cmd": "chunk", "d": base64.b64encode(data[i:i + args.chunk]).decode()})
            port.wait_ack("chunk", timeout=15)
            done += len(data[i:i + args.chunk])
            print("\r  %s %d/%d" % (p.name, done, total), end="", flush=True)
        port.send({"cmd": "file_end"})
        port.wait_ack("file_end")
        print()
    port.send({"cmd": "char_end"})
    print(port.wait_ack("char_end", timeout=15))


def cmd_pair(args):
    """Pair a board with this Linux host through bluetoothctl: the device
    shows a six digit passkey, typed here (or read from its console log
    when --console names its serial port). The bond is kept, the device is
    left untrusted so that BlueZ does not grab it by itself."""
    import re
    import subprocess
    import threading

    passkey = {"value": None}
    if args.console:
        def console():
            try:
                fd = os.open(args.console, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
            except OSError as e:
                print("console:", e)
                return
            buf = b""
            end = time.monotonic() + 90
            while time.monotonic() < end and passkey["value"] is None:
                r, _, _ = select.select([fd], [], [], 0.3)
                if r:
                    try:
                        buf += os.read(fd, 4096)
                    except OSError:
                        break
                    m = re.search(rb"passkey (\d{6})", buf)
                    if m:
                        passkey["value"] = m.group(1).decode()
                    buf = buf[-4096:]
            os.close(fd)
        threading.Thread(target=console, daemon=True).start()

    addr = args.target if (":" in args.target and len(args.target) == 17) else None
    proc = subprocess.Popen(["bluetoothctl"], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, bufsize=1)
    out = []

    def reader():
        for line in proc.stdout:
            out.append(re.sub(r"\x1b\[[0-9;]*[a-zA-Z]", "", line).rstrip())

    threading.Thread(target=reader, daemon=True).start()

    def send(cmd):
        proc.stdin.write(cmd + "\n")
        proc.stdin.flush()

    time.sleep(0.5)
    send("agent KeyboardOnly")
    send("default-agent")
    if addr is not None:
        send("remove " + addr)
    send("scan le")
    print("scanning for", args.target)
    t0 = time.monotonic()
    while time.monotonic() - t0 < 25:
        for line in out:
            m = re.search(r"Device ([0-9A-F:]{17}) (.*)", line)
            if m and (m.group(1) == addr or m.group(2).strip() == args.target):
                addr = m.group(1)
                break
        if addr is not None and any(addr in l for l in out):
            break
        time.sleep(0.5)
    send("scan off")
    if addr is None or not any(addr in l for l in out):
        send("quit")
        sys.exit("device not found; is it advertising? (reset it if it was connected before)")
    time.sleep(0.5)
    print("pairing with", addr)
    send("pair " + addr)
    t0 = time.monotonic()
    asked = False
    result = "timeout"
    while time.monotonic() - t0 < 60:
        joined = "\n".join(out[-30:])
        if not asked and "passkey" in joined.lower() and ("Enter" in joined or "Request" in joined):
            key = passkey["value"]
            if key is None and args.console:
                for _ in range(100):
                    key = passkey["value"]
                    if key:
                        break
                    time.sleep(0.1)
            if key is None:
                key = input("passkey shown on the device: ").strip()
            send(key)
            asked = True
        if "Pairing successful" in joined:
            result = "paired"
            break
        if "Failed to pair" in joined:
            result = "failed"
            break
        time.sleep(0.3)
    if result == "paired":
        # bonded but not trusted: a trusted device is reconnected by BlueZ
        # itself as soon as it advertises, which keeps the tools out
        send("untrust " + addr)
        time.sleep(0.5)
    send("quit")
    print(result)
    if result != "paired":
        print("\n".join(l for l in out[-8:] if l.strip() and "RSSI" not in l))
        sys.exit(1)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-p", "--port", help="serial port, pseudo terminal or ble:<name>")
    ap.add_argument("-v", "--verbose", action="store_true")
    sub = ap.add_subparsers(dest="command", required=True)
    sub.add_parser("monitor").set_defaults(fn=cmd_monitor)
    s = sub.add_parser("scenario")
    s.add_argument("name", choices=sorted(SCENARIOS))
    s.add_argument("--msg")
    s.add_argument("--entry", action="append", help="transcript line (repeatable)")
    s.add_argument("--prompt", nargs="+", metavar=("TOOL", "HINT"))
    s.add_argument("--once", action="store_true")
    s.set_defaults(fn=cmd_scenario)
    s = sub.add_parser("prompt")
    s.add_argument("tool")
    s.add_argument("hint", nargs="?", default="")
    s.set_defaults(fn=cmd_prompt)
    s = sub.add_parser("turn")
    s.add_argument("role")
    s.add_argument("text")
    s.set_defaults(fn=cmd_turn)
    for c in ("status", "unpair"):
        sub.add_parser(c).set_defaults(fn=cmd_simple)
    for c in ("name", "owner", "species"):
        s = sub.add_parser(c)
        s.add_argument("value")
        s.set_defaults(fn=cmd_simple)
    sub.add_parser("time").set_defaults(fn=cmd_time)
    s = sub.add_parser("push")
    s.add_argument("folder")
    s.add_argument("--chunk", type=int, default=300, help="bytes per chunk (default 300)")
    s.set_defaults(fn=cmd_push)
    s = sub.add_parser("pair", help="bond a board with this Linux host (BlueZ)")
    s.add_argument("target", help="advertised name (Claude-XXXX) or address")
    s.add_argument("--console", help="the board's serial console, to read the passkey from")
    s.set_defaults(fn=cmd_pair)
    args = ap.parse_args()
    if args.command == "pair":
        cmd_pair(args)
        return
    if args.port is None:
        ap.error("-p/--port is required")
    if args.port.startswith("ble:"):
        port = BlePort(args.port[4:], args.verbose)
    else:
        port = Port(args.port, args.verbose)
    try:
        args.fn(port, args)
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
