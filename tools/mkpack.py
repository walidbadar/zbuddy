#!/usr/bin/env python3
# Copyright (c) 2026 Muhammad Waleed Badar
# SPDX-License-Identifier: GPL-3.0-only
"""Generate the "clawd" character pack: an original retro pixel-art crab
drawn with Pillow, one GIF per state, 96 pixels wide, and its manifest.
With a second argument, also emit the pack as C arrays for the firmware's
built-in character.

    tools/mkpack.py characters/clawd app/src/builtin_pack.c

The pack format is the one of claude-desktop-buddy: manifest.json with the
colours and the GIF of each state (sleep, idle, busy, attention, celebrate,
dizzy, heart); a state may list several GIFs that rotate.
"""
import json
import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw

W = H = 96
BG = (0, 0, 0)
SHELL = (218, 119, 86)      # the crab's orange
SHELL_DARK = (160, 80, 55)
INK = (16, 12, 12)
WHITE = (255, 255, 255)
PINK = (255, 90, 140)
YELLOW = (255, 220, 40)
CYAN = (60, 220, 255)
PX = 4                      # one art pixel = 4 screen pixels


def frame():
    im = Image.new("RGB", (W, H), BG)
    return im, ImageDraw.Draw(im)


def px(d, x, y, col):
    """One chunky pixel of the 24x24 art grid."""
    d.rectangle((x * PX, y * PX, x * PX + PX - 1, y * PX + PX - 1), fill=col)


def row(d, x0, x1, y, col):
    for x in range(x0, x1 + 1):
        px(d, x, y, col)


def crab(d, dx=0, dy=0, claws=0, legs=0, eyes="open", look=0, squash=0):
    """A retro crab on the 24x24 grid: a wide shell, two claws, four legs a
    side, eyes on stalks. claws: 0 down, 1 raised, 2 one up; legs: 0/1 walk
    phase; eyes: open, closed, wide, spiral, heart, happy.
    """
    sy = 10 + dy + squash
    # shell
    row(d, 7 + dx, 16 + dx, sy, SHELL)
    for y in range(1, 6):
        row(d, 5 + dx, 18 + dx, sy + y, SHELL)
    row(d, 6 + dx, 17 + dx, sy + 6, SHELL)
    row(d, 7 + dx, 16 + dx, sy + 7, SHELL_DARK)
    # mouth
    row(d, 10 + dx, 13 + dx, sy + 5, SHELL_DARK)
    # legs
    for i, lx in enumerate((4, 3, 3, 4)):
        ly = sy + 3 + i
        wobble = 1 if (legs + i) % 2 else 0
        px(d, lx + dx - wobble, ly, SHELL_DARK)
        px(d, lx + dx - wobble - 1, ly + 1, SHELL_DARK)
        px(d, 23 - lx + dx + wobble, ly, SHELL_DARK)
        px(d, 24 - lx + dx + wobble, ly + 1, SHELL_DARK)
    # claws: a pincer of two prongs on an arm
    for side in (-1, 1):
        cx = 12 + side * 9 + dx
        up = claws == 1 or (claws == 2 and side == 1)
        cy = sy - 4 if up else sy
        px(d, cx - side, cy + 3, SHELL_DARK)        # arm
        px(d, cx, cy + 2, SHELL)
        px(d, cx, cy + 1, SHELL)
        px(d, cx + side, cy + 1, SHELL)
        px(d, cx + side, cy, SHELL)                  # upper prong
        px(d, cx + side * 2, cy - 1, SHELL)
        px(d, cx + side, cy + 2, SHELL)              # lower prong
        px(d, cx + side * 2, cy + 3, SHELL)
    # eye stalks and eyes: 3x3 whites with a pupil
    for ex in (9, 14):
        x = ex + dx
        px(d, x, sy - 1, SHELL_DARK)
        ey = sy - 3
        if eyes == "closed":
            row(d, x - 1, x + 1, ey + 1, INK)
        elif eyes == "happy":
            px(d, x - 1, ey + 1, INK)
            px(d, x + 1, ey + 1, INK)
            px(d, x, ey, INK)
        elif eyes == "heart":
            px(d, x, ey + 1, PINK)
            px(d, x - 1, ey, PINK)
            px(d, x + 1, ey, PINK)
            px(d, x, ey, PINK)
        else:
            for yy in range(ey, ey + 3):
                row(d, x - 1, x + 1, yy, WHITE if eyes != "spiral" else CYAN)
            if eyes == "spiral":
                px(d, x + look, ey + 1, INK)
            elif eyes == "wide":
                px(d, x + look, ey, INK)
                px(d, x + look, ey + 1, INK)
            else:
                px(d, x + look, ey + 1, INK)


def text(d, s, x, y, col):
    d.text((x, y), s, fill=col)


def save(path, frames, duration):
    frames[0].save(path, save_all=True, append_images=frames[1:], duration=duration, loop=0,
                   optimize=False)


def sleep_state():
    out = []
    for i in range(8):
        im, d = frame()
        crab(d, dy=2, squash=(1 if i % 4 == 2 else 0), eyes="closed", claws=0)
        for k, z in enumerate("zZz"):
            p = (i + k * 3) % 8
            text(d, z, 66 + p * 2, 22 - p * 3, (160, 160, 160) if k != 1 else WHITE)
        out.append(im)
    return out, 250


