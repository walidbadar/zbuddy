#!/usr/bin/env python3
# Copyright (c) 2026 Muhammad Waleed Badar
# SPDX-License-Identifier: GPL-3.0-only
"""Bridge Claude Code (the CLI) to a zbuddy device, for machines without
Claude Desktop. A daemon keeps the link to the device and sends the
heartbeats; Claude Code hooks forward their events to the daemon through a
Unix socket, and a PermissionRequest hook waits for the decision taken on
the device (Approve / Deny), falling back to the terminal prompt when the
device does not answer in time.

    tools/claude_code_bridge.py daemon -p ble:Claude-B176     # or /dev/ttyUSB0, /dev/pts/N
    tools/claude_code_bridge.py settings                      # the hooks to add to ~/.claude/settings.json
    tools/claude_code_bridge.py hook                          # what the hooks run (reads stdin)
    tools/claude_code_bridge.py install-service -p ble:Claude-B176   # run it as a systemd user service

State sent to the device: the sessions seen (SessionStart .. SessionEnd),
which of them are working (between a prompt or tool call and Stop), which
wait for a permission, the last user prompts and assistant replies as the
transcript, a rough token count, and the time. Tokens are estimated from
the length of the replies, since the hooks do not report usage.
"""
import argparse
import json
import os
import select
import socket
import sys
import threading
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import bridge_sim  # noqa: E402

SOCK = os.environ.get("ZBUDDY_BRIDGE_SOCK") or os.path.join(
    os.environ.get("XDG_RUNTIME_DIR", "/tmp"), "zbuddy-bridge.sock")
HEARTBEAT_S = 10
IDLE_AFTER_S = 120       # a session with no activity for this long is not "running"
PROMPT_TIMEOUT_S = 90    # the hook gives up and the terminal prompt takes over
MAX_ENTRIES = 8


class State:
    def __init__(self):
        self.lock = threading.Lock()
        self.sessions = {}      # session_id -> {"last": t, "running": bool, "waiting": bool}
        self.entries = []
        self.msg = "Claude Code"
        self.tokens = 0
        self.completed_until = 0
        self.prompts = []       # pending: {"id", "tool", "hint", "conn", "session"}
        self.decided = {}       # id -> decision
        self.changed = True

    def touch(self, sid, running=None):
        s = self.sessions.setdefault(sid, {"last": time.time(), "running": False, "waiting": False})
        s["last"] = time.time()
        if running is not None:
            s["running"] = running
        self.changed = True

    def add_entry(self, text):
        text = " ".join(text.split())
        if not text:
            return
        self.entries.append(text[:90])
        del self.entries[:-MAX_ENTRIES]
        self.changed = True

    def snapshot(self):
        now = time.time()
        for s in self.sessions.values():
            if s["running"] and now - s["last"] > IDLE_AFTER_S:
                s["running"] = False
        running = sum(1 for s in self.sessions.values() if s["running"])
        waiting = sum(1 for s in self.sessions.values() if s["waiting"])
        body = {
            "total": len(self.sessions),
            "running": running,
            "waiting": waiting,
            "completed": now < self.completed_until,
            "msg": self.msg[:30],
            "entries": list(self.entries),
            "tokens": self.tokens,
            "tokens_today": self.tokens,
        }
        if self.prompts:
            p = self.prompts[0]
            body["prompt"] = {"id": p["id"], "tool": p["tool"], "hint": p["hint"]}
        return body


def ask_hint(tool_input):
    """The first question and its option labels, within the device's 95
    character hint: "Which colour? [Red / Blue / Green]". The options are
    picked in the terminal; Approve lets the question through, Deny cancels."""
    qs = tool_input.get("questions")
    if not isinstance(qs, list) or not qs or not isinstance(qs[0], dict):
        return ""
    q = " ".join(str(qs[0].get("question", "")).split())
    labels = [str(o.get("label", "")) for o in qs[0].get("options", []) if isinstance(o, dict)]
    more = " (+%d)" % (len(qs) - 1) if len(qs) > 1 else ""
    # the question keeps up to 45 characters, the options get the rest
    room = 95 - len(more)
    if len(q) > 45 and len(q) + 3 + len(" / ".join(labels)) > room:
        q = q[:44].rstrip() + "~"
    if labels:
        opts = " / ".join(labels)
        if len(q) + 3 + len(opts) > room:
            opts = opts[:max(0, room - len(q) - 4)].rstrip() + "~"
        q += " [%s]" % opts
    return (q + more)[:95]


def short_tool(name, tool_input):
    """A hint of at most ~60 characters for the device's approval panel."""
    if not isinstance(tool_input, dict):
        return ""
    if name == "AskUserQuestion":
        return ask_hint(tool_input)
    for key in ("command", "file_path", "path", "url", "pattern", "query", "description"):
        v = tool_input.get(key)
        if isinstance(v, str) and v:
            return " ".join(v.split())[:60]
    return ", ".join("%s=%s" % (k, str(v)[:20]) for k, v in list(tool_input.items())[:2])[:60]


