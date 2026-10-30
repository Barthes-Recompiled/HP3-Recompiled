"""
Find a donor title's statically linked library functions in another title.

XDK library code is the same bytes from title to title except for what the linker
filled in: absolute addresses (globals, other functions, jump tables) and rel32
branch displacements. Each donor function, from its start to the next function
start with the int3/nop padding dropped, becomes a pattern in which those four-byte
fields are wildcards; the pattern must match the whole extent at a function start
of the target, and do so once.

tools/region/xbe_align.py does the same for two regional builds of one title,
where a branch's target can also be checked through the address map. Across titles
there is no map, so the branch bytes are wildcards here.
"""
import bisect
import re
import struct

import capstone

PAD = b"\xCC\x90"


def _extent(starts, a, limit):
    """End of the function at a: the next start, at most a + limit."""
    nxt = [s for s in starts if s > a]
    return min(min(nxt) if nxt else a + limit, a + limit)


def pattern(img, a, end):
    """Regex for the function bytes [a, end): image pointers and rel32 fields wild."""
    raw = img.masked_at(a, end - a, raw=True)
    if not raw:
        return None
    raw = raw.rstrip(PAD)
    wild = bytearray(len(raw))
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    off = 0
    for ins in md.disasm(raw, a):
        off = ins.address - a + ins.size
        o = ins.address - a
        b = ins.bytes
        rel = (b[0] in (0xE8, 0xE9) and ins.size == 5) or (b[0] == 0x0F and 0x80 <= b[1] <= 0x8F and ins.size == 6)
        if rel:
            wild[o + ins.size - 4:o + ins.size] = b"\1\1\1\1"
            continue
        for i in range(ins.size - 3):
            if img.lo <= struct.unpack_from("<I", b, i)[0] < img.hi:
                wild[o + i:o + i + 4] = b"\1\1\1\1"
    raw = raw[:off]  # a trailing partial instruction is not part of the function
    out = b"".join(b"." if wild[i] else re.escape(raw[i:i + 1]) for i in range(len(raw)))
    return (re.compile(out, re.S), len(raw)) if raw else None


def _hits(target, pat, tstarts):
    """Where pat matches a whole function: followed by padding, a function start or the section end."""
    rx, n = pat
    found = []
    for s in target.sections:
        if not s.flags & 0x4:
            continue
        for m in re.finditer(b"(?=" + rx.pattern + b")", s.bytes, re.S):
            e = m.start() + n
            if e >= len(s.bytes) or s.bytes[e] in PAD or s.va + e in tstarts:
                found.append(s.va + m.start())
    return found


def _anchor(d, cands, matched):
    """The one candidate at the same distance from a matched neighbour as in the donor, else None."""
    done = sorted(matched)
    i = bisect.bisect_left(done, d)
    deltas = {matched[n] - n for n in done[max(0, i - 1):i + 1]}
    taken = set(matched.values())
    fit = [c for c in cands if c - d in deltas and c not in taken]
    return fit[0] if len(fit) == 1 else None


def _branches(img, a, n):
    """Targets of the rel32 calls and jumps in the n bytes at a, in order."""
    raw = img.masked_at(a, n, raw=True) or b""
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    out = []
    for ins in md.disasm(raw, a):
        b = ins.bytes
        if (b[0] in (0xE8, 0xE9) and ins.size == 5) or (b[0] == 0x0F and 0x80 <= b[1] <= 0x8F and ins.size == 6):
            out.append(ins.address + ins.size + struct.unpack_from("<i", b, ins.size - 4)[0])
    return out


def _compared(dbr, cbr, matched):
    """(mapped donor target, candidate target) pairs; None when the branch counts differ."""
    if len(cbr) != len(dbr):
        return None
    return [(matched[d], t) for d, t in zip(dbr, cbr) if d in matched]


def _consistent(dbr, cbr, matched):
    """No branch target of the candidate contradicts a donor target already mapped, and none is the match of a
    different donor function (a target function is the image of one donor function at most)."""
    pairs = _compared(dbr, cbr, matched)
    if pairs is None or not all(m == t for m, t in pairs):
        return False
    owner = {t: d for d, t in matched.items()}
    return all(owner.get(t, d) == d for d, t in zip(dbr, cbr))


def _by_callees(dbr, cands, cbr, matched):
    """The one candidate whose branch targets agree with the donor's mapped ones (at least one compared), else None."""
    taken = set(matched.values())
    fit = [c for c in cands if c not in taken and _consistent(dbr, cbr[c], matched) and _compared(dbr, cbr[c], matched)]
    return fit[0] if len(fit) == 1 else None