def idle_state(variant):
    out = []
    for i in range(10):
        im, d = frame()
        if variant == 0:
            crab(d, eyes=("closed" if i == 7 else "open"), legs=0, claws=0)
        else:
            crab(d, look=(-1 if i < 5 else 1), legs=(i % 2), claws=(2 if i in (3, 4) else 0))
        out.append(im)
    return out, 200


def busy_state():
    out = []
    for i in range(8):
        im, d = frame()
        dx = int(round(3 * math.sin(i * math.pi / 4)))
        crab(d, dx=dx, legs=(i % 2), claws=(i % 2), look=(1 if dx > 0 else -1))
        text(d, "." * (1 + i % 3), 70, 14, WHITE)
        out.append(im)
    return out, 150


def attention_state():
    out = []
    for i in range(6):
        im, d = frame()
        up = i % 2
        crab(d, dy=(-1 if up else 0), claws=1, eyes="wide", legs=up)
        if up:
            text(d, "!", 44, 2, YELLOW)
            text(d, "!", 54, 8, YELLOW)
        out.append(im)
    return out, 200


def celebrate_state():
    out = []
    for i in range(10):
        im, d = frame()
        dy = -int(2 * abs(math.sin(i * math.pi / 5)))
        crab(d, dy=dy, claws=1, eyes=("happy" if i % 5 != 4 else "open"), legs=(i % 2))
        for k in range(8):
            phase = (i * 2 + k * 5) % 24
            x = 8 + k * 11
            y = 2 + phase * 3
            col = [YELLOW, PINK, CYAN, WHITE][k % 4]
            d.rectangle((x, y, x + 2, y + 2), fill=col)
        out.append(im)
    return out, 120


def dizzy_state():
    out = []
    for i in range(8):
        im, d = frame()
        dx = int(round(2 * math.sin(i * math.pi / 2)))
        crab(d, dx=dx, eyes="spiral", look=((i // 2) % 3) - 1, claws=2, legs=(i % 2))
        a = i * math.pi / 4
        for k, col in ((0, CYAN), (math.pi, YELLOW)):
            sx = 46 + int(28 * math.cos(a + k))
            sy = 18 + int(8 * math.sin(a + k))
            text(d, "*", sx, sy, col)
        out.append(im)
    return out, 150


def heart_state():
    out = []
    for i in range(10):
        im, d = frame()
        crab(d, dy=(-1 if i % 2 else 0), eyes="heart", claws=1)
        for k in range(4):
            phase = (i + k * 3) % 12
            x = 18 + k * 20
            y = 40 - phase * 3
            if y > 0:
                d.polygon((x, y + 4, x - 4, y, x - 2, y - 3, x, y - 1, x + 2, y - 3, x + 4, y),
                          fill=PINK)
        out.append(im)
    return out, 180


def write_builtin(out, states, manifest):
    """Emit the pack as C arrays for the firmware's built-in character."""
    path = Path(sys.argv[2]) if len(sys.argv) > 2 else None
    if path is None:
        return
    files = [(out / "manifest.json").name] + [n for gifs in states.values() for n, _ in gifs]
    lines = ["/*",
             " * Copyright (c) 2026 Muhammad Waleed Badar",
             " * SPDX-License-Identifier: GPL-3.0-only",
             " *",
             " * Generated by tools/mkpack.py: the built-in '%s' character pack." % manifest["name"],
             " */",
             "#include \"builtin_pack.h\"", ""]
    for n in files:
        data = (out / n).read_bytes()
        ident = "pack_" + n.replace(".", "_")
        lines.append("static const uint8_t %s[%d] = {" % (ident, len(data)))
        for i in range(0, len(data), 16):
            lines.append("\t" + " ".join("0x%02x," % b for b in data[i:i + 16]))
        lines.append("};")
    lines.append("")
    lines.append("const struct builtin_file builtin_pack_files[] = {")
    for n in files:
        ident = "pack_" + n.replace(".", "_")
        lines.append("\t{\"%s\", %s, sizeof(%s)}," % (n, ident, ident))
    lines.append("};")
    lines.append("")
    lines.append("const size_t builtin_pack_count = %d;" % len(files))
    lines.append("const char *const builtin_pack_name = \"%s\";" % manifest["name"])
    path.write_text("\n".join(lines) + "\n")
    print("wrote", path)


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "characters/clawd")
    out.mkdir(parents=True, exist_ok=True)
    states = {
        "sleep": [("sleep.gif", sleep_state())],
        "idle": [("idle_0.gif", idle_state(0)), ("idle_1.gif", idle_state(1))],
        "busy": [("busy.gif", busy_state())],
        "attention": [("attention.gif", attention_state())],
        "celebrate": [("celebrate.gif", celebrate_state())],
        "dizzy": [("dizzy.gif", dizzy_state())],
        "heart": [("heart.gif", heart_state())],
    }
    manifest_states = {}
    for state, gifs in states.items():
        names = []
        for name, (frames, duration) in gifs:
            save(out / name, frames, duration)
            names.append(name)
        manifest_states[state] = names if len(names) > 1 else names[0]
    manifest = {
        "name": "clawd",
        "colors": {"body": "#DA7756", "bg": "#000000", "text": "#FFFFFF",
                   "textDim": "#808080", "ink": "#100C0C"},
        "states": manifest_states,
    }
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    write_builtin(out, states, manifest)
    total = sum(p.stat().st_size for p in out.iterdir())
    print("wrote %s: %d bytes" % (out, total))


if __name__ == "__main__":
    main()
