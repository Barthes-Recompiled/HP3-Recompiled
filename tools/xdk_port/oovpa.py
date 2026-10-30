"""
Find XDK library functions in a title with XbSymbolDatabase's signatures (OOVPAs), a second source beside the
cross-title body match in match.py.

    py -3 -m tools.xdk_port.oovpa --db <XbSymbolDatabase>/src/OOVPADatabase --xbe game/default.xbe \
        --functions tools/disasm/output/functions.json --libs D3D8:5849,DSOUND:5849,XAPILIB:5849 \
        [--known <xdk_port map.json>...] -o tools/xdk_port/output/oovpa_poa.json

XbSymbolDatabase (Cxbx-Reloaded, MIT) is read from a local clone and never copied into this repository. An OOVPA is
a list of (offset, byte) pairs plus XREF entries: at an offset, a rel32 call/jump or an absolute address that must
point at another known symbol. For each symbol the scan takes the highest signature version not above the library's
version (XbSymbolDatabase's own rule), and accepts a hit only at a function start of the library's section, and only
when it is the one such start there. An XREF entry is checked when its symbol is known (an earlier hit, or a name in
--known); while it is not, it is skipped and the signature needs 8 byte pairs of its own. Passes repeat until
nothing new is found. Nothing is guessed.
"""
import argparse
import collections
import glob
import json
import os
import re
import struct
import sys

Sig = collections.namedtuple("Sig", "name version pairs xrefs")

# library name (XBE library table) -> OOVPADatabase folder, section holding its code
LIBS = {"D3D8": ("D3D8", "D3D"), "DSOUND": ("DSound", "DSOUND"), "XAPILIB": ("Xapi", ".text"), "XGRAPHC": ("XGraphic", "XGRPH")}
MIN_PAIRS = 8

_HEAD = re.compile(r"\bOOVPA_(SIG_HEADER_XREF|SIG_HEADER_NO_XREF|SIG_HEADER_XREF_DETECT|SIG_HEADER_NO_XREF_DETECT"
                   r"|XREF|NO_XREF)\s*\(\s*(\w+)\s*,\s*(\d+)")
_PAIR = re.compile(r"\{\s*(0x[0-9A-Fa-f]+|\d+)\s*,\s*(0x[0-9A-Fa-f]+|\d+)\s*\}")
_OVM = re.compile(r"\bOV_(?:MATCH|BYTES|BYTE)\s*\(([^)]*)\)")
_XREF = re.compile(r"\bXREF_ENTRY\s*\(\s*(0x[0-9A-Fa-f]+|\d+)\s*,\s*XREF_(\w+)\s*\)")


def parse_oovpa(text):
    """Every signature in an .inl text (both the OOVPA_SIG_HEADER_* + OOVPA_SIG_MATCH and the OOVPA_*XREF styles)."""
    text = re.sub(r"/\*.*?\*/", " ", re.sub(r"//[^\n]*", "", text), flags=re.S)
    heads = list(_HEAD.finditer(text))
    out = []
    for i, m in enumerate(heads):
        end = heads[i + 1].start() if i + 1 < len(heads) else len(text)
        body = text[m.end():end]
        if "OOVPA_SIG_MATCH_DUMMY" in body:
            continue
        pairs, xrefs = [], []
        for p in _PAIR.finditer(body):
            pairs.append((int(p.group(1), 0), int(p.group(2), 0)))
        for v in _OVM.finditer(body):
            args = [x.strip() for x in v.group(1).split(",") if x.strip()]
            off = int(args[0], 0)
            if len(args) == 2 and args[1].startswith("XREF_"):  # OV_MATCH(offset, XREF_name): an XREF entry
                xrefs.append((off, args[1][5:]))
                continue
            pairs.extend((off + k, int(b, 0)) for k, b in enumerate(args[1:]))
        for x in _XREF.finditer(body):
            xrefs.append((int(x.group(1), 0), x.group(2)))
        if pairs or xrefs:
            out.append(Sig(m.group(2), int(m.group(3)), sorted(pairs), xrefs))
    return out


def pick_versions(sigs, version):
    """Per symbol, the signature with the highest version not above `version`."""
    best = {}
    for s in sigs:
        if s.version <= version and (s.name not in best or s.version > best[s.name].version):
            best[s.name] = s
    return [best[n] for n in sorted(best)]


