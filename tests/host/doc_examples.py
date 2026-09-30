#!/usr/bin/env python3
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
"""
doc_examples.py - build, run and picture every example in the manual.

    python3 tests/host/doc_examples.py            # check all, write pictures
    python3 tests/host/doc_examples.py --check    # check only, no pictures

Every example in docs/manual is a fenced code block whose info string
starts with "c example=NAME":

    ```c example=line_basic
    qg_line(&scr, 0, 0, 239, 319, QG_YELLOW);
    ```

Options after the name:
    buf8          run on a framebuffer screen (for qg_paint, qg_get...)
    pack          with the examples' asset pack loaded (qg_asset_init works)
    compile-only  compile it, but don't run it (screen setup and the like,
                  which need real hardware)

Each example is pasted into doc_example_template.c, compiled against the
real library, and run on a stand-in 240x320 screen. The picture goes to
docs/manual/img/NAME.png. Any example that fails to compile, fails to run,
or draws off the screen fails the check: the manual can't quietly rot.
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
MANUAL = os.path.join(ROOT, "docs", "manual")
IMG = os.path.join(MANUAL, "img")
BUILD = os.path.join(HERE, "build", "doc")
LIB = os.path.join(ROOT, "qg4p")
FENCE = re.compile(r"^```c example=(\S+)([^\n]*)\n(.*?)^```", re.S | re.M)

SOURCES = ["qg_draw.c", "qg_draw_pct.c", "qg_block.c", "qg_palette.c", "qg_text.c", "qg_image.c",
           "fonts/qg_font_mono_12.c", "fonts/qg_font_sans_16.c", "fonts/qg_font_sans_bold_24.c",
           "backend/qg_backend_buf8.c", "assets/qg_asset.c"]


def find_examples():
    found = []
    for root, _dirs, files in os.walk(MANUAL):
        for f in sorted(files):
            if f.endswith(".md"):
                path = os.path.join(root, f)
                text = open(path, encoding="utf-8").read()
                for m in FENCE.finditer(text):
                    found.append({"name": m.group(1), "opts": m.group(2).split(),
                                  "code": m.group(3), "file": os.path.relpath(path, ROOT),
                                  "text": text})
    return found


def main():
    check_only = "--check" in sys.argv
    os.makedirs(BUILD, exist_ok=True)
    os.makedirs(IMG, exist_ok=True)
    template = open(os.path.join(HERE, "doc_example_template.c")).read()
    cflags = ["-std=c11", "-O1", "-w", "-I" + os.path.join(HERE, "stubs_examples"),
              "-I" + os.path.join(HERE, "stubs"), "-I" + LIB, "-I" + os.path.join(LIB, "assets"),
              "-I" + os.path.join(ROOT, "examples"), "-DQG_ASSET_HOST_TEST"]

    # The library, compiled once (twice: ordinary and framebuffer builds share it).
    objs = []
    for src in SOURCES + ["../examples/example_art.c"]:
        o = os.path.join(BUILD, os.path.basename(src).replace(".c", ".o"))
        r = subprocess.run(["gcc", *cflags, "-c", os.path.join(LIB, src), "-o", o], capture_output=True, text=True)
        if r.returncode:
            sys.exit("library build failed: %s\n%s" % (src, r.stderr))
        objs.append(o)

    # The examples' asset pack, for examples tagged "pack".
    r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "mkpack.py"),
                        os.path.join(ROOT, "examples", "pack"), "--out", os.path.join(BUILD, "pack", "assets")],
                       capture_output=True, text=True)
    if r.returncode:
        sys.exit("mkpack failed:\n" + r.stderr)

    examples = find_examples()
    names = [e["name"] for e in examples]
    dupes = {n for n in names if names.count(n) > 1}
    failures = []
    if dupes:
        failures.append("duplicate example names: " + ", ".join(sorted(dupes)))

    try:
        from PIL import Image
    except ImportError:
        Image = None

    for e in examples:
        name, opts = e["name"], e["opts"]
        src = os.path.join(BUILD, name + ".c")
        open(src, "w").write(template.replace("/*SNIPPET*/", e["code"]))
        extra = ["-DDOC_BUF8"] if "buf8" in opts else []
        if "pack" in opts:
            extra += ["-DDOC_PACK", "-Dqg_asset_init=doc_asset_init"]
        if "compile-only" in opts:
            r = subprocess.run(["gcc", *cflags, *extra, "-fsyntax-only", src], capture_output=True, text=True)
            if r.returncode:
                failures.append("%s (%s): does not compile\n%s" % (name, e["file"], r.stderr[:800]))
            continue
        exe = os.path.join(BUILD, name)
        r = subprocess.run(["gcc", *cflags, *extra, src, *objs, "-lm", "-o", exe], capture_output=True, text=True)
        if r.returncode:
            failures.append("%s (%s): does not compile\n%s" % (name, e["file"], r.stderr[:800]))
            continue
        ppm = os.path.join(BUILD, name + ".ppm")
        r = subprocess.run([exe, ppm], capture_output=True, text=True, cwd=BUILD, timeout=20)
        if r.returncode:
            failures.append("%s (%s): failed when run: %s" % (name, e["file"], r.stderr.strip()))
            continue
        if "img/%s.png" % name not in e["text"]:
            failures.append("%s (%s): its picture img/%s.png isn't shown in the page" % (name, e["file"], name))
        if not check_only and Image is not None:
            Image.open(ppm).save(os.path.join(IMG, name + ".png"), optimize=True)

    ran = sum(1 for e in examples if "compile-only" not in e["opts"])
    print("%d examples in the manual: %d run and pictured, %d compiled only"
          % (len(examples), ran, len(examples) - ran))
    for f in failures:
        print("FAIL  " + f)
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
