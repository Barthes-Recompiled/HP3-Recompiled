#!/usr/bin/env python3
"""List XDK library functions the title calls that the HLE does not replace.

    py -3 port/tools/check_hle_coverage.py --gen port/src/recomp/gen --analysis game/default_analysis.json \
        [--allow port/hle_coverage_allow.txt] [--section D3D] [--section DSOUND] [--strict]

The HLE replaces the XDK's Direct3D and DirectSound with host code. A title-called library function it does not
replace runs the XDK's own body, which for Direct3D writes NV2A push-buffer commands the host renderer never reads,
and for DirectSound mixes XDK voices with the host mixer's. This finds them in the generated code:

* called: a direct call (RECOMP_ABI_CALL) or tail call (sub_X(); return;) from a function in .text, or the
  function's address as a literal in .text code (a function pointer the title stores or pushes);
* replaced: regen.sh renames an overridden function's generated body to sub_X_gen, so the generated code itself
  says what the build overrides (an override under #if 0 is not one);
* DSOUND gap: called and not replaced;
* D3D gap: called, not replaced, and its body reaches, through functions that are not replaced either, a call to
  D3DDevice_MakeSpace / D3D_MakeRequestedSpace / D3DDevice_BeginStateBig (it pushes hardware commands).

Allowlist lines: `0x<addr> <name>: <reason>` (`#` comments). Prints one line per gap; exits 1 only with --strict.
Indirect calls through tables the title builds at run time are not seen: at run time hle_d3d8.cpp logs
"d3d: MakeSpace from <caller>" for any push that still happens.
"""
import argparse
import collections
import glob
import json
import os
import re
import sys

# PoA USA: D3DDevice_MakeSpace, D3D_MakeRequestedSpace, D3DDevice_BeginStateBig (hle_d3d8.cpp)
PUSH_SINKS = {0x0026C7F0, 0x0026C6B0, 0x0026C800}

_DEF = re.compile(r"^void sub_([0-9A-F]{8})(_gen)?\(void\)\s*$")
_CALL = re.compile(r"RECOMP_ABI_CALL\(0x([0-9A-Fa-f]{8})u")
_TAIL = re.compile(r"\bsub_([0-9A-F]{8})\(\);\s*return;")
_SKIP = re.compile(r"RECOMP_ABI_CALL\([^)]*\)|PUSH32\(esp, 0x[0-9A-Fa-f]+u\)|/\*.*?\*/|_jt == 0x[0-9A-Fa-f]+u")
_LIT = re.compile(r"\b0x([0-9A-Fa-f]{6,8})u\b")

Gen = collections.namedtuple("Gen", "defs replaced calls lits")
Gap = collections.namedtuple("Gap", "section target callers taken reason")


def scan_gen(texts):
    """Function starts, replaced ones, {caller: [targets]} and {caller: [address-like literals]}."""
    defs, replaced = set(), set()
    calls, lits = collections.defaultdict(list), collections.defaultdict(list)
    cur = None
    for text in texts:
        for line in text.splitlines():
            m = _DEF.match(line)
            if m:
                cur = int(m.group(1), 16)
                defs.add(cur)
                if m.group(2):
                    replaced.add(cur)
                continue
            if cur is None or "0x" not in line and "sub_" not in line:
                continue
            calls[cur].extend(int(t, 16) for t in _CALL.findall(line))
            calls[cur].extend(int(t, 16) for t in _TAIL.findall(line))
            lits[cur].extend(int(v, 16) for v in _LIT.findall(_SKIP.sub("", line)))
    return Gen(defs, replaced, calls, lits)


def _section(sections, a):
    for name, lo, hi in sections:
        if lo <= a < hi:
            return name
    return None


def _pushers(gen, sections, sinks):
    """D3D functions that are not replaced and reach a push-buffer sink through others that are not replaced."""
    d3d = {f for f in gen.defs if _section(sections, f) == "D3D" and f not in gen.replaced}
    reach = {f for f in d3d if any(t in sinks for t in gen.calls.get(f, ()))}
    grew = True
    while grew:
        grew = False
        for f in d3d - reach:
            if any(t in reach for t in gen.calls.get(f, ())):
                reach.add(f)
                grew = True
    return reach


def find_gaps(gen, sections, allow, want, sinks=PUSH_SINKS):
    callers, taken = collections.Counter(), collections.Counter()
    for f in gen.calls.keys() | gen.lits.keys():
        if _section(sections, f) != ".text":
            continue
        for t in gen.calls.get(f, ()):
            callers[t] += 1
        for v in gen.lits.get(f, ()):
            if v in gen.defs:
                taken[v] += 1
    pushers = _pushers(gen, sections, sinks) if "D3D" in want else set()
    out = []
    for t in sorted(callers.keys() | taken.keys()):
        sec = _section(sections, t)
        if sec not in want or t in gen.replaced or t in allow:
            continue
        if sec == "D3D":
            if t not in pushers:
                continue
            reason = "pushes hardware commands"
        else:
            reason = "runs the XDK body"
        out.append(Gap(sec, t, callers[t], taken[t], reason))
    return out


def read_allow(path):
    out = {}
    if not path or not os.path.exists(path):
        return out
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.split("#", 1)[0].strip()
            if line:
                addr, _, why = line.partition(" ")
                out[int(addr, 16)] = why.strip()
    return out


def read_sections(a):
    if a.sections:
        out = []
        for part in a.sections.split(","):
            name, rng = part.split(":")
            lo, hi = rng.split("-")
            out.append((name, int(lo, 16), int(hi, 16)))
        return out
    with open(a.analysis) as f:
        return [(s["name"], int(s["virtual_addr"], 16), int(s["virtual_addr"], 16) + s["virtual_size"])
                for s in json.load(f)["sections"]]


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--gen", required=True, help="port/src/recomp/gen")
    ap.add_argument("--analysis", help="game/default_analysis.json (the section table)")
    ap.add_argument("--sections", help="NAME:LO-HI,... in hex instead of --analysis")
    ap.add_argument("--allow", help="allowlist: 0x<addr> <name>: <reason>")
    ap.add_argument("--section", action="append", choices=["D3D", "DSOUND"], help="sections to check (default both)")
    ap.add_argument("--strict", action="store_true", help="exit 1 when there is a gap")
    a = ap.parse_args(argv)
    texts = []
    for p in sorted(glob.glob(os.path.join(a.gen, "recomp_*.c"))):
        with open(p, encoding="utf-8", errors="replace") as f:
            texts.append(f.read())
    want = tuple(a.section or ("D3D", "DSOUND"))
    gaps = find_gaps(scan_gen(texts), read_sections(a), read_allow(a.allow), want)
    for g in gaps:
        print("%-6s 0x%08X %3d direct caller(s) %2d address-taken  %s" % (g.section, g.target, g.callers, g.taken, g.reason))
    per = collections.Counter(g.section for g in gaps)
    print("check_hle_coverage: %s" % (", ".join("%s %d gap(s)" % (s, per[s]) for s in want)))
    return 1 if a.strict and gaps else 0


if __name__ == "__main__":
    sys.exit(main())