def _reader(img):
    secs = [(s.va, s.bytes) for s in img.sections]

    def at(a, n):
        for va, b in secs:
            if va <= a and a + n <= va + len(b):
                return b[a - va:a - va + n]
        return None
    return at


def _fits(at, s, sig, known):
    """True when sig matches at s; None when it can't say (an unknown XREF and too few pairs)."""
    for off, b in sig.pairs:
        got = at(s + off, 1)
        if got is None or got[0] != b:
            return False
    unknown = 0
    for off, name in sig.xrefs:
        if name not in known:
            unknown += 1
            continue
        raw = at(s + off, 4)
        if raw is None:
            return False
        v = struct.unpack("<I", raw)[0]
        if known[name] not in ((s + off + 4 + v) & 0xFFFFFFFF, v):
            return False
    if unknown and len(sig.pairs) < MIN_PAIRS:
        return None
    return True


def scan(img, sections, starts, sigs, known):
    """({address: Sig} accepted hits, [names not found]). sections: [(name, lo, hi)] the signatures may hit in."""
    at = _reader(img)
    cands = sorted(a for a in starts if any(lo <= a < hi for _, lo, hi in sections))
    known = dict(known)
    hits, done = {}, set()
    grew = True
    while grew:
        grew = False
        taken = set(hits)
        for sig in sigs:
            if sig.name in done:
                continue
            fit = [a for a in cands if _fits(at, a, sig, known) is True]
            if len(fit) == 1 and fit[0] not in taken:
                hits[fit[0]] = sig
                known[sig.name] = fit[0]
                done.add(sig.name)
                taken.add(fit[0])
                grew = True
    unmatched = sorted(s.name for s in sigs if s.name not in done)
    return hits, unmatched


def load_known(paths):
    """{name: target address} from xdk_port maps (named entries only)."""
    out = {}
    for p in paths or ():
        with open(p) as f:
            for v in json.load(f).get("matched", {}).values():
                if isinstance(v, dict) and v.get("name"):
                    out[v["name"]] = int(v["to"], 16)
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(prog="tools.xdk_port.oovpa", description=__doc__.split("\n\n")[0])
    ap.add_argument("--db", required=True, help="XbSymbolDatabase/src/OOVPADatabase")
    ap.add_argument("--xbe", required=True)
    ap.add_argument("--functions", required=True, help="tools.disasm functions.json of the XBE")
    ap.add_argument("--libs", required=True, help="LIB:VERSION,... e.g. D3D8:5849,DSOUND:5849,XAPILIB:5849")
    ap.add_argument("--known", nargs="*", help="xdk_port maps whose named matches count as known XREF targets")
    ap.add_argument("-o", "--out", required=True)
    a = ap.parse_args(argv)
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
    from tools.region.xbe_align import Xbe
    img = Xbe(a.xbe)
    with open(a.functions) as f:
        starts = {int(fn["start"], 16) for fn in json.load(f)}
    secs = {s.name: (s.va, s.va + len(s.bytes)) for s in img.sections}
    known = load_known(a.known)
    out = {"xbe_sha1": img.sha1, "db": os.path.abspath(a.db), "hits": {}, "unmatched": {}}
    for spec in a.libs.split(","):
        lib, ver = spec.split(":")
        folder, sec = LIBS[lib]
        sigs = []
        for p in sorted(glob.glob(os.path.join(a.db, folder, "*.inl"))):
            with open(p, encoding="utf-8", errors="replace") as f:
                sigs.extend(parse_oovpa(f.read()))
        sigs = pick_versions(sigs, int(ver))
        lo, hi = secs[sec]
        hits, unmatched = scan(img, [(sec, lo, hi)], starts, sigs, known)
        known.update({s.name: addr for addr, s in hits.items()})
        for addr, s in sorted(hits.items()):
            out["hits"]["0x%08X" % addr] = {"name": s.name, "lib": lib, "version": s.version}
        out["unmatched"][lib] = unmatched
        print("oovpa: %s %s: %d of %d signatures found" % (lib, ver, len(hits), len(sigs)))
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, "w") as f:
        json.dump(out, f, indent=1)
    return 0


if __name__ == "__main__":
    sys.exit(main())
