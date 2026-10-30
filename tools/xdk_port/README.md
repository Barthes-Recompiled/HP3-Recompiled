# tools/xdk_port

Finds a donor title's statically linked library (XDK) functions in another title, so hand-written overrides written
against the donor's addresses (`HLE(ADDR, n)`, `sub_ADDR`) can be moved to the target's. hp3_recomp uses it to carry
hp1's D3D8 / DSOUND / XAPI overrides (XDK 5659) to Prisoner of Azkaban (XDK 5849).

```
py -3 -m tools.xdk_port --donor <donor.xbe> --donor-functions <donor functions.json> \
    --donor-names <override sources or {"0xADDR": "name"} json>... \
    --target game/default.xbe --target-functions tools/disasm/output/functions.json \
    -o tools/xdk_port/output/map.json
```

The `functions.json` files are `tools.disasm` output for each XBE (`-o` picks the folder).

## How it matches

Library code is the same bytes from title to title except for what the linker filled in. Each donor function, from
its start to the next function start with int3/nop padding dropped, becomes a pattern in which every 32-bit value
pointing into the image and every rel32 branch displacement is a wildcard. A match must cover the whole pattern at a
target function start and be followed by padding, another function start or the section end (so a longer function
that merely begins the same way does not count).

- **unique**: the pattern matches at exactly one function start.
- **anchored**: it matches at several, and exactly one of them is as far from a matched neighbour (the nearest
  matched donor function before or after) as in the donor, and no other donor function has it. Library code keeps
  its order and spacing within an object file. Anchored matches can anchor further ones.
- **callee**: it still matches at several starts (look-alike wrappers that differ only in what they call), and
  exactly one candidate's rel32 call/jump targets agree with the donor's, mapped through the matches so far (at
  least one compared). Tried before neighbour anchoring in each round; neither rule takes a candidate whose calls
  contradict a mapped callee. For this to work the donor names must include the callees: use a whole-library run
  (every donor function in D3D, D3DX, XGRPH, DSOUND, XPP as `{"0xADDR": ""}`, named ones merged in).
- unmatched: `no-match`, `ambiguous` or `not-function-start`. Nothing is guessed: a wrong address bound to an
  override silently replaces an unrelated function.

## Checked against a known answer

hp1's two regional XBEs (PS EUR `6014a9e9...` -> PS USA `82ecf164...`) with the names in hp1's `hle_d3d8.cpp`,
`hle_dsound.c`, `hle_input.c`, `hle_video.c`, `hle_xapi.c`, `hle_skip.c`: 154 donor functions, 153 matched (116
unique, 37 anchored), every one equal to hp1's verified region map (`port/region/ps-usa.json`); 1 unmatched
(D3DDevice_Clear, no-match). 2026-10-30. Rerun with callee matching (hp1's sources had grown to 190 names): 184
matched (144 unique, 39 anchored, 1 callee: D3DResource_BlockUntilNotBusy), every one equal to the region map; 6
unmatched (Clear and five game functions).

Across titles there is no such answer to compare with; matches there are evidence, and the overrides they move are
checked by running the game.

The map holds addresses, names and counts only. Keep it out of the repository (`tools/xdk_port/output/` is ignored).

Tests: `py -3 tools/xdk_port/test_match.py`.

## Second source: XbSymbolDatabase signatures (`oovpa.py`)

```
py -3 -m tools.xdk_port.oovpa --db <XbSymbolDatabase>/src/OOVPADatabase --xbe game/default.xbe \
    --functions tools/disasm/output/functions.json --libs D3D8:5849,DSOUND:5849,XAPILIB:5849 \
    --known tools/xdk_port/output/lib2_hp1_to_poa.json -o tools/xdk_port/output/oovpa_poa.json
```

Reads a local clone of [XbSymbolDatabase](https://github.com/Cxbx-Reloaded/XbSymbolDatabase) (MIT; checked with
commit `20eced544726f5558c5a408458f38a086cc4e543`, 2025-08-23), kept outside this repository (e.g.
`DecompProject\out\XbSymbolDatabase`); nothing of it is copied here. Rules in the module docstring: highest version
not above the library's, unique hit at a function start of the library's section, XREF entries checked once their
target is known (else 8 byte pairs needed), repeated until nothing new is found.

Checked against a known answer: hp1's PS EUR XBE (XDK 5659) finds D3D8 102, DSOUND 118, XAPILIB 32 symbols; the 46
that hp1's overrides also name are all at hp1's addresses. PoA (5849): D3D8 104, DSOUND 72, XAPILIB 37; of the
symbols both this and the whole-library body match name, 73 agree and none differ (2026-10-30).
