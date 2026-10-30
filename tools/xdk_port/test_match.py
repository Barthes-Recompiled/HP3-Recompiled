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


def test_data_map_from_matched_pairs():
    from tools.xdk_port.match import data_map
    # the same function in both; its global moved from TEXT+0x100 to TEXT+0x150
    donor = image(place({0x00: fn(TEXT + 0x100, TEXT + 0x00, TEXT + 0x80), 0x80: b"\xC3"}))
    target = image(place({0x20: fn(TEXT + 0x150, TEXT + 0x20, TEXT + 0xA0), 0xA0: b"\xC3"}))
    m = data_map(donor, {TEXT: TEXT + 0x20}, {TEXT, TEXT + 0x80}, target)
    assert m[TEXT + 0x100] == {TEXT + 0x150: 1}, m


def test_names_from_source():
    from tools.xdk_port.__main__ import names_from_source
    src = """
// D3DDevice_BlockOnFence(fence)
HLE(00192060, 1) {
HLE(00193490, 0) { return 0; }  // D3DDevice_IsBusy
extern "C" void sub_0019D860_gen(void);
void sub_001934C0(void)
/* see sub_00012345 in a comment */
COMPLEX_STATE(00190720, gpu::kRsPsTextureModes)
"""
    n = names_from_source(src)
    assert n[0x192060] == "D3DDevice_BlockOnFence", n
    assert n[0x193490] == "D3DDevice_IsBusy", n
    assert 0x19D860 in n and 0x1934C0 in n, n
    assert 0x12345 not in n, n  # only defined or declared functions, not mentions in comments
    assert 0x190720 in n, n  # a binding macro whose first argument is the address


# callee anchoring: wrappers that are the same bytes except for the function they jump to
def wrap(at, to):
    """mov eax,[esp+4]; jmp to"""
    return b"\x8B\x44\x24\x04" + b"\xE9" + struct.pack("<i", to - (at + 9))


def body(k):
    """push ebp; mov ebp,esp; inc eax * k; pop ebp; ret: a unique body per k"""
    return b"\x55\x8B\xEC" + b"\x40" * k + b"\x5D\xC3"


# donor: wrappers at +0x00/+0x20/+0x40 jumping to X/Y/Z at +0x100/+0x140/+0x180
WD = {0x00: 0x100, 0x20: 0x140, 0x40: 0x180}
CALLEES = {0x100: 1, 0x140: 2, 0x180: 3}
CDONOR = place({**{o: wrap(TEXT + o, TEXT + t) for o, t in WD.items()}, **{o: body(k) for o, k in CALLEES.items()}})
CDSTARTS = {TEXT + o for o in list(WD) + list(CALLEES)}
CNAMES = {TEXT + o: "f%X" % o for o in list(WD) + list(CALLEES)}
# target: callees moved by 0x50, wrappers reordered and spaced so no neighbour distance fits
TCALLEES = {0x150: 1, 0x190: 2, 0x1D0: 3}


def ctarget(wraps):
    return image(place({**{o: wrap(TEXT + o, TEXT + t) for o, t in wraps.items()},
                        **{o: body(k) for o, k in TCALLEES.items()}}))


def test_callee_anchor_picks_wrapper():
    tw = {0x08: 0x1D0, 0x30: 0x150, 0x58: 0x190}  # Z', X', Y'
    r = match_functions(image(CDONOR), CNAMES, CDSTARTS, ctarget(tw), {TEXT + o for o in list(tw) + list(TCALLEES)})
    assert r["matched"][TEXT + 0x00] == TEXT + 0x30, r
    assert r["matched"][TEXT + 0x20] == TEXT + 0x58, r
    assert r["matched"][TEXT + 0x40] == TEXT + 0x08, r
    assert r["callee"] == {TEXT + 0x00, TEXT + 0x20, TEXT + 0x40}, r


