# PoA European address audit

Canonical USA SHA-1: `51b09e01169fda86f2cb7e5ce45ee80a10268918`.
European SHA-1: `c8841e3c1d2d5ec20dbe286e5bec9bb5104b41f9`.

`xbe_align.py map` (Python 3, Capstone 5.0.7) produces **19 segments**:
12 high-confidence byte segments and 7 low-confidence BSS extensions. All deltas
are zero; **55,151 / 55,151 pointers verify**. The entry is `0x111DB` in both.
The `.XTLID` range `0x3D6660..0x3D6B78` is reported unmapped on both sides;
no hand-written code uses it.

Independent comparison of all 13 sections confirms identical virtual addresses,
virtual sizes, raw sizes and stored section bytes, including `.XTLID`. The two
whole files have equal lengths and differ in 534 header bytes. BSS has no stored
bytes: its identity mappings below are justified by the identical referencing
instructions and section layouts, rather than by comparing zero-filled memory.

Reproduction (keep map/reports in ignored local output):

```powershell
py -3 tools/region/xbe_align.py map --from ../hp3_recomp/game/default.xbe --to game/default.xbe -o tools/xdk_port/output/region/poa-eur-map.json
py -3 tools/region/xbe_align.py profile --map tools/xdk_port/output/region/poa-eur-map.json --name poa-eur --explicit tools/xdk_port/output/region/explicit.json -o port/region/poa-eur.json
$files = Get-ChildItem port/src -Recurse -File | Where-Object { $_.Extension -in '.c','.cpp','.h' -and $_.FullName -notmatch '\\recomp\\(gen|region)\\' } | ForEach-Object { $_.FullName }
py -3 tools/region/xbe_align.py addrs --map tools/xdk_port/output/region/poa-eur-map.json --from ../hp3_recomp/game/default.xbe --to game/default.xbe @files port/icall_seeds.json port/tools/patch_gen.py
```

For profile regeneration, extract `explicit.json` from the checked-in profile's
`explicit` list first. The initial source audit found 300 occurrences / 235 unique
values: 217 high, 17 ambiguous and one unmapped. The scanner includes comments
and numbers. The final scan, including the self-check, has 305 occurrences / 237 unique
values: 219 high, the same 17 ambiguous, and the same one unmapped.

Every initial ambiguous/unmapped value is accounted for below. Disassembly of
the references was compared at the same addresses in **both** XBEs; it matches
exactly. Only the active addresses that need overriding get explicit entries.

| Value (identity mapping) | Use and disposition | Matching reference in both XBEs |
|---|---|---|
| `0x275B48` | Dirty flags; explicit BSS mapping | Read at `0x2D168` |
| `0x275B50` | Texture state; explicit BSS mapping | Indexed write at `0x2D16F` |
| `0x275D50` | Render state; explicit BSS mapping | Indexed write at `0x2D234` |
| `0x275FE8` | Device pointer; explicit BSS mapping | CreateDevice store at `0x264E13` |
| `0x275FF0` | Static CDevice; explicit BSS mapping | CreateDevice address load at `0x264E09` |
| `0x278490` | Stream array; explicit BSS mapping | Indexed read at `0x266D00` |
| `0x38A2F8` | APT set-variable callback; explicit BSS mapping | Indirect call at `0x2031A` |
| `0x38BA6C` | Game clock; explicit BSS mapping | Initialization at `0x1A7572`, getter at `0x1A75AF` |
| `0x264C64` | Only unmapped source value: exclusive `.text` end used by the return-address scan; explicit bound | Both section headers: start `0x11000`, size `0x253C64`; the endpoint has no instructions |
| `0x376B44` | Debug chapter; comment only, no explicit needed | Write at `0x1B7DD`, read at `0x21B6A` |
| `0x376B48` | Debug room; comment only | Write at `0x1B7E3`, read at `0x21B71` |
| `0x376B4C` | Debug start-point name; comment only | Write at `0x1B7E8`, read at `0x21B64` |
| `0x30F948` | Renderer singleton; comment only | Read at `0x1DC56` |
| `0x3603A8` | View-projection matrix copy; comment only | Address load at `0x51ED3` |
| `0x3CB6D4` | XAPI state; comment in disabled override, no live use | Store at `0x117FB` |
| `0x275F90` | Immediate-mode state; comment only | Store at `0x26619C` |
| `0x275F94` | Immediate-mode state; comment only | Compare at `0x2661D3` |
| `0x382010` | Active DirectSound flag mask, already classified as a number | `default_headroom` tests buffer flag bits; no address mapping applies |

New `retarget.json` classifications (all addresses; **no new numbers**):

| Value | Files | Reason |
|---|---|---|
| `0x266100` | `src/difftest_hle.h` | SetRenderState ZEnable guest HLE entry in the Unicorn stop/delegation table |
| `0x268400` | `src/difftest_hle.h`, `src/scda/xdk/hle_d3d8.cpp` | SetTexture HLE entry; also passed to the guest-call helper for Swap BYPASSCOPY |
| `0x268700` | `src/difftest_hle.h` | SetTileNoWait guest HLE entry in the Unicorn stop/delegation table |
| `0x26C800` | `src/difftest_hle.h` | BeginStateBig guest HLE entry in the Unicorn stop/delegation table |
| `0x270D00` | `src/difftest_hle.h` | SetPixelShader guest HLE entry in the Unicorn stop/delegation table |

All five function addresses lie in byte-identical D3D code; regen's function-start
check validates the override entry points. Retargeting needs zero source copies,
but must retain an empty `region.cmake` stamp to select `poa-eur` in CMake. The
canonical profile still uses the original sources and removes an old stamp.

The region self-check runs at `CGameChapter::Start` (`0x115500`) final join
`0x115678`, after its load and character state restoration calls. The stack buffer at
`ebp-0x40` still holds the PLFS chapter path formatted at `0x115534` (no later
writes in this function). It skips `frontend*.pkg`, then logs once, checking the
clock's tick count, elapsed milliseconds, divisor, delta and floor, plus the
static D3D device pointer. See `docs/bringup.md`, “60 fps”, for the clock layout.
It reads on the guest thread without changing guest state. USA uses the same
hook with profile name `poa-usa`; all address mappings remain canonical.

Runtime behavior and the first `[REGION] poa-eur: ... -> ok` remain for the smoke
test. This task does not launch the game.

## Validation (2026-10-31)

- MSYS2 UCRT64 `bash port/regen.sh`: passed, 20,818 translated functions,
  all hand-written function/data references accepted, D3D/DSOUND zero HLE gaps.
- CMake Release / Ninja, `cmake --build build-win -j4`: passed; produced
  `hppoa.exe`, `hppoa_launcher.exe` and `hppoa_trainer.exe`. Existing toolkit
  compiler warnings remain. TLS check: 60 thread-local variables, no global
  address reads.
- Retarget: EUR accepted with zero rewrites; repeat identity retarget and
  canonical cleanup verified with a synthetic profile. Actual USA SHA-1 still
  selects the canonical source path. The USA profile is unchanged; a full USA
  regeneration/build was not run in this EUR checkout.
- Self-check compiled with `-Wall -Wextra -Werror` and synthetic guest memory:
  frontend skipped, one gameplay line only, valid clock gives `ok`, zero divisor
  gives `SUSPECT`. Reapplying the generated hook is idempotent.
- `git diff --check`: passed. No game launch or push.
