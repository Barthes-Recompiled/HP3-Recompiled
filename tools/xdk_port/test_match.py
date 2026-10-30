"""
Self-check for the cross-title XDK matcher.

Run: py -3 tools/xdk_port/test_match.py

Builds two tiny images in memory, so it runs anywhere. The properties worth pinning:

  - a library function is found in another title although every absolute pointer
    and every rel32 branch in it differs (the code is the same, its neighbours moved);
  - only a whole-function match counts: a shared opening is not a match;
  - a body found twice, or found somewhere that is not a function start, is
    reported with its reason instead of being guessed. A wrong address bound to an
    override silently replaces an unrelated function, which is worse than none.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))

from tools.region.xbe_align import SEC_EXEC, Section, Xbe, mask_pointers  # noqa: E402
from tools.xdk_port.match import match_functions  # noqa: E402

TEXT = 0x11000


def image(code, text_va=TEXT):
    """An Xbe with one executable section holding code (no file on disk)."""
    x = Xbe.__new__(Xbe)
    x.path, x.sha1, x.base = "<memory>", "", 0x10000
    x.sections = [Section(".text", SEC_EXEC, text_va, len(code), 0, len(code), bytes(code))]
    x.lo, x.hi = x.base, text_va + len(code)
    x._starts = [text_va]
    for s in x.sections:
        s.masked = mask_pointers(s.bytes, x.lo, x.hi)
    return x


def fn(ptr, call_from, call_to, tail=b""):
    """push ebp; mov ebp,esp; mov eax,[ptr]; call call_to; <tail>; pop ebp; ret"""
    body = b"\x55\x8B\xEC" + b"\xA1" + struct.pack("<I", ptr)
    rel = call_to - (call_from + len(body) + 5)
    return body + b"\xE8" + struct.pack("<i", rel) + tail + b"\x5D\xC3"


def place(blobs, size=0x200):
    """Lay blobs out at TEXT+offset (dict offset -> bytes), int3 padding between."""
    buf = bytearray(b"\xCC" * size)
    for off, b in blobs.items():
        buf[off:off + len(b)] = b
    return buf


# donor: the function at TEXT+0x00, a neighbour at TEXT+0x40 (its extent ends there)
DONOR = place({0x00: fn(TEXT + 0x100, TEXT + 0x00, TEXT + 0x80), 0x40: b"\x33\xC0\xC3", 0x80: b"\xC3"})
DONOR_STARTS = {TEXT, TEXT + 0x40, TEXT + 0x80}
A = TEXT


def test_exact_match():
    target = place({0x20: fn(TEXT + 0x100, TEXT + 0x20, TEXT + 0xA0), 0xA0: b"\xC3"})
    r = match_functions(image(DONOR), {A: "D3DDevice_Foo"}, DONOR_STARTS, image(target), {TEXT + 0x20, TEXT + 0xA0})
    assert r["matched"] == {A: TEXT + 0x20}, r
    assert r["unmatched"] == {}, r


def test_pointer_masked():
    target = place({0x20: fn(TEXT + 0x150, TEXT + 0x20, TEXT + 0xA0), 0xA0: b"\xC3"})
    r = match_functions(image(DONOR), {A: "f"}, DONOR_STARTS, image(target), {TEXT + 0x20})
    assert r["matched"] == {A: TEXT + 0x20}, r


def test_rel32_masked():
    target = place({0x30: fn(TEXT + 0x100, TEXT + 0x30, TEXT + 0x1F0), 0x1F0: b"\xC3"})
    r = match_functions(image(DONOR), {A: "f"}, DONOR_STARTS, image(target), {TEXT + 0x30})
    assert r["matched"] == {A: TEXT + 0x30}, r


def test_prefix_only_rejected():
    # same opening, one more instruction (inc eax) before the epilogue
    target = place({0x20: fn(TEXT + 0x100, TEXT + 0x20, TEXT + 0xA0, tail=b"\x40")})
    r = match_functions(image(DONOR), {A: "f"}, DONOR_STARTS, image(target), {TEXT + 0x20})
    assert r["matched"] == {}, r
    assert r["unmatched"] == {A: "no-match"}, r


def test_longer_target_rejected():
    # the donor's whole body, then more code of the same function after its ret
    target = place({0x20: fn(TEXT + 0x100, TEXT + 0x20, TEXT + 0xA0) + b"\x33\xC0\xC3"})
    r = match_functions(image(DONOR), {A: "f"}, DONOR_STARTS, image(target), {TEXT + 0x20})
    assert r["unmatched"] == {A: "no-match"}, r


def test_duplicate_rejected():
    target = place({0x20: fn(TEXT + 0x100, TEXT + 0x20, TEXT + 0xA0),
                    0x60: fn(TEXT + 0x100, TEXT + 0x60, TEXT + 0xA0)})
    r = match_functions(image(DONOR), {A: "f"}, DONOR_STARTS, image(target), {TEXT + 0x20, TEXT + 0x60})
    assert r["unmatched"] == {A: "ambiguous"}, r


SHORT = b"\x33\xC0\xC3"  # xor eax,eax; ret: a body many functions share


def test_ambiguous_resolved_by_neighbour():
    # donor: a unique function at +0x00, the short one right after it at +0x40
    target = place({0x20: fn(TEXT + 0x100, TEXT + 0x20, TEXT + 0xA0), 0x60: SHORT, 0x180: SHORT, 0xA0: b"\xC3"})
    r = match_functions(image(DONOR), {A: "f", TEXT + 0x40: "g"}, DONOR_STARTS, image(target),
                        {TEXT + 0x20, TEXT + 0x60, TEXT + 0x180, TEXT + 0xA0})
    assert r["matched"] == {A: TEXT + 0x20, TEXT + 0x40: TEXT + 0x60}, r
    assert r["anchored"] == {TEXT + 0x40}, r


def test_ambiguous_without_anchor_stays():
    # neither copy sits where the neighbour says
    target = place({0x20: fn(TEXT + 0x100, TEXT + 0x20, TEXT + 0xA0), 0x70: SHORT, 0x180: SHORT, 0xA0: b"\xC3"})
    r = match_functions(image(DONOR), {A: "f", TEXT + 0x40: "g"}, DONOR_STARTS, image(target),
                        {TEXT + 0x20, TEXT + 0x70, TEXT + 0x180, TEXT + 0xA0})
    assert r["unmatched"] == {TEXT + 0x40: "ambiguous"}, r


def test_anchor_never_reuses_a_target():
    # g fits 0x60 by its neighbour before (f, delta 0x20), h fits 0x60 by its neighbour after (k, delta 0): one target
    k = b"\x55\x8B\xEC\x40\x40\x5D\xC3"  # push ebp; mov ebp,esp; inc eax; inc eax; pop ebp; ret
    donor = place({0x00: fn(TEXT + 0x100, TEXT + 0x00, TEXT + 0xC0), 0x40: SHORT, 0x60: SHORT, 0x90: k,
                   0xC0: b"\xC3"})
    dstarts = {TEXT, TEXT + 0x40, TEXT + 0x60, TEXT + 0x90, TEXT + 0xC0}
    target = place({0x20: fn(TEXT + 0x100, TEXT + 0x20, TEXT + 0xC0), 0x60: SHORT, 0x180: SHORT, 0x90: k,
                    0xC0: b"\xC3"})
    r = match_functions(image(donor), {A: "f", TEXT + 0x40: "g", TEXT + 0x60: "h", TEXT + 0x90: "k"}, dstarts,
                        image(target), {TEXT + 0x20, TEXT + 0x60, TEXT + 0x180, TEXT + 0x90, TEXT + 0xC0})
    hits = [d for d, t in r["matched"].items() if t == TEXT + 0x60]
    assert len(hits) == 1, r


def test_not_function_start():
    target = place({0x20: fn(TEXT + 0x100, TEXT + 0x20, TEXT + 0xA0)})
    r = match_functions(image(DONOR), {A: "f"}, DONOR_STARTS, image(target), {TEXT + 0x10})
    assert r["unmatched"] == {A: "not-function-start"}, r


def test_names_from_source():
    from tools.xdk_port.__main__ import names_from_source
    src = """
// D3DDevice_BlockOnFence(fence)
HLE(00192060, 1) {
HLE(00193490, 0) { return 0; }  // D3DDevice_IsBusy
extern "C" void sub_0019D860_gen(void);
void sub_001934C0(void)
/* see sub_00012345 in a comment */
"""
    n = names_from_source(src)
    assert n[0x192060] == "D3DDevice_BlockOnFence", n
    assert n[0x193490] == "D3DDevice_IsBusy", n
    assert 0x19D860 in n and 0x1934C0 in n, n
    assert 0x12345 not in n, n  # only defined or declared functions, not mentions in comments


if __name__ == "__main__":
    tests = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    failed = 0
    for t in tests:
        try:
            t()
            print("ok   ", t.__name__)
        except Exception as e:  # report every failure, not just the first
            failed += 1
            print("FAIL ", t.__name__, "-", type(e).__name__, e)
    print("%d/%d passed" % (len(tests) - failed, len(tests)))
    sys.exit(1 if failed else 0)
