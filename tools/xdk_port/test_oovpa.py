"""
Self-check for the XbSymbolDatabase signature scan.

Run: py -3 tools/xdk_port/test_oovpa.py

Signature text inline, a tiny image in memory. Pinned: both source styles parse; the version used is the highest
one not above the library's; a hit must be unique among function starts; an XREF entry is checked when its target is
known, and a signature with an unknown XREF needs 8 byte pairs of its own.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))

from tools.xdk_port.oovpa import parse_oovpa, pick_versions, scan  # noqa: E402
from tools.xdk_port.test_match import TEXT, image, place  # noqa: E402

NEW_STYLE = """
// * IDirectSoundBuffer_SetFrequency
OOVPA_SIG_HEADER_XREF(IDirectSoundBuffer_SetFrequency,
                      4039,
                      XRefOne)
OOVPA_SIG_MATCH(
    XREF_ENTRY(0x15, XREF_CDirectSoundBuffer_SetFrequency),
    { 0x04, 0xFF },
    OV_MATCH(0x0A, 0x83, 0xC0, 0xE4),
    //
);
OOVPA_SIG_HEADER_NO_XREF(D3DDevice_Clear,
                         5849)
OOVPA_SIG_MATCH(
    { 0x00, 0x55 },
);
OOVPA_SIG_HEADER_NO_XREF(D3DDevice_Nothing, 5849)
OOVPA_SIG_MATCH_DUMMY();
"""

OLD_STYLE = """
OOVPA_XREF(CDirectSound_DoWork,
           4039,
           1 + 9,
           XRefOne)
    {
        // CDirectSound_DoWork+0x18 : call [CMcpxAPU::ServiceDeferredCommandsLow]
        XREF_ENTRY(0x19, XREF_CMcpxAPU_ServiceDeferredCommandsLow),
        { 0x00, 0x56 },
        { 0x01, 0x8B },
    }
OOVPA_END;
OOVPA_NO_XREF(XapiFoo, 5849, 2)
    {
        { 0x00, 0x6A }, { 0x01, 0x00 },
    }
