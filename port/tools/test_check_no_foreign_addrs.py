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
void f(void) { FALLBACK(00033333); }
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
    assert "x.c:11" in out and "00033333" in out, out  # a macro that pastes sub_ADDR_gen from its argument
    for quiet in ("00099999", "00088888", "00077777"):
        assert quiet not in out, out


def test_clean_file_passes():
    rc, out = run(SRC, [0x11000, 0x22222, 0x33333])
    assert rc == 0, out


DATA_SRC = """\
constexpr uint32_t kGood = 0x00012004;   // referenced by the title's code
constexpr uint32_t kBad = 0x00012000;    // in the image, referenced by nothing
constexpr uint32_t kNumber = 0x00020000; // a plain number (retarget.json numbers)
constexpr uint32_t kEnd = 0x00013000;    // xref-free: section end
uint32_t kFar = 0x10000000;              // outside the image
#if 0
uint32_t kOff = 0x00012008;
#endif
"""


def run_data(src, asm, numbers):
    with tempfile.TemporaryDirectory() as d:
        c = os.path.join(d, "x.c")
        f = os.path.join(d, "functions.json")
        a = os.path.join(d, "asm")
        os.mkdir(a)
        with open(c, "w") as fh:
            fh.write(src)
        with open(f, "w") as fh:
            json.dump([{"start": "0x00011000", "section": ".text"}], fh)
        with open(os.path.join(a, "text.asm"), "w") as fh:
            fh.write(asm)
        r = os.path.join(d, "retarget.json")
        with open(r, "w") as fh:
            json.dump({"numbers": [{"values": numbers, "files": ["x.c"]}]}, fh)
        p = subprocess.run([sys.executable, TOOL, "--functions", f, "--asm-dir", a, "--image-range", "0x10000-0x14000",
                            "--numbers", r, c], capture_output=True, text=True)
        return p.returncode, p.stdout + p.stderr


def test_flags_unreferenced_data_literal():
    rc, out = run_data(DATA_SRC, "  00011000: mov eax, dword ptr [0x12004]\n", ["0x20000"])
    assert rc == 1, out
    assert "x.c:2" in out and "00012000" in out, out
    for quiet in ("00012004", "00020000", "00013000", "10000000", "00012008"):
        assert quiet not in out, out


def test_referenced_data_passes():
    rc, out = run_data(DATA_SRC.replace("0x00012000", "0x00012004"), "  mov eax, [0x12004]\n", ["0x20000"])
    assert rc == 0, out

if __name__ == "__main__":
    failed = 0
    for t in (test_flags_only_live_foreign_names, test_clean_file_passes, test_flags_unreferenced_data_literal,
              test_referenced_data_passes):
        try:
            t()
            print("ok   ", t.__name__)
        except Exception as e:
            failed += 1
            print("FAIL ", t.__name__, "-", type(e).__name__, e)
    sys.exit(1 if failed else 0)
