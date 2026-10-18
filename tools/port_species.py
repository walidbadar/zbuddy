#!/usr/bin/env python3
# Copyright (c) 2026 Muhammad Waleed Badar
# SPDX-License-Identifier: GPL-3.0-only
"""Convert the species files of claude-desktop-buddy (C++, M5StickC Plus)
into the C files of lib/zbuddy/species/.

    tools/port_species.py <path/to/claude-desktop-buddy/src/buddies> lib/zbuddy/species

The art and the animation sequences are kept verbatim; only the language
and the drawing helper names change.
"""
import re
import sys
from pathlib import Path

RENAMES = {
    "buddyPrintSprite": "zbuddy_art_sprite",
    "buddyPrintLine": "zbuddy_art_line",
    "buddySetColor": "zbuddy_art_color",
    "buddySetCursor": "zbuddy_art_cursor",
    "buddyPrint": "zbuddy_art_print",
    "BUDDY_X_CENTER": "ZBUDDY_ART_X_CENTER",
    "BUDDY_CANVAS_W": "ZBUDDY_ART_CANVAS_W",
    "BUDDY_Y_BASE": "ZBUDDY_ART_Y_BASE",
    "BUDDY_Y_OVERLAY": "ZBUDDY_ART_Y_OVERLAY",
    "BUDDY_CHAR_W": "ZBUDDY_ART_CHAR_W",
    "BUDDY_CHAR_H": "ZBUDDY_ART_CHAR_H",
    "BUDDY_BG": "ZBUDDY_ART_BG",
    "BUDDY_HEART": "ZBUDDY_ART_HEART",
    "BUDDY_DIM": "ZBUDDY_ART_DIM",
    "BUDDY_YEL": "ZBUDDY_ART_YEL",
    "BUDDY_WHITE": "ZBUDDY_ART_WHITE",
    "BUDDY_CYAN": "ZBUDDY_ART_CYAN",
    "BUDDY_GREEN": "ZBUDDY_ART_GREEN",
    "BUDDY_PURPLE": "ZBUDDY_ART_PURPLE",
    "BUDDY_RED": "ZBUDDY_ART_RED",
    "BUDDY_BLUE": "ZBUDDY_ART_BLUE",
}

HEADER = """/*
 * Copyright (c) 2026 Muhammad Waleed Badar
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * The {name} species. Art and animation sequences from claude-desktop-buddy
 * (Copyright 2026 Anthropic, PBC, MIT licence, see LICENSE.claude-desktop-buddy),
 * converted to C by tools/port_species.py.
 */
#include <zbuddy/art.h>

#include <stdint.h>
#include <string.h>

"""


def split_args(s):
    """Top-level comma split of an argument list."""
    out, depth, cur = [], 0, []
    for ch in s:
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        if ch == "," and depth == 0:
            out.append("".join(cur))
            cur = []
        else:
            cur.append(ch)
    out.append("".join(cur))
    return out


def fix_default_args(src, name, n_required):
    """Append the defaulted trailing argument (x_off = 0)."""
    pos = 0
    while True:
        i = src.find(name + "(", pos)
        if i < 0:
            return src
        j = i + len(name) + 1
        depth = 1
        k = j
        while depth > 0:
            if src[k] == "(":
                depth += 1
            elif src[k] == ")":
                depth -= 1
            k += 1
        args = src[j : k - 1]
        n = len(split_args(args))
        if n < n_required:
            src = src[: k - 1] + ", 0" + src[k - 1 :]
        pos = k
    return src


def convert(text, species):
    lines = []
    for line in text.splitlines():
        if line.startswith("#include") or line.startswith("extern TFT_eSprite"):
            continue
        if re.match(r"^\s*namespace \w+ \{\s*$", line):
            continue
        if re.match(r"^\}\s*//\s*namespace", line):
            continue
        lines.append(line)
    src = "\n".join(lines).strip() + "\n"
    src = re.sub(r"\b" + species + r"::", "", src)
    src = re.sub(
        r"extern const Species \w+ = \{",
        "const struct zbuddy_species zbuddy_species_" + species + " = {",
        src,
    )
    for old, new in RENAMES.items():
        src = re.sub(r"\b" + old + r"\b", new, src)
    src = fix_default_args(src, "zbuddy_art_sprite", 5)
    src = fix_default_args(src, "zbuddy_art_line", 4)
    src = src.replace("const char* const*", "const char *const *")
    src = src.replace("const char* const", "const char *const")
    src = src.replace("nullptr", "NULL")
    src = silence_unused(src)
    return HEADER.format(name=species) + src


def silence_unused(src):
    """Poses that are declared but not in any sequence: keep them (they
    document the art) and tell the compiler they are intentional."""
    out = []
    for line in src.splitlines():
        out.append(line)
        m = re.match(r"\s*static const char \*const (\w+)\[\d+\]\s*=", line)
        if m and len(re.findall(r"\b" + m.group(1) + r"\b", src)) == 1:
            out.append("  (void)%s;" % m.group(1))
    return "\n".join(out) + "\n"


def main():
    src_dir, dst_dir = Path(sys.argv[1]), Path(sys.argv[2])
    dst_dir.mkdir(parents=True, exist_ok=True)
    for f in sorted(src_dir.glob("*.cpp")):
        species = f.stem
        out = dst_dir / (species + ".c")
        out.write_text(convert(f.read_text(), species))
        print("wrote", out)


if __name__ == "__main__":
    main()