OOVPA_END;
"""


def test_oovpa_parse_both_styles():
    sigs = {s.name: s for s in parse_oovpa(NEW_STYLE + OLD_STYLE)}
    f = sigs["IDirectSoundBuffer_SetFrequency"]
    assert f.version == 4039, f
    assert f.pairs == [(0x04, 0xFF), (0x0A, 0x83), (0x0B, 0xC0), (0x0C, 0xE4)], f
    assert f.xrefs == [(0x15, "CDirectSoundBuffer_SetFrequency")], f
    assert sigs["D3DDevice_Clear"].pairs == [(0, 0x55)]
    assert "D3DDevice_Nothing" not in sigs  # a dummy has nothing to match
    d = sigs["CDirectSound_DoWork"]
    assert (d.version, d.pairs, d.xrefs) == (4039, [(0, 0x56), (1, 0x8B)], [(0x19, "CMcpxAPU_ServiceDeferredCommandsLow")])
    assert sigs["XapiFoo"].pairs == [(0, 0x6A), (1, 0)]


def sig_text(name, version, pairs, xrefs=()):
    body = ",\n".join(["XREF_ENTRY(0x%X, XREF_%s)" % x for x in xrefs] + ["{ 0x%02X, 0x%02X }" % p for p in pairs])
    return "OOVPA_SIG_HEADER_XREF(%s, %d, XRefOne)\nOOVPA_SIG_MATCH(\n%s,\n);\n" % (name, version, body)


def test_oovpa_highest_version_not_above():
    sigs = parse_oovpa(sig_text("F", 4039, [(0, 1)]) + sig_text("F", 5558, [(0, 2)]) + sig_text("F", 5933, [(0, 3)]))
    picked = pick_versions(sigs, 5849)
    assert [(s.name, s.version) for s in picked] == [("F", 5558)], picked


EIGHT = bytes([0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x10, 0x53, 0x56])  # push ebp; mov ebp,esp; sub esp,16; push ebx; push esi
PAIRS8 = list(enumerate(EIGHT))


def scan_one(code_at, starts, sigs, known=None):
    img = image(place(code_at))
    return scan(img, [(".text", TEXT, TEXT + 0x200)], {TEXT + s for s in starts}, sigs, known or {})


def test_oovpa_unique_hit():
    hits, unmatched = scan_one({0x20: EIGHT + b"\xC3"}, {0x20}, parse_oovpa(sig_text("G", 5849, PAIRS8)))
    assert {a: s.name for a, s in hits.items()} == {TEXT + 0x20: "G"}, hits
    assert unmatched == [], unmatched


def test_oovpa_duplicate_rejected():
    hits, unmatched = scan_one({0x20: EIGHT + b"\xC3", 0x80: EIGHT + b"\xC3"}, {0x20, 0x80},
                               parse_oovpa(sig_text("G", 5849, PAIRS8)))
    assert hits == {}, hits
    assert unmatched == ["G"], unmatched


def test_oovpa_hit_only_at_function_start():
    hits, _ = scan_one({0x20: EIGHT + b"\xC3"}, {0x10}, parse_oovpa(sig_text("G", 5849, PAIRS8)))
    assert hits == {}, hits


def call_to(at, target):
    """mov eax,[esp+4]; call target; ret 4 (the call's rel32 at +5)"""
    return b"\x8B\x44\x24\x04\xE8" + struct.pack("<i", target - (at + 9)) + b"\xC2\x04\x00"


def test_oovpa_xref_checked_when_known():
    # two look-alike wrappers; the XREF says which one calls CFoo
    code = {0x20: call_to(TEXT + 0x20, TEXT + 0x100), 0x40: call_to(TEXT + 0x40, TEXT + 0x140),
            0x100: b"\xC3", 0x140: b"\xC3"}
    sigs = parse_oovpa(sig_text("IFoo", 5849, [(0, 0x8B), (4, 0xE8)], xrefs=[(5, "CFoo")]))
    hits, _ = scan_one(code, {0x20, 0x40, 0x100, 0x140}, sigs, known={"CFoo": TEXT + 0x140})
    assert {a: s.name for a, s in hits.items()} == {TEXT + 0x40: "IFoo"}, hits


def test_oovpa_xref_found_in_same_scan():
    # CFoo has its own signature; IFoo's XREF to it is checked in a later pass
    code = {0x20: call_to(TEXT + 0x20, TEXT + 0x100), 0x40: call_to(TEXT + 0x40, TEXT + 0x140),
            0x100: b"\xC3", 0x140: EIGHT + b"\xC3"}
    sigs = parse_oovpa(sig_text("IFoo", 5849, [(0, 0x8B), (4, 0xE8)], xrefs=[(5, "CFoo")]) + sig_text("CFoo", 5849, PAIRS8))
    hits, _ = scan_one(code, {0x20, 0x40, 0x100, 0x140}, sigs)
    assert {a: s.name for a, s in hits.items()} == {TEXT + 0x40: "IFoo", TEXT + 0x140: "CFoo"}, hits


def test_oovpa_xref_unknown_needs_8_pairs():
    code = {0x20: call_to(TEXT + 0x20, TEXT + 0x100), 0x100: b"\xC3"}
    few = parse_oovpa(sig_text("IFoo", 5849, [(0, 0x8B), (4, 0xE8)], xrefs=[(5, "CUnknown")]))
    hits, unmatched = scan_one(code, {0x20, 0x100}, few)
    assert hits == {} and unmatched == ["IFoo"], (hits, unmatched)
    many = parse_oovpa(sig_text("G", 5849, PAIRS8, xrefs=[(9, "CUnknown")]))
    hits, _ = scan_one({0x20: EIGHT + b"\xC3"}, {0x20}, many)
    assert {a: s.name for a, s in hits.items()} == {TEXT + 0x20: "G"}, hits


def test_oovpa_ov_match_xref():
    t = """OOVPA_SIG_HEADER_XREF(XapiBar, 4831, XRefOne)
OOVPA_SIG_MATCH(
    OV_MATCH(0x0C, XREF_KT_FUNC_KeRaiseIrqlToDpcLevel),
    OV_MATCH(0x00, 0x6A /* push */, 0x01),
);
"""
    (s,) = parse_oovpa(t)
    assert s.xrefs == [(0x0C, "KT_FUNC_KeRaiseIrqlToDpcLevel")] and s.pairs == [(0, 0x6A), (1, 1)], s


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