def handle_event(state, ev, conn):
    """Apply one hook event; returns True when the connection must stay open
    for a decision."""
    name = ev.get("hook_event_name", "")
    sid = ev.get("session_id", "?")
    with state.lock:
        if name == "SessionStart":
            state.touch(sid, running=False)
            state.msg = "session started"
        elif name == "SessionEnd":
            state.sessions.pop(sid, None)
            state.msg = "session ended"
            state.changed = True
        elif name == "UserPromptSubmit":
            state.touch(sid, running=True)
            prompt = ev.get("prompt") or ev.get("message") or ""
            if prompt:
                state.add_entry("user: " + prompt)
            state.msg = "thinking"
        elif name == "PreToolUse":
            state.touch(sid, running=True)
            state.msg = ev.get("tool_name", "tool")
        elif name == "PostToolUse":
            state.touch(sid, running=True)
        elif name == "Stop":
            state.touch(sid, running=False)
            last = ev.get("last_assistant_message") or ""
            if last:
                state.add_entry("assistant: " + last)
                state.tokens += max(1, len(last) // 4)
            state.completed_until = time.time() + 15
            state.msg = "done"
        elif name == "Notification":
            state.touch(sid)
            if ev.get("notification_type") == "permission_prompt":
                state.sessions[sid]["waiting"] = True
                state.msg = "approval needed"
        elif name == "PermissionRequest":
            state.touch(sid, running=True)
            state.sessions[sid]["waiting"] = True
            pid = "p-%d-%d" % (int(time.time() * 1000) % 100000000, len(state.prompts))
            state.prompts.append({
                "id": pid,
                "tool": ev.get("tool_name", "tool")[:20],
                "hint": short_tool(ev.get("tool_name"), ev.get("tool_input")),
                "conn": conn,
                "session": sid,
                "at": time.time(),
            })
            state.msg = "approval needed"
            state.changed = True
            return True
        else:
            state.touch(sid)
    return False


def answer_prompt(state, pid, decision):
    with state.lock:
        for i, p in enumerate(state.prompts):
            if p["id"] == pid:
                del state.prompts[i]
                break
        else:
            return
        s = state.sessions.get(p["session"])
        if s is not None:
            s["waiting"] = any(q["session"] == p["session"] for q in state.prompts)
        state.changed = True
    reply = {"decision": "allow" if decision == "once" else "deny"}
    try:
        p["conn"].sendall((json.dumps(reply) + "\n").encode())
        p["conn"].close()
    except OSError:
        pass
    print("prompt %s: %s" % (p["tool"], reply["decision"]))


def expire_prompts(state):
    """Prompts the hook gave up on (its own timeout) are dropped."""
    now = time.time()
    with state.lock:
        stale = [p for p in state.prompts if now - p["at"] > PROMPT_TIMEOUT_S + 5]
        for p in stale:
            state.prompts.remove(p)
            try:
                p["conn"].close()
            except OSError:
                pass
        if stale:
            state.changed = True


def serve_hooks(state, sock):
    while True:
        conn, _ = sock.accept()
        try:
            conn.settimeout(5)
            data = b""
            while not data.endswith(b"\n"):
                chunk = conn.recv(65536)
                if not chunk:
                    break
                data += chunk
            ev = json.loads(data.decode() or "{}")
        except (OSError, ValueError):
            conn.close()
            continue
        keep = handle_event(state, ev, conn)
        if not keep:
            conn.close()
        print("event", ev.get("hook_event_name"), ev.get("tool_name", ""))


def open_port(args):
    if args.port.startswith("ble:"):
        return bridge_sim.BlePort(args.port[4:], args.verbose)
    return bridge_sim.Port(args.port, args.verbose)


def run_link(port, state, args):
    """Heartbeats and decisions over one connection; returns when it breaks."""
    now = time.time()
    tz = -time.altzone if time.localtime(now).tm_isdst else -time.timezone
    port.send({"time": [int(now), int(tz)]})
    next_beat = 0
    while True:
        expire_prompts(state)
        with state.lock:
            due = state.changed or time.time() >= next_beat
            state.changed = False
            body = state.snapshot() if due else None
        if body is not None:
            port.send(body)
            next_beat = time.time() + HEARTBEAT_S
        for line in port.lines(0.5):
            if not line.startswith("{"):
                continue
            try:
                obj = json.loads(line)
            except ValueError:
                continue
            if obj.get("cmd") == "permission":
                answer_prompt(state, obj.get("id"), obj.get("decision"))
            elif args.verbose:
                print("<-", line)


def cmd_daemon(args):
    state = State()
    if os.path.exists(SOCK):
        os.unlink(SOCK)
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.bind(SOCK)
    sock.listen(8)
    threading.Thread(target=serve_hooks, args=(state, sock), daemon=True).start()
    print("listening for hooks on", SOCK)
    delay = 2
    try:
        while True:
            port = None
            try:
                port = open_port(args)
                delay = 2
                run_link(port, state, args)
            except KeyboardInterrupt:
                raise
            except SystemExit as e:
                print("link:", e)
            except Exception as e:  # the link dropped: bleak, serial or socket errors
                print("link lost: %s" % (str(e).splitlines() or ["?"])[0])
            finally:
                if port is not None:
                    port.close()
            with state.lock:
                state.changed = True  # resend the state once reconnected
            print("reconnecting in %d s" % delay)
            time.sleep(delay)
            delay = min(delay * 2, 30)
    finally:
        sock.close()
        if os.path.exists(SOCK):
            os.unlink(SOCK)


def cmd_hook(args):
    """Run by Claude Code: forward the event, wait for a decision when it is
    a PermissionRequest. Prints nothing when the daemon is absent or silent,
    which leaves Claude Code's own prompt in place."""
    data = sys.stdin.read()
    try:
        ev = json.loads(data)
    except ValueError:
        return
    try:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(2)
        s.connect(SOCK)
        s.sendall((json.dumps(ev) + "\n").encode())
    except OSError:
        return
    if ev.get("hook_event_name") != "PermissionRequest":
        s.close()
        return
    s.settimeout(PROMPT_TIMEOUT_S)
    try:
        reply = b""
        while not reply.endswith(b"\n"):
            chunk = s.recv(4096)
            if not chunk:
                break
            reply += chunk
        decision = json.loads(reply.decode() or "{}").get("decision")
    except (OSError, ValueError):
        decision = None
    s.close()
    if decision == "allow":
        out = {"hookSpecificOutput": {"hookEventName": "PermissionRequest",
                                      "decision": {"behavior": "allow"}}}
    elif decision == "deny":
        out = {"hookSpecificOutput": {"hookEventName": "PermissionRequest",
                                      "decision": {"behavior": "deny",
                                                   "message": "Denied on the buddy device"}}}
    else:
        return
    print(json.dumps(out))


def cmd_settings(args):
    me = str(Path(__file__).resolve())
    hook = {"type": "command", "command": "python3 %s hook" % me, "timeout": 5}
    perm = {"type": "command", "command": "python3 %s hook" % me, "timeout": PROMPT_TIMEOUT_S + 5}
    hooks = {ev: [{"matcher": "*", "hooks": [hook]}]
             for ev in ("SessionStart", "SessionEnd", "UserPromptSubmit", "PreToolUse",
                        "PostToolUse", "Stop", "Notification")}
    hooks["PermissionRequest"] = [{"matcher": "*", "hooks": [perm]}]
    print(json.dumps({"hooks": hooks}, indent=2))


UNIT = """[Unit]
Description=zbuddy: bridge Claude Code to the desk pet ({port})
After=bluetooth.target

[Service]
Type=simple
ExecStart={python} -u {script} daemon -p {port}
Restart=always
RestartSec=5
Environment=PYTHONUNBUFFERED=1

[Install]
WantedBy=default.target
"""


def cmd_install_service(args):
    """Write a systemd user unit for the daemon and start it. The unit uses
    the Python that runs this command (it needs bleak for a ble: port)."""
    import subprocess

    unit_dir = Path.home() / ".config" / "systemd" / "user"
    unit_dir.mkdir(parents=True, exist_ok=True)
    unit = unit_dir / "zbuddy-bridge.service"
    unit.write_text(UNIT.format(python=sys.executable, script=str(Path(__file__).resolve()),
                                port=args.port))
    print("wrote", unit)
    if args.no_start:
        return
    for cmd in (["systemctl", "--user", "daemon-reload"],
                ["systemctl", "--user", "enable", "--now", "zbuddy-bridge.service"]):
        subprocess.run(cmd, check=True)
    print("started; follow it with: journalctl --user -u zbuddy-bridge -f")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="command", required=True)
    d = sub.add_parser("daemon", help="keep the link to the device and send the heartbeats")
    d.add_argument("-p", "--port", required=True, help="ble:<name>, a serial port or a pty")
    d.add_argument("-v", "--verbose", action="store_true")
    d.set_defaults(fn=cmd_daemon)
    sub.add_parser("hook", help="forward a Claude Code hook event (stdin)").set_defaults(fn=cmd_hook)
    sub.add_parser("settings", help="print the hooks for ~/.claude/settings.json").set_defaults(fn=cmd_settings)
    i = sub.add_parser("install-service", help="run the daemon as a systemd user service")
    i.add_argument("-p", "--port", required=True, help="ble:<name>, a serial port or a pty")
    i.add_argument("--no-start", action="store_true", help="only write the unit file")
    i.set_defaults(fn=cmd_install_service)
    args = ap.parse_args()
    try:
        args.fn(args)
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
