#!/usr/bin/env python3
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
"""
size_report.py - how much flash and RAM each part of the program uses,
read from the linker's map file.

EXAMPLE
    python3 tools/size_report.py build/my_app.elf.map

WHERE THE MAP FILE COMES FROM
    The Pico SDK build already writes one: build/<program>.elf.map. The
    linker lists every piece of code and data it placed, with its size and
    the file it came from. This script adds those up per source file.

WHAT THE COLUMNS MEAN
    flash   code (.text) and constant data (.rodata), plus the starting
            values of initialised variables (.data), which are copied to RAM
            at start-up
    RAM     variables: initialised (.data) and zeroed (.bss)
    Only what is actually linked is counted: the linker drops functions a
    program never calls (--gc-sections), so the figures depend on which
    library features the program uses.
"""

import re
import sys
from collections import defaultdict

# One placed input section, in either of the two layouts GNU ld uses:
#   " .text.foo      0x10001234      0x1a4 path/lib.a(file.c.obj)"
#   " .text.foo\n                0x10001234      0x1a4 path/lib.a(file.c.obj)"
ENTRY = re.compile(r"^\s+0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)\s+(\S.*)$")
NAMED = re.compile(r"^ (\.\S+|COMMON)\s+0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)\s+(\S.*)$")
NAME_ONLY = re.compile(r"^ (\.\S+|COMMON)\s*$")


def kind(section):
    """flash, ram, or both, from the input section's name."""
    if section == "COMMON" or section.startswith((".bss", ".noinit", ".uninitialized")):
        return "ram"
    if section.startswith(".data") or section.startswith(".time_critical"):
        return "both"      # stored in flash, copied to RAM at start-up
    if section.startswith((".text", ".rodata", ".ARM", ".init", ".fini", ".boot", ".binary_info",
                           ".embedded", ".flashdata", ".vectors", ".eh_frame", ".gnu", ".picobin")):
        return "flash"
    return None             # debug info and bookkeeping: not in the program


def source_of(path):
    """('group', 'file') for an object path."""
    m = re.search(r"([^/\\\\]+)\.a\(([^)]+)\)", path)
    if m:
        lib, obj = m.group(1), m.group(2)
        lib = lib[3:] if lib.startswith("lib") else lib      # libqg4p -> qg4p
        obj = re.sub(r"\.(c|cpp|S)\.obj$|\.obj$|\.o$", "", obj)
        return lib, obj
    obj = re.sub(r".*[/\\\\]", "", path)
    obj = re.sub(r"\.(c|cpp|S)\.obj$|\.obj$|\.o$", "", obj)
    if "pico-sdk" in path or ".pico-sdk" in path or "/sdk/" in path:
        return "pico-sdk", obj
    return "program", obj


def parse(path):
    sizes = defaultdict(lambda: [0, 0])          # (group, file) -> [flash, ram]
    in_map = False
    pending = None
    with open(path, errors="replace") as fh:
        for line in fh:
            if line.startswith("Linker script and memory map"):
                in_map = True
                continue
            if not in_map:
                continue                        # skips "Discarded input sections"
            m = NAMED.match(line)
            if m:
                sec, size, obj = m.group(1), int(m.group(3), 16), m.group(4)
                pending = None
            else:
                m1 = NAME_ONLY.match(line)
                if m1:
                    pending = m1.group(1)
                    continue
                m2 = ENTRY.match(line)
                if not (m2 and pending):
                    pending = None
                    continue
                sec, size, obj = pending, int(m2.group(2), 16), m2.group(3)
                pending = None
            if size == 0 or obj.startswith("*"):
                continue
            k = kind(sec)
            if k is None:
                continue
            key = source_of(obj.strip())
            if k in ("flash", "both"):
                sizes[key][0] += size
            if k in ("ram", "both"):
                sizes[key][1] += size
    return sizes


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    sizes = parse(sys.argv[1])
    if not sizes:
        sys.exit("No placed sections found. Is this a GNU ld map file?")

    groups = defaultdict(list)
    for (g, f), (fl, ram) in sizes.items():
        groups[g].append((f, fl, ram))

    order = ["qg4p", "assets", "program", "pico-sdk"]
    order += sorted(g for g in groups if g not in order)
    total_fl = total_ram = 0
    print("%-34s %9s %9s" % ("", "flash", "RAM"))
    for g in order:
        if g not in groups:
            continue
        rows = sorted(groups[g], key=lambda r: -r[1])
        gf = sum(r[1] for r in rows)
        gr = sum(r[2] for r in rows)
        total_fl += gf
        total_ram += gr
        print("%-34s %9d %9d" % (g + ":", gf, gr))
        if g in ("qg4p", "assets"):
            for f, fl, ram in rows:
                print("    %-30s %9d %9d" % (f, fl, ram))
    print("%-34s %9d %9d" % ("TOTAL", total_fl, total_ram))
    print("\n(%.1f KB flash, %.1f KB RAM; the RAM figure excludes the stack and heap)"
          % (total_fl / 1024, total_ram / 1024))


if __name__ == "__main__":
    main()