def match_functions(donor, donor_addrs, donor_starts, target, target_starts, limit=0x4000):
    """{"matched": {donor_addr: target_addr}, "unmatched": {donor_addr: reason}, "anchored": {donor_addr, ...}}.

    reason: "no-match" (the whole body is nowhere), "ambiguous" (at more than one
    function start), "not-function-start" (found, but only where no function starts),
    "callee-conflict" (found, but it calls something other than its donor's mapped callees).

    A body found at several function starts is resolved when exactly one of them is
    as far from a matched neighbour (the nearest matched donor function before or
    after it) as it is in the donor: library code keeps its order and spacing within
    an object. Those are listed in "anchored"; they can anchor further ones.

    A body still found at several starts (look-alike wrappers that differ only in
    what they call) is resolved when exactly one candidate's rel32 call/jump targets
    agree with the donor's, mapped through the matches so far, with at least one
    compared. Those are listed in "callee". Neither rule takes a candidate whose
    calls contradict a mapped callee. Both repeat until nothing changes. Last, any
    match (unique ones too) whose calls contradict its donor's mapped callees is
    dropped as "callee-conflict".
    """
    starts = sorted(set(donor_starts) | set(donor_addrs))
    tstarts = set(target_starts)
    matched, unmatched, ambiguous, lengths = {}, {}, {}, {}
    for a in sorted(donor_addrs):
        pat = pattern(donor, a, _extent(starts, a, limit))
        lengths[a] = pat[1] if pat else 0
        hits = _hits(target, pat, tstarts) if pat else []
        at = [h for h in hits if h in tstarts]
        if len(at) == 1:
            matched[a] = at[0]
        elif at:
            ambiguous[a] = at
        else:
            unmatched[a] = "not-function-start" if hits else "no-match"
    # byte-identical donor functions found once in the target: nothing says which donor it is
    owners = {}
    for a, t in matched.items():
        owners.setdefault(t, []).append(a)
    for t, ds in owners.items():
        if len(ds) > 1:
            for a in ds:
                del matched[a]
                unmatched[a] = "ambiguous"
    anchored, callee = set(), set()
    branches = {}  # (image, address) -> rel32 targets, for the callee test
    for a, cands in ambiguous.items():
        n = lengths[a]
        branches[a] = _branches(donor, a, n)
        for c in cands:
            branches.setdefault(("t", c), _branches(target, c, n))
    progress = True
    while progress:
        progress = False
        # callees first: a matched callee is stronger evidence than a neighbour's distance
        for a in sorted(set(ambiguous) - anchored - callee):
            c = _by_callees(branches[a], ambiguous[a], {t: branches[("t", t)] for t in ambiguous[a]}, matched)
            if c is not None:
                matched[a] = c
                callee.add(a)
                progress = True
        for a in sorted(set(ambiguous) - anchored - callee):
            # a candidate whose calls contradict what is already matched cannot be the same function
            ok = [t for t in ambiguous[a] if _consistent(branches[a], branches[("t", t)], matched)]
            c = _anchor(a, ok, matched)
            if c is not None:
                matched[a] = c
                anchored.add(a)
                progress = True
    for a in ambiguous:
        if a not in anchored and a not in callee:
            unmatched[a] = "ambiguous"
    # every match, unique ones too: a function whose calls go elsewhere than its donor's mapped callees is a look-alike
    # (DirectSoundCreateStream vs DirectSoundCreateBuffer are the same bytes calling different methods)
    dropped = True
    while dropped:
        dropped = False
        for a in sorted(matched):
            t = matched[a]
            dbr = branches[a] if a in branches else _branches(donor, a, lengths[a])
            tbr = branches.get(("t", t)) or _branches(target, t, lengths[a])
            if not _consistent(dbr, tbr, matched):
                del matched[a]
                anchored.discard(a)
                callee.discard(a)
                unmatched[a] = "callee-conflict"
                dropped = True
    return {"matched": matched, "unmatched": unmatched, "anchored": anchored, "callee": callee}


def data_map(donor, matched, donor_starts, target, limit=0x4000):
    """{donor value: {target value: votes}} for the image addresses matched functions use.

    A matched pair is the same instructions, so an absolute address at an offset in the
    donor function (a global, a table, another function) is at the same offset in the
    target's copy: each occurrence is one vote for that pairing.
    """
    starts = sorted(set(donor_starts) | set(matched))
    votes = {}
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    for d, t in matched.items():
        end = _extent(starts, d, limit)
        raw = donor.masked_at(d, end - d, raw=True)
        traw = target.masked_at(t, end - d, raw=True)
        if not raw or not traw:
            continue
        for ins in md.disasm(raw.rstrip(PAD), d):
            o, b = ins.address - d, ins.bytes
            rel = (b[0] in (0xE8, 0xE9) and ins.size == 5) or (b[0] == 0x0F and 0x80 <= b[1] <= 0x8F and ins.size == 6)
            if rel:
                continue
            for i in range(ins.size - 3):
                v = struct.unpack_from("<I", b, i)[0]
                if donor.lo <= v < donor.hi and o + i + 4 <= len(traw):
                    tv = struct.unpack_from("<I", traw, o + i)[0]
                    if target.lo <= tv < target.hi:
                        votes.setdefault(v, {})
                        votes[v][tv] = votes[v].get(tv, 0) + 1
    return votes
