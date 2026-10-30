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
- unmatched: `no-match`, `ambiguous` or `not-function-start`. Nothing is guessed: a wrong address bound to an
  override silently replaces an unrelated function.

## Checked against a known answer

hp1's two regional XBEs (PS EUR `6014a9e9...` -> PS USA `82ecf164...`) with the names in hp1's `hle_d3d8.cpp`,
`hle_dsound.c`, `hle_input.c`, `hle_video.c`, `hle_xapi.c`, `hle_skip.c`: 154 donor functions, 153 matched (116
unique, 37 anchored), every one equal to hp1's verified region map (`port/region/ps-usa.json`); 1 unmatched
(D3DDevice_Clear, no-match). 2026-10-30.

Across titles there is no such answer to compare with; matches there are evidence, and the overrides they move are
checked by running the game.

The map holds addresses, names and counts only. Keep it out of the repository (`tools/xdk_port/output/` is ignored).

Tests: `py -3 tools/xdk_port/test_match.py`.
