#!/usr/bin/env python3
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
"""
check_quickref.py - the one-page quick reference stays complete and true.

    python3 tests/host/check_quickref.py

Every public function declared in the library's headers must appear in
docs/manual/quick-reference.md, and every qg_ name the page mentions must
exist (as a function, macro, type or constant) somewhere in the headers.
"""
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
LIB = os.path.join(ROOT, "qg4p")
HEADERS = ["qg_screen.h", "qg_draw.h", "qg_draw_pct.h", "qg_block.h", "qg_text.h", "qg_image.h",
           "qg_palette.h", "qg_types.h", "qg_config.h", "hal/qg_hal.h", "assets/qg_asset.h", "qg_flash.h",
           "fonts/qg_font_mono_12.c", "fonts/qg_font_sans_16.c", "fonts/qg_font_sans_bold_24.c"]

public, known = set(), set()
for h in HEADERS:
    text = open(os.path.join(LIB, h)).read()
    known |= set(re.findall(r"\b(qg_\w+|QG_\w+)\b", text))
    if h.endswith(".h") and "config" not in h and "types" not in h:
        code = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
        for m in re.finditer(r"^(?:static inline\s+)?[a-zA-Z_][\w\s\*]*?\b(qg_\w+)\s*\(", code, re.M):
            if not m.group(1).startswith(("qg_hal_", "qg_int_")):
                public.add(m.group(1))

page = open(os.path.join(ROOT, "docs", "manual", "quick-reference.md")).read()
mentioned = set(re.findall(r"\b(qg_\w+|QG_\w+)\b", page))

missing = sorted(public - mentioned)
unknown = sorted(n for n in mentioned - known if not n.startswith(("QG_ROT_", "QG_ALIGN_")) or n not in known)
unknown = [n for n in unknown if n not in known]
print("%d public functions; the quick reference names %d qg_/QG_ identifiers" % (len(public), len(mentioned)))
for n in missing:
    print("MISSING from the quick reference: " + n)
for n in unknown:
    print("NOT IN THE LIBRARY: " + n)
sys.exit(1 if missing or unknown else 0)
