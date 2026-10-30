"""
Carry the library functions hand-written overrides bind to from a donor title to a target title.

    py -3 -m tools.xdk_port --donor hp1/default.xbe --donor-functions hp1_functions.json \\
        --donor-names port/src/scda/xdk/hle_d3d8.cpp port/src/hle_dsound.c ... \\
        --target game/default.xbe --target-functions tools/disasm/output/functions.json \\
        -o tools/xdk_port/output/map.json

--donor-names takes the donor's override sources (every binding macro such as HLE(ADDR, n) and sub_ADDR the code defines, declares or
calls; mentions inside comments are left out, a // Name comment on the line or the line before names it) or a JSON
{"0xADDR": "name"}. The functions.json files are tools.disasm's output for each XBE.

The map holds addresses, names and counts only (no image bytes); keep it out of the repository like other tool output.
"""
import argparse
import json
import os
import re
import sys

from tools.region.xbe_align import Xbe
from tools.xdk_port.match import match_functions

# HLE(ADDR, n), COMPLEX_STATE(ADDR, ...), FALLBACK(ADDR): binding macros with the address first; sub_ADDR( calls,
# definitions and declarations
_FUNC = re.compile(r"\b[A-Z][A-Z0-9_]*\(\s*([0-9A-Fa-f]{8})\s*[,)]|\bsub_([0-9A-Fa-f]{8})(?:_gen)?\s*\(")
_NAME = re.compile(r"//\s*([A-Za-z_][A-Za-z0-9_:]*)")


def names_from_source(text):
    """{addr: name or ""} for the functions a C/C++ source defines, declares or calls."""
    out = {}
    in_block = False
    prev_name = ""
    for line in text.splitlines():
        code, comment = "", ""
        i = 0
        while i < len(line):
            if in_block:
                j = line.find("*/", i)
                if j < 0:
                    i = len(line)
                    break
                in_block, i = False, j + 2
            elif line.startswith("/*", i):
                in_block, i = True, i + 2
            elif line.startswith("//", i):
                comment = line[i:]
                break
            else:
                code += line[i]
                i += 1
        m = _NAME.match(comment.strip()) if comment else None
        name_here = m.group(1) if m else ""
        found = [int(a or b, 16) for a, b in _FUNC.findall(code)]
        for a in found:
            name = name_here or prev_name
            if name or a not in out:
                out[a] = name
        prev_name = name_here if not code.strip() else ""
    return out


def load_names(paths):
    names = {}
    for p in paths:
        with open(p, encoding="utf-8", errors="replace") as f:
            text = f.read()
        if p.endswith(".json"):
            got = {int(k, 16): v for k, v in json.loads(text).items()}
        else:
            got = names_from_source(text)
        for a, n in got.items():
            if n or a not in names:
                names[a] = n
    return names


def load_starts(path):
    with open(path) as f:
        return {int(fn["start"], 16) for fn in json.load(f)}


def main(argv=None):
    ap = argparse.ArgumentParser(prog="tools.xdk_port", description=__doc__.split("\n\n")[0])
    ap.add_argument("--donor", required=True)
    ap.add_argument("--donor-functions", required=True)
    ap.add_argument("--donor-names", required=True, nargs="+")
    ap.add_argument("--target", required=True)
    ap.add_argument("--target-functions", required=True)
    ap.add_argument("-o", "--out", required=True)
    a = ap.parse_args(argv)

    donor, target = Xbe(a.donor), Xbe(a.target)
    names = load_names(a.donor_names)
    dstarts = load_starts(a.donor_functions)
    # only addresses that are functions in the donor: a stray name in a source is not a library function
    addrs = {x: n for x, n in names.items() if x in dstarts}
    r = match_functions(donor, addrs, dstarts, target, load_starts(a.target_functions))
    out = {
        "donor_sha1": donor.sha1, "target_sha1": target.sha1,
        "counts": {"names": len(names), "donor_functions": len(addrs), "matched": len(r["matched"]), "anchored": len(r["anchored"]), "callee": len(r["callee"]),
                   "unmatched": len(r["unmatched"]), "not_donor_functions": len(names) - len(addrs)},
        "matched": {"0x%08X" % d: {"to": "0x%08X" % t, "name": addrs[d], "how": "anchored" if d in r["anchored"] else "callee" if d in r["callee"] else "unique"}
                    for d, t in sorted(r["matched"].items())},
        "unmatched": {"0x%08X" % d: {"why": w, "name": addrs[d]} for d, w in sorted(r["unmatched"].items())},
        "not_donor_functions": ["0x%08X" % x for x in sorted(set(names) - set(addrs))],
    }
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, "w") as f:
        json.dump(out, f, indent=1)
    c = out["counts"]
    print("xdk_port: %d of %d donor functions matched (%d by a neighbour, %d by callees), %d unmatched (%d names were not donor"
          " functions) -> %s" % (c["matched"], c["donor_functions"], c["anchored"], c["callee"], c["unmatched"],
                                 c["not_donor_functions"], a.out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
