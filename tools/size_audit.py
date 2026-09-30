#!/usr/bin/env python3
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
"""
size_audit.py - flash and RAM for EVERY program in a build, and which
programs carry which library files. For keeping QG4P's promise: if a
program doesn't use a feature, it shouldn't pay for it.

EXAMPLES
    python3 tools/size_audit.py build
    python3 tools/size_audit.py build --out sizes.json
    python3 tools/size_audit.py build --baseline old_sizes.json

WHAT IT DOES
    Finds every linker map file under the build folder (the Pico SDK writes
    one per program: <program>.elf.map), reads each exactly as
    tools/size_report.py does, and prints:

      1. every program's flash and RAM, in total and for QG4P alone
      2. every QG4P library file, and the programs that include it

    The second list is the one for spotting problems. A library file shows up
    in a program only if something in that program needs it, so a file where
    you don't expect it is a clue. For example, qg_backend_buf8.c (the
    framebuffer) in a program that never uses a framebuffer means some code
    is naming QG_BACKEND_BUF8 without needing to.

    --out writes everything as JSON (default: size_audit.json in the build
    folder). --baseline compares against an earlier JSON file and shows what
    grew, what shrank, and which library files came or went.
"""

import argparse
import datetime
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from size_report import parse          # the same parser as size_report.py

LIBRARY_GROUPS = ("qg4p", "qg4p_assets")


def find_maps(build_dir):
    """Every .map file under build_dir, as (program name, path)."""
    found = []
    for root, _dirs, files in os.walk(build_dir):
        for f in files:
            if f.endswith(".map"):
                name = f[:-len(".elf.map")] if f.endswith(".elf.map") else f[:-len(".map")]
                found.append((name, os.path.join(root, f)))
    return sorted(found)


def audit(build_dir):
    programs = {}
    for name, path in find_maps(build_dir):
        sizes = parse(path)
        groups = {}
        total_fl = total_ram = 0
        for (group, obj), (fl, ram) in sizes.items():
            g = groups.setdefault(group, {"flash": 0, "ram": 0, "files": {}})
            g["flash"] += fl
            g["ram"] += ram
            g["files"][obj] = {"flash": fl, "ram": ram}
            total_fl += fl
            total_ram += ram
        programs[name] = {
            "map": os.path.relpath(path, build_dir),
            "total": {"flash": total_fl, "ram": total_ram},
            "groups": groups,
        }

    # The reverse view: each library file, and who carries it.
    library = {}
    for name, prog in programs.items():
        for group in LIBRARY_GROUPS:
            for obj, sz in prog["groups"].get(group, {}).get("files", {}).items():
                entry = library.setdefault(obj, {"group": group, "flash": sz["flash"],
                                                 "ram": sz["ram"], "programs": []})
                entry["programs"].append(name)
                # A file's size can vary slightly per program (the linker
                # keeps only the functions each program uses): keep the max.
                entry["flash"] = max(entry["flash"], sz["flash"])
                entry["ram"] = max(entry["ram"], sz["ram"])
    for entry in library.values():
        entry["programs"].sort()

    return {
        "generated": datetime.datetime.now().isoformat(timespec="seconds"),
        "build_dir": os.path.abspath(build_dir),
        "programs": programs,
        "library_files": dict(sorted(library.items())),
    }


def lib_totals(prog):
    fl = sum(prog["groups"].get(g, {}).get("flash", 0) for g in LIBRARY_GROUPS)
    ram = sum(prog["groups"].get(g, {}).get("ram", 0) for g in LIBRARY_GROUPS)
    return fl, ram


def short(name):
    return name[5:] if name.startswith("qg4p_") else name


def print_report(result):
    programs, library = result["programs"], result["library_files"]
    names = sorted(programs)

    print("PROGRAMS                              total              QG4P only")
    print("%-28s %9s %9s  %9s %9s" % ("", "flash", "RAM", "flash", "RAM"))
    for n in names:
        p = programs[n]
        lf, lr = lib_totals(p)
        print("%-28s %9d %9d  %9d %9d" % (n, p["total"]["flash"], p["total"]["ram"], lf, lr))

    print("\nLIBRARY FILES, and the programs that include them")
    print("(a file where you don't expect it is worth a look)\n")
    for obj, e in library.items():
        users = e["programs"]
        who = "ALL %d programs" % len(names) if len(users) == len(names) else ", ".join(short(u) for u in users)
        print("  %-26s %6d flash %6d RAM   %2d/%d  %s" % (obj, e["flash"], e["ram"], len(users), len(names), who))


def print_comparison(result, baseline):
    old, new = baseline["programs"], result["programs"]
    print("\nCOMPARED WITH THE BASELINE (%s)" % baseline.get("generated", "?"))
    print("%-28s %10s %10s" % ("", "flash", "RAM"))
    for n in sorted(set(old) | set(new)):
        if n not in old:
            print("%-28s  (new program)" % n)
            continue
        if n not in new:
            print("%-28s  (gone)" % n)
            continue
        dfl = new[n]["total"]["flash"] - old[n]["total"]["flash"]
        dram = new[n]["total"]["ram"] - old[n]["total"]["ram"]
        files_old = {o for g in LIBRARY_GROUPS for o in old[n]["groups"].get(g, {}).get("files", {})}
        files_new = {o for g in LIBRARY_GROUPS for o in new[n]["groups"].get(g, {}).get("files", {})}
        notes = ["+" + f for f in sorted(files_new - files_old)] + ["-" + f for f in sorted(files_old - files_new)]
        if dfl or dram or notes:
            print("%-28s %+10d %+10d  %s" % (n, dfl, dram, " ".join(notes)))


def main():
    ap = argparse.ArgumentParser(description="Flash and RAM for every program in a build.")
    ap.add_argument("build_dir", help="the build folder to search for .map files")
    ap.add_argument("--out", help="JSON file to write (default: <build_dir>/size_audit.json)")
    ap.add_argument("--baseline", help="an earlier JSON file to compare against")
    args = ap.parse_args()

    if not os.path.isdir(args.build_dir):
        sys.exit("No such folder: %s" % args.build_dir)
    result = audit(args.build_dir)
    if not result["programs"]:
        sys.exit("No .map files found under %s. Has the project been built?" % args.build_dir)

    print_report(result)
    if args.baseline:
        with open(args.baseline) as fh:
            print_comparison(result, json.load(fh))

    out = args.out or os.path.join(args.build_dir, "size_audit.json")
    with open(out, "w") as fh:
        json.dump(result, fh, indent=2)
    print("\nWrote %s (%d programs)" % (out, len(result["programs"])))


if __name__ == "__main__":
    main()
