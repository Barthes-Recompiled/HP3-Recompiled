#!/usr/bin/env python3
"""Self-checks for check_hle_coverage.py: py -3 port/tools/test_check_hle_coverage.py"""
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_hle_coverage as c  # noqa: E402

SECTIONS = [(".text", 0x11000, 0x264C64), ("D3D", 0x264C80, 0x278A1C), ("DSOUND", 0x2B66E0, 0x2BF914)]
SINKS = {0x0026C7F0}


def fn(addr, body, gen=False):
    return "void sub_%08X%s(void)\n{\n%s\n}\n" % (addr, "_gen" if gen else "", body)


def call(target, ret=0x00011234):
    return "    PUSH32(esp, 0x%08Xu); RECOMP_ABI_CALL(0x%08Xu, sub_%08X); /* call 0x%08X */" % (ret, target, target, target)


def gaps(text, want=("D3D", "DSOUND"), allow=None):
    g = c.scan_gen([text])
    return {(x.section, x.target) for x in c.find_gaps(g, SECTIONS, allow or {}, want, SINKS)}


def test_direct_call_gap():
    t = fn(0x00011000, call(0x002B752F)) + fn(0x002B752F, "")
    assert gaps(t) == {("DSOUND", 0x002B752F)}


def test_overridden_not_a_gap():
    # regen renames an overridden function's generated body to sub_X_gen
    t = fn(0x00011000, call(0x002B75A3)) + fn(0x002B75A3, "", gen=True)
    assert gaps(t) == set()


def test_hle_under_if0_is_a_gap():
    # an override under #if 0 is not compiled, so regen keeps the plain generated name
    t = fn(0x00011000, call(0x002B7A60)) + fn(0x002B7A60, "")
    assert gaps(t) == {("DSOUND", 0x002B7A60)}


def test_allowlisted_not_a_gap():
    t = fn(0x00011000, call(0x002B752F)) + fn(0x002B752F, "")
    assert gaps(t, allow={0x002B752F: "x: reason"}) == set()


def test_address_taken_counts():
    t = fn(0x00011000, "    MEM32(esi + 8) = 0x002B7603u;") + fn(0x002B7603, "")
    g = c.scan_gen([t])
    found = c.find_gaps(g, SECTIONS, {}, ("DSOUND",), SINKS)
    assert [(x.target, x.callers, x.taken) for x in found] == [(0x002B7603, 0, 1)]


def test_tail_call_counts():
    t = fn(0x00011000, "    g_seh_ebp = ebp; sub_002B761F(); return;") + fn(0x002B761F, "")
    assert gaps(t) == {("DSOUND", 0x002B761F)}


def test_library_internal_caller_ignored():
    # only the title's own code (.text) counts as a caller
    t = fn(0x002B7000, call(0x002B752F)) + fn(0x002B752F, "")
    assert gaps(t) == set()


def test_d3d_gap_only_if_reaches_makespace():
    t = (fn(0x00011000, call(0x00267000) + "\n" + call(0x00267100))
         + fn(0x00267000, call(0x0026C7F0))   # pushes: a gap
         + fn(0x00267100, "")                 # CPU-only: fine
         + fn(0x0026C7F0, "", gen=True))
    assert gaps(t, want=("D3D",)) == {("D3D", 0x00267000)}


def test_d3d_reach_is_transitive():
    t = (fn(0x00011000, call(0x00267000)) + fn(0x00267000, call(0x00267200))
         + fn(0x00267200, call(0x0026C7F0)) + fn(0x0026C7F0, "", gen=True))
    assert gaps(t, want=("D3D",)) == {("D3D", 0x00267000)}


def test_reach_stops_at_override():
    t = (fn(0x00011000, call(0x00267000)) + fn(0x00267000, call(0x00267200))
         + fn(0x00267200, call(0x0026C7F0), gen=True) + fn(0x0026C7F0, "", gen=True))
    assert gaps(t, want=("D3D",)) == set()


def test_allowlist_file():
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as f:
        f.write("# comment\n0x002B752F IDirectSound_Foo: never reaches a voice\n\n")
    try:
        assert c.read_allow(f.name) == {0x002B752F: "IDirectSound_Foo: never reaches a voice"}
    finally:
        os.unlink(f.name)


def test_strict_exit_code():
    with tempfile.TemporaryDirectory() as d:
        with open(os.path.join(d, "recomp_0000.c"), "w") as f:
            f.write(fn(0x00011000, call(0x002B752F)) + fn(0x002B752F, ""))
        args = ["--gen", d, "--sections", ".text:11000-264C64,DSOUND:2B66E0-2BF914", "--section", "DSOUND"]
        assert c.main(args) == 0
        assert c.main(args + ["--strict"]) == 1


if __name__ == "__main__":
    failed = 0
    for name, f in sorted(globals().items()):
        if name.startswith("test_") and callable(f):
            try:
                f()
                print("ok  ", name)
            except Exception as e:  # noqa: BLE001
                failed += 1
                print("FAIL", name, type(e).__name__, e)
    print("%d failed" % failed)
    sys.exit(1 if failed else 0)
