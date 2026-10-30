"""
Self-check for check_no_foreign_addrs.py.

Run: py -3 port/tools/test_check_no_foreign_addrs.py

An override bound to an address that is not a function start in this XBE silently replaces part of some other
function (hand-written code carried over from another title). The checker must name exactly those lines, and must not
be fooled by mentions in comments or by code switched off with #if 0.
"""
import json
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = os.path.join(HERE, "check_no_foreign_addrs.py")

SRC = """\
/* sub_00099999 in a block comment */
HLE(00011000, 1) {
    return 0;
}
// sub_00088888 in a line comment
extern void sub_00022222_gen(void);
#if 0  /* not bound */
HLE(00077777, 0) { return 0; }
#endif
void sub_00011000(void);
"""


def run(src, starts):
    with tempfile.TemporaryDirectory() as d:
        c = os.path.join(d, "x.c")
        f = os.path.join(d, "functions.json")
        with open(c, "w") as fh:
            fh.write(src)
        with open(f, "w") as fh:
            json.dump([{"start": "0x%08X" % s} for s in starts], fh)
        p = subprocess.run([sys.executable, TOOL, "--functions", f, c], capture_output=True, text=True)
        return p.returncode, p.stdout + p.stderr


def test_flags_only_live_foreign_names():
    rc, out = run(SRC, [0x11000])
    assert rc == 1, out
    assert "x.c:6" in out and "00022222" in out, out
    for quiet in ("00099999", "00088888", "00077777"):
        assert quiet not in out, out


def test_clean_file_passes():
    rc, out = run(SRC, [0x11000, 0x22222])
    assert rc == 0, out


if __name__ == "__main__":
    failed = 0
    for t in (test_flags_only_live_foreign_names, test_clean_file_passes):
        try:
            t()
            print("ok   ", t.__name__)
        except Exception as e:
            failed += 1
            print("FAIL ", t.__name__, "-", type(e).__name__, e)
    sys.exit(1 if failed else 0)
