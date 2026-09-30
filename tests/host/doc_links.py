#!/usr/bin/env python3
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
"""
doc_links.py - check every link and picture in the documentation.

    python3 tests/host/doc_links.py

For every Markdown file in docs/ and the READMEs, every relative link
([text](path#anchor)) and picture (<img src="...">) must point at a file
or folder that exists, and every #anchor at a real heading in that file,
worked out the way GitHub turns headings into anchors. Web links are skipped.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
LINK = re.compile(r"(?<!!)\[[^\]]*\]\(([^)\s]+)\)|!\[[^\]]*\]\(([^)\s]+)\)|<img[^>]*\ssrc=\"([^\"]+)\"")
FENCE = re.compile(r"^```.*?^```", re.S | re.M)


def slug(heading):
    """GitHub's anchor for a heading: lower case, punctuation dropped, spaces to hyphens."""
    h = re.sub(r"`", "", heading.strip().lower())
    h = re.sub(r"[^\w\- ]", "", h)
    return h.replace(" ", "-")


def anchors(path, cache={}):
    if path not in cache:
        seen, found = {}, set()
        text = FENCE.sub("", open(path, encoding="utf-8").read())
        for m in re.finditer(r"^#{1,6}\s+(.*)$", text, re.M):
            s = slug(m.group(1))
            n = seen.get(s, 0)
            found.add(s if n == 0 else "%s-%d" % (s, n))
            seen[s] = n + 1
        cache[path] = found
    return cache[path]


def main():
    files = []
    for top in ("docs", "examples", "tests"):
        for root, _d, fs in os.walk(os.path.join(ROOT, top)):
            if "build" in root.split(os.sep):
                continue
            files += [os.path.join(root, f) for f in fs if f.endswith(".md")]
    files += [os.path.join(ROOT, f) for f in ("README.md", "CHANGELOG.md")]

    problems, checked = [], 0
    for path in sorted(files):
        text = FENCE.sub("", open(path, encoding="utf-8").read())
        for m in LINK.finditer(text):
            target = m.group(1) or m.group(2) or m.group(3)
            if re.match(r"^[a-z]+:", target):
                continue                              # web and mail links
            checked += 1
            file_part, _, anchor = target.partition("#")
            dest = path if not file_part else os.path.normpath(os.path.join(os.path.dirname(path), file_part))
            where = os.path.relpath(path, ROOT)
            if not os.path.exists(dest):
                problems.append("%s: %s -> no such file" % (where, target))
            elif anchor and dest.endswith(".md") and anchor not in anchors(dest):
                problems.append("%s: %s -> no heading '#%s' in %s" % (where, target, anchor, os.path.relpath(dest, ROOT)))

    print("%d links and pictures checked in %d files" % (checked, len(files)))
    for p in problems:
        print("BROKEN  " + p)
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
