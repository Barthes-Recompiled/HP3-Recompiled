#!/usr/bin/env python3
"""Fail when hand-written code names a guest function that is not a function start in this XBE.

    python3 port/tools/check_no_foreign_addrs.py --functions tools/disasm/output/functions.json FILE...

Overrides bind by name (HLE(ADDR, n), sub_ADDR). An address carried over from another title that is not a function
here would replace part of some unrelated function, silently. regen.sh runs this after disassembling, over every
hand-written file; comments and code under `#if 0` are not looked at.
"""
import argparse
import json
import re
import sys

_NAME = re.compile(r"\bHLE\(\s*([0-9A-Fa-f]{8})\s*,|\bsub_([0-9A-Fa-f]{8})(?:_gen)?\b")


def live_lines(text):
    """(line number, code) with comments blanked and #if 0 ... #endif skipped."""
    text = re.sub(r"/\*.*?\*/", lambda m: re.sub(r"[^\n]", " ", m.group(0)), text, flags=re.S)
    depth = 0  # nesting inside an #if 0
    for n, line in enumerate(text.splitlines(), 1):
        code = line.split("//", 1)[0]
        d = code.strip()
        if depth:
            if re.match(r"#\s*if", d):
                depth += 1
            elif re.match(r"#\s*endif", d):
                depth -= 1
            continue
        if re.match(r"#\s*if\s+0\b", d):
            depth = 1
            continue
        yield n, code


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--functions", required=True)
    ap.add_argument("files", nargs="+")
    a = ap.parse_args(argv)
    with open(a.functions) as f:
        starts = {int(fn["start"], 16) for fn in json.load(f)}
    bad = []
    for path in a.files:
        with open(path, encoding="utf-8", errors="replace") as f:
            text = f.read()
        for n, code in live_lines(text):
            for x, y in _NAME.findall(code):
                addr = int(x or y, 16)
                if addr not in starts:
                    bad.append("%s:%d: %08X is not a function start in this XBE" % (path, n, addr))
    for b in bad:
        print(b)
    if bad:
        print("check_no_foreign_addrs: %d reference(s) to addresses that are not functions here" % len(bad))
        return 1
    print("check_no_foreign_addrs: %d files, every named function is a function start" % len(a.files))
    return 0


if __name__ == "__main__":
    sys.exit(main())
