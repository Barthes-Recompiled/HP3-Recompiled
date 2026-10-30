#!/usr/bin/env python3
"""Fail when hand-written code names a guest function that is not a function start in this XBE.

    python3 port/tools/check_no_foreign_addrs.py --functions tools/disasm/output/functions.json FILE...

Overrides bind by name (HLE(ADDR, n), FALLBACK(ADDR), sub_ADDR). An address carried over from another title that is not a function
here would replace part of some unrelated function, silently. regen.sh runs this after disassembling, over every
hand-written file; comments and code under `#if 0` are not looked at.

With --asm-dir (and --xbe or --image-range), every hex literal inside the image must also be something this XBE's
disassembly mentions: a global carried from another title at the wrong address reads or overwrites unrelated data.
Plain numbers that happen to fall in the image's range go in retarget.json's "numbers" (--numbers); a deliberate
unreferenced address gets an `// xref-free: <reason>` comment on its line.
"""
import argparse
import json
import os
import re
import sys

# HLE(ADDR, n), FALLBACK(ADDR) and other macros that paste sub_ADDR / sub_ADDR_gen from an 8-digit argument; sub_ADDR
_NAME = re.compile(r"\b[A-Z][A-Z0-9_]*\(\s*([0-9A-Fa-f]{8})\s*[,)]|\bsub_([0-9A-Fa-f]{8})(?:_gen)?\b")


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
        yield n, code, line


_LITERAL = re.compile(r"\b0x([0-9A-Fa-f]{5,8})(?=[uUlL]*\b)")


def referenced_values(asm_dir, sections):
    """Every hex value the title's disassembly mentions (immediates, displacements, targets)."""
    seen = set()
    for sec in sections:
        path = os.path.join(asm_dir, ("text" if sec == ".text" else sec) + ".asm")
        if os.path.exists(path):
            with open(path, encoding="utf-8", errors="replace") as f:
                seen.update(int(v, 16) for v in re.findall(r"\b0x([0-9a-fA-F]+)\b", f.read()))
    return seen


def plain_numbers(path):
    """{file suffix: {value}} from retarget.json's "numbers" (number-like values that are not addresses)."""
    out = {}
    if path:
        with open(path) as f:
            for e in json.load(f).get("numbers", []):
                for fn in e["files"]:
                    out.setdefault(fn.replace("\\", "/"), set()).update(int(v, 16) for v in e["values"])
    return out


def image_range(a):
    if a.image_range:
        lo, hi = a.image_range.split("-")
        return int(lo, 16), int(hi, 16)
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
    from tools.region.xbe_align import Xbe
    x = Xbe(a.xbe)
    return x.lo, x.hi


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--functions", required=True)
    ap.add_argument("--asm-dir", help="tools.disasm's asm/ listings: also check address literals (data) against them")
    ap.add_argument("--xbe", help="the XBE, for the image range the data check covers")
    ap.add_argument("--image-range", help="LO-HI in hex instead of --xbe")
    ap.add_argument("--numbers", help="retarget.json: number-like values per file that are not addresses")
    ap.add_argument("files", nargs="+")
    a = ap.parse_args(argv)
    with open(a.functions) as f:
        fns = json.load(f)
    starts = {int(fn["start"], 16) for fn in fns}
    data = bool(a.asm_dir)
    if data:
        lo, hi = image_range(a)
        refs = referenced_values(a.asm_dir, {fn.get("section", ".text") for fn in fns})
        numbers = plain_numbers(a.numbers)
    bad = []
    for path in a.files:
        with open(path, encoding="utf-8", errors="replace") as f:
            text = f.read()
        norm = path.replace("\\", "/")
        allowed = set().union(*[v for k, v in numbers.items() if norm == k or norm.endswith("/" + k)]) if data else set()
        for n, code, raw in live_lines(text):
            for x, y in _NAME.findall(code):
                addr = int(x or y, 16)
                if addr not in starts:
                    bad.append("%s:%d: %08X is not a function start in this XBE" % (path, n, addr))
            if not data or "xref-free:" in raw:
                continue
            for v in (int(m, 16) for m in _LITERAL.findall(code)):
                if lo <= v < hi and v not in starts and v not in refs and v not in allowed:
                    bad.append("%s:%d: 0x%08X is in the image but nothing in this XBE's code refers to it"
                               " (another title's data address? a plain number goes in retarget.json \"numbers\";"
                               " a deliberate one gets an // xref-free: <reason> comment)" % (path, n, v))
    for b in bad:
        print(b)
    if bad:
        print("check_no_foreign_addrs: %d reference(s) to addresses that are not functions or data here" % len(bad))
        return 1
    print("check_no_foreign_addrs: %d files, every named function is a function start%s"
          % (len(a.files), ", every address literal is one this XBE's code uses" if data else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