def test_callee_anchor_needs_a_mapped_callee():
    tw = {0x08: 0x1D0, 0x30: 0x150, 0x58: 0x190}
    wrappers = {TEXT + o: "w%X" % o for o in WD}  # the callees are not donor functions here: nothing to compare
    r = match_functions(image(CDONOR), wrappers, CDSTARTS, ctarget(tw), {TEXT + o for o in list(tw) + list(TCALLEES)})
    assert r["matched"] == {}, r
    assert set(r["unmatched"].values()) == {"ambiguous"}, r


def test_callee_anchor_conflict_stays_ambiguous():
    tw = {0x08: 0x150, 0x30: 0x150, 0x58: 0x190}  # two target wrappers jump to X'
    r = match_functions(image(CDONOR), CNAMES, CDSTARTS, ctarget(tw), {TEXT + o for o in list(tw) + list(TCALLEES)})
    assert r["unmatched"].get(TEXT + 0x00) == "ambiguous", r
    assert r["matched"][TEXT + 0x20] == TEXT + 0x58, r


def test_callee_chain_settles():
    # donor: W0 -> V0 -> X, W1 -> V1 -> Y (W and V are look-alike wrappers)
    donor = place({0x00: wrap(TEXT, TEXT + 0x60), 0x20: wrap(TEXT + 0x20, TEXT + 0x80),
                   0x60: wrap(TEXT + 0x60, TEXT + 0x100), 0x80: wrap(TEXT + 0x80, TEXT + 0x140),
                   0x100: body(1), 0x140: body(2)})
    dstarts = {TEXT + o for o in (0x00, 0x20, 0x60, 0x80, 0x100, 0x140)}
    names = {a: "f" for a in dstarts}
    target = place({0x08: wrap(TEXT + 0x08, TEXT + 0x98), 0x38: wrap(TEXT + 0x38, TEXT + 0x70),
                    0x70: wrap(TEXT + 0x70, TEXT + 0x150), 0x98: wrap(TEXT + 0x98, TEXT + 0x190),
                    0x150: body(1), 0x190: body(2)})
    tstarts = {TEXT + o for o in (0x08, 0x38, 0x70, 0x98, 0x150, 0x190)}
    r = match_functions(image(donor), names, dstarts, image(target), tstarts)
    assert r["matched"][TEXT + 0x60] == TEXT + 0x70, r
    assert r["matched"][TEXT + 0x80] == TEXT + 0x98, r
    assert r["matched"][TEXT + 0x00] == TEXT + 0x38, r
    assert r["matched"][TEXT + 0x20] == TEXT + 0x08, r


def test_unique_match_rejected_on_callee_conflict():
    # DirectSoundCreateStream in the donor, DirectSoundCreateBuffer in the target: the same bytes, another callee.
    # The donor's callee X is matched elsewhere, so the target wrapper (calling Y') is not the donor's function.
    donor = place({0x00: wrap(TEXT, TEXT + 0x100), 0x100: body(1), 0x140: body(2)})
    target = place({0x30: wrap(TEXT + 0x30, TEXT + 0x190), 0x150: body(1), 0x190: body(2)})
    r = match_functions(image(donor), {TEXT: "CreateStream", TEXT + 0x100: "X", TEXT + 0x140: "Y"},
                        {TEXT, TEXT + 0x100, TEXT + 0x140}, image(target), {TEXT + 0x30, TEXT + 0x150, TEXT + 0x190})
    assert TEXT not in r["matched"], r
    assert r["unmatched"][TEXT] == "callee-conflict", r


def test_unique_match_rejected_when_callee_belongs_to_another():
    # the donor wrapper calls X, which the target lacks; the target wrapper calls Y', the match of the donor's Y
    donor = place({0x00: wrap(TEXT, TEXT + 0x100), 0x100: body(1), 0x140: body(2)})
    target = place({0x30: wrap(TEXT + 0x30, TEXT + 0x190), 0x190: body(2)})
    r = match_functions(image(donor), {TEXT: "CreateStream", TEXT + 0x100: "X", TEXT + 0x140: "Y"},
                        {TEXT, TEXT + 0x100, TEXT + 0x140}, image(target), {TEXT + 0x30, TEXT + 0x190})
    assert r["matched"] == {TEXT + 0x140: TEXT + 0x190}, r
    assert r["unmatched"][TEXT] == "callee-conflict", r


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
