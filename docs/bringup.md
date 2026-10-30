# PoA bring-up notes

What the boot milestone (M2) has to check or finish, collected while moving hp1's overrides to PoA (2026-10-30).
Addresses are PoA USA (`default.xbe` SHA-1 `51b09e01...`) unless they say hp1.

## How the overrides were moved

`tools/xdk_port` matched hp1's library functions (PS EUR XBE, XDK 5659) into PoA (XDK 5849):

- override functions: 151 named in the kept files, 102 matched (90 unique, 10 by a neighbour, +2 from the
  whole-library run), 49 not;
- whole library sections (D3D, D3DX, XGRPH, DSOUND, XPP): 889 of 1053 hp1 functions matched (73 by a neighbour);
- hp2 (CoS EUR, XDK 5028) as a second donor: 73 of 134 matched; 66 PoA functions were found by both donors
  independently, with no conflicting name.

Maps (addresses only, not in git): `tools/xdk_port/output/map_hp1_to_poa.json`, `map_hp2_to_poa.json`,
`lib_hp1_to_poa.json`. `port/tools/check_no_foreign_addrs.py` (run by regen.sh) fails on any name in live code
that is not a PoA function start.

## Bindings to suspect first

Live bindings that do not rest on a unique body match (marked `// xdk_port: ... match` in the code):

| PoA | hp1 | Function | How |
|---|---|---|---|
| 0x00267550 | 0x00192170 | D3DDevice_InsertFence | anchored |
| 0x002662D0, 0x00266340, 0x002663B0, 0x002663D0, 0x002663F0 | 0x00191B60 ... 0x00191C80 | SetRenderState YuvEnable, OcclusionCullEnable, StencilCullEnable, RopZCmpAlwaysRead, RopZRead | anchored |
| 0x0026AA80 | 0x00194CE0 | D3DResource_BlockUntilNotBusy (a jmp to the uniquely matched D3D_BlockOnResource, as in hp1) | whole-library run |
| 0x002B8584 | 0x001A87DC | DirectSound SetI3DL2Listener | anchored |
| 0x002B7FAE | 0x001A7F53 | IDirectSoundBuffer SetBufferData | whole-library run |
| 0x002B75A3 | 0x001A6E26 | IDirectSoundBuffer Play | anchored |
| 0x002B7587 | 0x001A6E0A | IDirectSoundBuffer SetVolume | anchored |
| 0x002C09C2 | 0x001F5038 | XInputClose | anchored |

The final review checked the DirectSound ones byte for byte against hp1's (same shape, same `ret n`, same spacing).

## Globals and code addresses carried by evidence (verify in M2)

Derived from matched function pairs: the same instruction in hp1's and PoA's copy of a function, one vote per
occurrence (`tools.xdk_port.match.data_map` over `tools/xdk_port/output/lib_hp1_to_poa.json`), or by reading the same
table in both images. The vote rows below are printed by a script from that file, not typed (a hand conversion
first put five of them 0x400 too high; the final review caught it). `check_no_foreign_addrs.py` now also fails on any
address literal PoA's code never mentions.

| Name (file) | hp1 | PoA | Evidence |
|---|---|---|---|
| D3D__pDevice (kDevicePtr; main.c fence) | 0x001A13D8 | 0x00275FE8 | 101 votes, no other candidate; PoA code mentions it 112 times |
| static CDevice (kDevice) | 0x001A13E0 | 0x00275FF0 | 3 votes, no other candidate; PoA code mentions it 6 times |
| D3D__DirtyFlags | 0x001A0F38 | 0x00275B48 | 59 votes, no other candidate; PoA code mentions it 479 times |
| D3D__TextureState | 0x001A0F40 | 0x00275B50 | 6 votes, no other candidate; PoA code mentions it 24 times |
| D3D__RenderState | 0x001A1140 | 0x00275D50 | 11 votes, no other candidate; PoA code mentions it 14 times |
| g_Stream | 0x001A3898 | 0x00278490 | 6 votes, no other candidate; PoA code mentions it 6 times |
| D3DSIMPLERENDERSTATEENCODE (kSimpleEncode) | 0x001FB2B0 | 0x002C7AD8 | 1 votes, no other candidate; PoA code mentions it 2 times |
| XPP device type table begin | 0x001F4204 | 0x002BF924 | 2 votes, no other candidate; PoA code mentions it 4 times |
| XPP device type table end | 0x001F420C | 0x002BF92C | 2 votes, no other candidate; PoA code mentions it 3 times |
| D3D_DestroyResource | 0x00194A70 | 0x0026A810 | matched function |
| default state init (kInitDefaultState) | 0x00196D50 | 0x0026B180 | matched function |
| g_DeviceType_Gamepad | 0x001F422C | 0x002BF9F4 | the gamepad's XPP_DEVICE_TYPE_INFO (+4) in both tables; same layout; the title pushes it as the gamepad type |
| entry point (main.c) | 0x00154331 | 0x000111DB | the XBE header |
| title .text end (hle_d3d8.cpp stack scan) | 0x00190714 | 0x00264C64 | the XBE section table |

Spacing is the same in both titles: pDevice to CDevice 8, DirtyFlags to TextureState 8, TextureState to RenderState
0x200, CDevice to g_Stream 0x24A0 = 0x928 dwords, the device size Direct3D_CreateDevice clears in both.

**CDevice field offsets:** the final review compared every offset hle_d3d8.cpp uses (+0x2C, +0x30, +0x790, +0x794,
+0x948, +0x95C/+0x960, +0xC60, +0xEE0, +0xEF8, +0xF98, +0x1A04, +0x1A08, +0x1A14, +0x2478) in hp1's and PoA's D3D code:
the same use counts in both (Swap uses +0x2478 five times in each). The device layout looks unchanged; still to be
confirmed by running (M2): vblank count +0x1DE8 and VBlankCallback +0x1DB8 were not in that comparison.

**Not found:** CDirectSoundStream vtables (hle_dsound.c STREAM_VTABLE/2, hp1 0x0020F098 / 0x0020F08C): set to 0.

## Overrides not bound yet

Disabled in place with `#if 0` and the hp1 address, so M2 can rebind them. Route for the rest: XbSymbolDatabase
(Cxbx-Reloaded) for XDK 5849, which hp1 used for its DirectSound addresses, and SCDA-XBERecomp's DirectSound, which
targets 5849; Ghidra / REA on PoA where neither helps.

| hp1 address | function | why | file |
|---|---|---|---|
| 0x00155251 | XAPI disc authentication (from XapiInitProcess) | no-match | hle_xapi.c |
| 0x00191A30 | (unnamed) | no-match | hle_d3d8.cpp |
| 0x00191AC0 | (unnamed) | no-match | hle_d3d8.cpp |
| 0x00192070 | (unnamed) | no-match | hle_d3d8.cpp |
| 0x00192BC0 | D3DDevice_GetRenderTarget2 | no-match | hle_d3d8.cpp |
| 0x001934C0 | D3DDevice_GetDisplayFieldStatus | no-match | hle_video.c |
| 0x00194EC0 | D3DDevice_Clear | no-match | hle_d3d8.cpp |
| 0x001952B0 | D3DDevice_CreatePixelShader | no-match | hle_d3d8.cpp |
| 0x00195500 | D3DDevice_SetPixelShaderConstant | no-match | hle_d3d8.cpp |
| 0x00195B90 | D3DDevice_IsFencePending | no-match | hle_d3d8.cpp |
| 0x0019D910 | D3DDevice_GetOverlayUpdateStatus | no-match | hle_video.c |
| 0x001A5FD6 | (unnamed) | no-match | hle_dsound.c |
| 0x001A5FF5 | (unnamed) | no-match | hle_dsound.c |
| 0x001A6696 | IDirectSoundStream_AddRef | no-match | hle_dsound.c |
| 0x001A66DD | IDirectSoundStream_Release | no-match | hle_dsound.c |
| 0x001A672B | IDirectSoundStream_GetInfo | no-match | hle_dsound.c |
| 0x001A6792 | IDirectSoundStream_Discontinuity | no-match | hle_dsound.c |
| 0x001A67DF | IDirectSoundStream_Flush | no-match | hle_dsound.c |
| 0x001A682A | IDirectSoundStream_GetStatus | no-match | hle_dsound.c |
| 0x001A687B | IDirectSoundStream_Process | no-match | hle_dsound.c |
| 0x001A68F2 | IDirectSoundStream_Pause | no-match | hle_dsound.c |
| 0x001A6943 | IDirectSoundStream_FlushEx | no-match | hle_dsound.c |
| 0x001A6D27 | IDirectSoundStream_SetVolume | no-match | hle_dsound.c |
| 0x001A6D79 | IDirectSoundStream_SetHeadroom | no-match | hle_dsound.c |
| 0x001A6DF2 | (unnamed) | no-match | hle_dsound.c |
| 0x001A6E62 | IDirectSoundBuffer_SetLoopRegion | ambiguous | hle_dsound.c |
| 0x001A6E82 | IDirectSoundBuffer_GetStatus | ambiguous | hle_dsound.c |
| 0x001A6E9E | IDirectSoundBuffer_SetCurrentPosition | ambiguous | hle_dsound.c |
| 0x001A6EBA | (unnamed) | ambiguous | hle_dsound.c |
| 0x001A6EBF | (unnamed) | ambiguous | hle_dsound.c |
| 0x001A6EC4 | (unnamed) | ambiguous | hle_dsound.c |
| 0x001A6EC9 | (unnamed) | no-match | hle_dsound.c |
| 0x001A6EE1 | (unnamed) | no-match | hle_dsound.c |
| 0x001A779D | IDirectSoundStream_SetMixBins | no-match | hle_dsound.c |
| 0x001A77EF | IDirectSoundBuffer_SetFrequency | ambiguous | hle_dsound.c |
| 0x001A780B | IDirectSoundBuffer_SetMixBins | ambiguous | hle_dsound.c |
| 0x001A7827 | IDirectSoundBuffer_SetMaxDistance | no-match | hle_dsound.c |
| 0x001A784B | IDirectSoundBuffer_SetMinDistance | no-match | hle_dsound.c |
| 0x001A786F | IDirectSoundBuffer_SetPosition | no-match | hle_dsound.c |
| 0x001A78A4 | IDirectSoundBuffer_SetVelocity | no-match | hle_dsound.c |
| 0x001A78D9 | IDirectSoundBuffer_SetDistanceFactor | no-match | hle_dsound.c |
| 0x001A78FD | IDirectSoundBuffer_SetDopplerFactor | no-match | hle_dsound.c |
| 0x001A7921 | IDirectSoundBuffer_SetRolloffCurve | ambiguous | hle_dsound.c |
| 0x001A7945 | IDirectSoundBuffer_SetPlayRegion | ambiguous | hle_dsound.c |
| 0x001A7965 | (unnamed) | ambiguous | hle_dsound.c |
| 0x001A7F37 | IDirectSoundBuffer_SetFormat | ambiguous | hle_dsound.c |
| 0x001A844A | (unnamed) | no-match | hle_dsound.c |
| 0x001A8794 | (unnamed) | no-match | hle_dsound.c |
| 0x001A87B8 | (unnamed) | no-match | hle_dsound.c |

Consequences until they are bound:

- **DirectSound HLE is off by default** (`RECOMP_HLE_DSOUND=1` turns it on): with 37 of its entry points unbound, part
  host mixer and part XDK DirectSound would share voices. The XDK's DirectSound runs on the emulated APU instead.
- **D3D HLE stays on**, but Clear, CreatePixelShader, SetPixelShaderConstant, GetRenderTarget2, IsFencePending and
  three unnamed functions run the XDK's own bodies, which write the push buffer the host renderer does not read.
  Clears and pixel shaders are the first things M2 has to bind.
- **hle_video.c's vblank overrides** (GetDisplayFieldStatus, GetOverlayUpdateStatus) and **hle_xapi.c's disc check**
  are unbound.

## Renderer gaps

- hp1's renderer is fixed-function only (Philosopher's Stone has no vertex shaders); hp2's has vertex and pixel
  shader HLE (`hp2_recomp/port/src/renderer/xdk/hle_d3d8.cpp`: SetVertexShader, SetPixelShader,
  SetVertexShaderConstant*, LoadVertexShader, SetShaderConstantMode). PoA calls those (hp2 donor matched them, e.g.
  SetPixelShader 0x00270D00, LoadVertexShader 0x00266D40, SetShaderConstantMode 0x00266E40). The survey below
  confirms PoA draws with a programmable vertex shader: port hp2's vertex shader path in M2.

**Immediate mode (M3 Task 4).** `D3DDevice_Begin` 0x00269B20, `End` 0x00269B60, `SetVertexData2f` 0x00269A80 and
`SetVertexData4f` 0x00269AC0 (5 args, `ret 20`) are bound (`gpu/immediate.h`, tests `port/tests/immediate`): a
register keeps its value like the NV2A's inline attributes, D3DVSDE_VERTEX (0xFFFFFFFF) writes v0 and emits a
vertex, End packs the list in the stream-0 layout (FVF or declaration) for one UP draw. Users (DeepCode read,
spot-checked in recomp_0001.c): an effect object (constructor `sub_0004B190`; 32 slots of 0x2500 bytes at
`this+0x140`; three 32x32 texture wrappers at +0x4A1C8 / +0x4A210 / +0x4A258) drawn by `sub_0004A3C0`, which returns
early while `[0x313A6C] != 0`. `sub_00049A70` (once, guard +0x4A2A4) sets FVF 0x42 through `sub_00267090` and draws
`Begin(6)` (6-vertex gradient strip; register 3 diffuse, then 0xFFFFFFFF) and `Begin(4)` (2-vertex transparent line)
into the +0x4A288 target; `sub_00049F70` (each time the effect renders) sets FVF 0x144 and draws two 4-vertex
strips (diffuse, `SetVertexData2f(9, u, v)` texcoord 0, position) into +0x4A240; the per-slot draw `sub_00043C00`
samples all three textures. Run m3t4b: `immediate mode prim 6, 6 vertices, stride 16 (from 00049DD4)` and `prim 4,
2 vertices, stride 16 (from 00049EF8)`, no `not supported` line, boot unchanged. `check_hle_coverage`: D3D 0 gaps,
DSOUND 0 gaps. The start-up pass draws before the first Swap, so a timed capture can't show its target; the
level-side pass is checked in Task 5.

**Frame rate (M3 Task 6).** PoA's frame timer `sub_001A7450` *clamps* a short frame's delta up to the minimum at
0x38BA84 (1000/30 or 1000/25 ms, set by `sub_00096520` from renderer init) and does not wait; its delta 0x38BA74 has
no consumer found (DeepCode read; `sub_00026A50` measures QPC itself). The real 30 Hz limiter is the presentation
interval: `sub_00034750` creates the device with `0x80000002` (D3DPRESENT_INTERVAL_TWO_OR_IMMEDIATE, the log's
`CreateDevice ... interval 80000002`). The HLE decoded only 2/3/4 and ran it at one vblank, so the game ran at 60 and
its per-frame logic at double speed (the user saw the opening cutscene run fast). Fix: `xdk/present_interval.h`
(XDK ONE 1, TWO 2, THREE 4, FOUR 8, IMMEDIATE 0x80000000 ored in; test `port/tests/present_interval`, the old mapping
fails on 0x80000002). Runs m3t6 / m3t6c / m3t6d: 29.9-30.0 fps. Speed: by eye at the user check (Task 10). A real
60 fps for PoA needs its frame-stepped logic decoupled: its own plan after M3; caps above 30 (`fps_limit` 120+,
unlocked) still bypass the interval and run the game fast.

**Missed functions (M3 Task 5).** `[ICALL] Failed to resolve VA 0x000CBEE0` in the level (m3t3): a method stored
by a static initializer (0x0026023D `mov eax, 0xcbee0`, into the table at 0x2FE200, beside 0xCBE20 / 0xCA090) and
never found by the disassembler. A sweep of code immediates for unknown 16-aligned function-like targets found it
and 0x243390 (after int3 padding, merged into 0x243370's span; a callback a getter returns at 0x244561);
`scan_code_ptrs.py` (data tables) adds 0x777F0 (`push ebp` after a table). All three are in `port/icall_seeds.json`;
0x5E040 / 0x5E050 (start with an int3 byte, never called) are left out. m3t6d: 0 ICALL failures in the level.
`MakeSpace from 00268E2D` (16 per level load) is D3DVertexBuffer_Lock2's vertex-cache invalidate, allowlisted in M2.

**m3t6c / m3t6d (30 fps; Start 65 s, Start 80 s, A 88 / 96 / 104 s).** At 30 fps the first Start skips the intro
movie; the second leaves the title for the "I solemnly swear that I am up to no good" screen (`poa_trc.big`,
`frontendspeech.pkg`, `frontendaudio.pkg`), A gives the main menu (`poa_fe.big`: New Game / Load Game / Bonus
Activities / Options), A on New Game loads the Hogwarts Express. Audio (`RECOMP_DSOUND_WAV`): RMS 0 on the
"Press START" title only; quiet, rising sound from the swear screen on. **In the level, popping (user) = one
repeating burst**: every 3-5 s a one-second window with RMS ~2420, 164 or 284 full-scale samples and 15-16
sample-to-sample jumps over 8000, identical each time: one sound decoded or mixed wrongly (next to check: which
voice/format plays then, `RECOMP_DSOUND_TRACE`).

## Title facts so far

- Frame timing follows the video standard: hp2's toolkit fix for `XC_FACTORY_AV_REGION` notes PoA runs 25 fps on PAL
  and 30 fps on NTSC (taken into hp3, docs/toolkit-sync.md). The 60 fps work starts from there.
- PoA's DSOUND section is 37 KB against hp1's 121 KB, and it has no XMV section (EA's own movie player, `.mad` files).

## Left over from Philosopher's Stone

- Comments that still describe PS code: hle_input.c (stick deadzone, ~line 243), profiler.c (~107),
  scda/app/input.cpp (~281, keyboard movement pace).
- hppoa_trainer.exe's pages (spells, levels, checkpoints) are PS-shaped; the game side ignores them.
- Settings with no PoA hook yet: cutscene_skip (cutscene state not found), intro_skip (movie names), Quit To Desktop
  (menus), 16:9 Hor+ (camera), the menu/loading/text-icon 2D handling.

## Regeneration (M1)

`regen.sh` on the USA XBE: 20,813 of 20,815 functions translated (0 failed), 102 hand-written overrides, 47 unresolved
call targets, 908 unimplemented instructions (32 mnemonics, mostly `outsd`/`outsb`/`insb`/`arpl`/`insd`: data decoded
as code). For scale, hp1's shipped build has 26 and 520 on its own XBE.

Of the 47 unresolved targets, 40 are called only from `gap_prologue` "functions" with no callers (the disassembler's
guesses in gaps between code: data). The other 7 to look at if they come up at run time:

| Caller | Section | Targets |
|---|---|---|
| sub_0028768E (call_target, 2 callers) | D3DX | 0x2879F8, 0x287A08, 0x287A0C, 0x287A18, 0x287A63 |
| sub_0028E898 (prologue, 1 caller) | D3DX | 0x28F042, 0xEA31F039 (outside the image) |
| sub_002B48AE (call_target, no callers) | XGRPH | 0x2B5A67, 0x2B5A68, 0x2B5A6D, 0x2B5A7B, 0x2B5A83 |
| sub_002C7768 (cc_boundary) | XPP | 0x2C77D6, 0x2C77E6 (.rdata) |

## Survey of PoA's code (Codex, read-only, 2026-10-30)

Static evidence (call sites in the title's code), not observed execution.

- **Vertex shaders: yes.** SetVertexShader (0x00267090) from 20 call sites; renderer init `sub_00034750` calls
  `sub_00030850`, which assembles 2,900 bytes of embedded `xvs.1.1` source (0x002CA8C0) with XGRPH's assembler
  (`sub_0029D050`) and creates it (CreateVertexShader 0x00266A90): four-bone skinning, normals, view/projection,
  lighting, texture coordinates; constants 0-47 bones, 48-55 view/projection, uploaded from `sub_0002F420` /
  `sub_0002F520` through SetVertexShaderConstantNotInlineFast (0x00266BD0). Declaration at 0x002FA6B4. **M2 needs
  hp2's vertex shader HLE.** Pixel shaders: no title call found (SetPixelShader is called only inside D3D).
- **Draws:** DrawVerticesUP 0x002694D0 (20 sites), DrawVertices 0x00269780 (3), DrawIndexedVertices 0x00269820 (1),
  DrawIndexedVerticesUP 0x00269640 (1, probable), Begin/End immediate mode 0x00269B20/0x00269B60 (4 each). Push
  buffer APIs: not identified.
- **Other D3D:** CopyRects 0x00267A60 (1), SetPalette 0x002685B0 (10), InsertFence 0x00267550 / BlockOnFence
  0x00267460 (1 each, in `sub_000345B0` with Swap), SetGammaRamp 0x00267230 (1), SetRenderTarget 0x00267720 (15),
  Swap 0x0026A610 (3). No overlay (movies are drawn as YUY2 textures).
- **DirectSound:** 44 call sites into 21 entry points, all from EA's audio layer (TbAudioBody, TbSoundManager,
  TbMapStreamer). `sub_0014EC90` creates DirectSound and downloads an embedded DSP image (0x002DCD28, 0x3360 bytes).
  Bound so far: DirectSoundCreate 0x002B874A, CreateSoundBuffer 0x002B8560, DownloadEffectsImage 0x002B7508,
  SetI3DL2Listener 0x002B8584, Release 0x002B6962/0x002B6978, SetBufferData 0x002B7FAE, Play 0x002B75A3, SetVolume
  0x002B7587. Unnamed and called: 0x002B752F (7), 0x002B754F, 0x002B756B, 0x002B75DF, 0x002B7603, 0x002B761F,
  0x002B763F, 0x002B765B, 0x002B767B, 0x002B7A60, 0x002B7A7C, 0x002B8791. Naming these is the DirectSound part of M2.
- **Threads:** one PsCreateSystemThreadEx site (XAPI CreateThread `sub_00011DD1`), five callers: title main thread
  (start 0x00011167 -> main 0x0001A5F0), a callback dispatcher (0x00147CE0, twice: archive worker 0x0014A650 "BF
  Header", timing worker 0x00222B10, memory-card worker 0x00223A60), audio worker 0x0014E9C0, XAPI timer 0x00247580.
- **Timing:** XGetVideoStandard (`sub_00195024`) is read once at renderer init (0x000349E5); the init sets a minimum
  frame time of 1000/30 or 1000/25 ms through `sub_00096520`; the timer `sub_001A7450` uses it as the minimum elapsed
  delta (0x001A74A0) and caps a delta at 100 ms (0x001A74A9): a variable time step with a 33/40 ms floor. The game
  clock is QPC `sub_00195129` (rdtsc) via `sub_001A7610`, read by the frame timer `sub_00026A50`.
  BlockUntilVerticalBlank is not called by the title directly. **For 60 fps: check what lowering that floor does.**
- **Movies:** `sub_001AC0A0` builds `<Movie>%s.mad` and plays it (`sub_0005DCD0`); frames are converted from
  planar YUV to YUY2 with MMX (`sub_0005D3C0`) and drawn as a textured quad with D3D's YUV path. Six movies:
  `attract`, `eac_usa`, `filmus`, `gallery`, `intro`, `wb` (`eac_usa`, `wb`, `intro` are the start-up skip candidates).
- **Debug menu:** see docs/re-notes.md.

## M2 runs

Test folder `Harry Potter Xbox Decomps\test\PoA_USA` (exes, `game` junction, `hppoa.toml` with `debug_logs = true`,
`debug.env` holding the launcher's `RECOMP_*` defaults from `launcher.cpp` StartGame plus the run's traces).

**Baseline 1 (2026-10-30, m2-boot at 938f9d1, defaults: DirectSound HLE off).** Boots, reads `runtime.cfg`,
`filesys.dat`, `data/packages/plfs/frontend-startup.pkg`, creates the D3D device (640x480, format 6, interval
0x80000002) and opens the window, then crashes about 2 s in: `[APU] MMIO decode fail ... offset=0x2000` and an access
violation reading 0xFE802000. That is the XDK's own DirectSound reading an APU register during DirectSoundCreate:
guest stack 0x002BE040 <- ... <- 0x002B8761 (DSOUND) <- 0x0014ECA5 (`sub_0014EC90`, EA's audio init). The emulated
APU cannot carry PoA's DirectSound, so the HLE is required, not optional (Task 7).

**Baseline 2 (same build, `RECOMP_HLE_DSOUND=1`, the partly bound HLE).** Gets past audio init:
`DirectSoundCreate`, `DownloadEffectsImage(13152 bytes)`, **180 `CreateStream` calls** (PCM, 1 channel, 48 kHz,
1 packet each, same callback 0x00F7FB7C), one 6-channel buffer (SetBufferData + Play). So PoA plays through
DirectSound *streams*, and the stream entry points and vtables (hp1's unbound IDirectSoundStream_* set,
`STREAM_VTABLE` = 0) are on the boot path. Then: `data/gui/inkpot.ttf`, `quorumstd-medium.ttf`, `poa_base.big`,
`poa_overlay.big`, `poa_startup_xbox.big`, the audio packages and `frontend0.sdt` / `gui_stream.sdt`, `ls_text.txt`,
and `data/movie/eac_usa.mad` opens. One `[FRAME] 49.3 fps` line, then no more Swaps for the remaining ~70 s while
the threads keep running (critical sections and waits in the kernel summary): it hangs in the first logo movie. No
captures (they count from the first Swap and need 20 s of frames). First suspect: the `.mad` player paces itself on
audio stream packets the unbound stream functions never complete.

Other lines: `d3d: Begin/End immediate mode is not supported (from 00049DD4)` and `(from 00049EF8)`; hp1/hp2's
renderer has no immediate mode (Begin/End + SetVertexData*). Missing files the console misses too (path joined
without a separator, so not ours): `fontsfonts.ini`, `texturessymbols.xsh`, `variablesthingvars.txt`.
Counts: 0 `MakeSpace from`, 0 `ICALL`, 0 `[CRASH]`, 0 unknown shader handles.

**Static coverage (Task 2, `port/tools/check_hle_coverage.py`, run by regen.sh).** Title-called library functions
the HLE does not replace, at m2-boot be2dd60+:

- DSOUND, 12: 0x002B752F (7 callers), 0x002B754F, 0x002B756B, 0x002B75DF, 0x002B7603, 0x002B761F, 0x002B763F,
  0x002B765B (4), 0x002B767B (3), 0x002B7A60, 0x002B7A7C, 0x002B9760. The Codex survey's unnamed set, except that
  0x002B9760 appears and 0x002B8791 does not (not called directly from `.text`).
- D3D, 7 that reach MakeSpace / MakeRequestedSpace / BeginStateBig: 0x00266190 (9 callers), 0x00266230 (3),
  0x00266BD0 SetVertexShaderConstantNotInlineFast (2), 0x00268E10 (20), 0x00269050 (7), 0x00269640
  DrawIndexedVerticesUP (1), 0x00269AC0 (32). No address-taken library functions.

**Callee matching (Task 3).** Whole-library runs (every donor function in the library sections):
hp1 -> PoA 899 of 1094 (40 anchored, 41 by callees), hp2 (CoS USA) -> PoA 554 of 944 (32 anchored, 25 by callees);
maps `tools/xdk_port/output/lib2_hp{1,2}_to_poa.json`. Five of the "ambiguous" DirectSound buffer wrappers are
now callee matches, each found by both donors at the same PoA address: SetLoopRegion 0x002B761F, GetStatus
0x002B763F, SetCurrentPosition 0x002B767B, SetFrequency 0x002B7A60, SetMixBins 0x002B7A7C. Coverage gaps they
explain: 0x002B9760 = hp1 0x001AA384 (both donors), 0x002B765B = hp2 0x00145337. D3D gaps: 0x00269AC0 (32 callers)
= hp2's **D3DDevice_SetVertexData4f** (unique), so PoA draws a lot in Begin/End immediate mode, which the renderer
lacks; 0x00268E10 = hp1 0x00197BC0 / hp2 0x00132E50 (unique in both, unnamed). The IDirectSoundStream methods and
the other unnamed DirectSound entries are still unmatched.

**XbSymbolDatabase signatures (Task 4, `tools/xdk_port/oovpa.py`, map `tools/xdk_port/output/oovpa_poa.json`).**
104 D3D8, 72 DSOUND, 37 XAPILIB symbols in PoA. Against the body-match maps: 73 names agree, none differ (nine
apparent differences were names misread from source comments: SetTransform, Lock2DSurface, ZBias, ...). Names for
the coverage gaps:

| PoA | Signature name | Body match |
|---|---|---|
| 0x002B752F | IDirectSound_SetMixBinHeadroom | - |
| 0x002B754F | IDirectSoundBuffer_SetEG | - |
| 0x002B756B | IDirectSoundBuffer_SetFilter | - |
| 0x002B75DF | IDirectSoundBuffer_PauseEx | - |
| 0x002B7603 | IDirectSoundBuffer_Pause | - |
| 0x002B761F | IDirectSoundBuffer_SetLoopRegion | callee (hp1 and hp2) |
| 0x002B763F | IDirectSoundBuffer_GetStatus | callee (hp1 and hp2) |
| 0x002B765B | IDirectSoundBuffer_GetCurrentPosition | callee (hp2) |
| 0x002B767B | IDirectSoundBuffer_SetCurrentPosition | callee (hp1 and hp2) |
| 0x002B7A60 | IDirectSoundBuffer_SetFrequency | callee (hp1 and hp2) |
| 0x002B7A7C | IDirectSoundBuffer_SetMixBins | callee (hp1 and hp2) |
| 0x002B9760 | - | hp1 0x001AA384, hp2 (callee / anchored) |
| 0x002B8791 | DirectSoundCreateBuffer | - |
| 0x00269050 | D3DDevice_Clear | - |
| 0x00268E10 | D3DVertexBuffer_Lock2 | unique (hp1 0x00197BC0, hp2 0x00132E50) |
| 0x00266BD0 | D3DDevice_SetVertexShaderConstantNotInlineFast | unique (hp2) |
| 0x00269640 | D3DDevice_DrawIndexedVerticesUP | - |
| 0x00269AC0 | D3DDevice_SetVertexData4f | unique (hp2) |
| 0x00266190, 0x00266230 | - | - |

Also found: D3DDevice_CreateVertexShader 0x00266A90, SetVertexData2f 0x00269A80, Begin 0x00269B20, End 0x00269B60,
XapiInitProcess 0x000117AB, DirectSoundCreate 0x002B874A, IDirectSound_CreateSoundBuffer 0x002B8560. Not found by
either source: CreatePixelShader, SetPixelShaderConstant, GetRenderTarget2, IsFencePending,
GetDisplayFieldStatus, GetOverlayUpdateStatus, SetVertexShaderConstant1/4 (none of them title-called), and the
IDirectSoundStream / CDirectSoundStream methods.

**D3D bound (Task 5).** D3DDevice_Clear -> 0x00269050 (XbSymbolDatabase; `ret 0x18`); hp1's two "unnamed"
COMPLEX_STATE entries are SetRenderState_StencilEnable 0x00266190 and SetRenderState_StencilFail 0x00266230, by
their bodies: they store D3D__RenderState[144] / [145] (0x275F90 / 0x275F94, the renderer's kRsStencilEnable /
kRsStencilFail) and push NV097 0x32C (stencil test enable) / 0x370 (stencil op fail). DrawIndexedVerticesUP
0x00269640 (`ret 0x14`) gets hp2's HLE. D3DVertexBuffer_Lock2 0x00268E10 is allowlisted: its one push is NV097
0x1710 (vertex cache invalidate). CreatePixelShader, SetPixelShaderConstant, GetRenderTarget2 and IsFencePending
stay disabled (found by neither source, and no title push goes through them). Coverage now: D3D 2 gaps (Task 6),
DSOUND 12.

**Vertex shaders (Task 6).** CreateVertexShader 0x00266A90 (`ret 0x10`) and SetVertexShaderConstantNotInlineFast
0x00266BD0 (`__fastcall`, `ret 4`) get hp2's HLE. PoA facts, from its D3D code: `m_VertexShader` is still device
+0x794 (SetVertexShader 0x00267090 stores the handle there, and +0x798 / +0x79C after it); XDK 5849's
NotInlineFast keeps no constant mirror in memory (5028's, hp2 0x0013F7D0, does): it only pushes NV097 0x1EA4 with
ecx (the hardware register) and the data, so `VsConstants` writes only the renderer's copy. SetVertexShaderConstant1
/ 4 are not title-called. Run (2026-10-30, `RECOMP_HLE_DSOUND=1`): `d3d: vertex shader 01303381, 7 decl tokens, 59
instructions, type 2078` at renderer init, no unknown handle. Begin/End immediate mode: only `sub_00049A70` and its
neighbour (both called from `sub_0004A3C0`) use it, two quads each after SetRenderTarget and SetVertexShader: a
screen-space effect pass, not the menus. D3D coverage: 1 gap (SetVertexData4f).

**DirectSound bound (Task 7, steps 1-2).** The M1 binding of 0x002B8791 as DirectSoundCreateStream was wrong: it is
DirectSoundCreateBuffer (XbSymbolDatabase). hp1's CreateStream 0x001A89E9 is the same bytes but calls
CDirectSound::CreateSoundStream, while PoA's calls 0x002B83A1, which is hp1's CDirectSound::CreateSoundBuffer
0x001A8462 (body match) and XbSymbolDatabase's CDirectSound_CreateSoundBuffer. That explains baseline 2's 180
"CreateStream" lines with a callback on the stack (0x00F7FB7C): buffer descriptions read as stream ones.
`tools/xdk_port` now drops any match, unique ones too, whose calls contradict its donor's mapped callees, or whose
callee is already another donor function's match (`callee-conflict`; hp1 known answer still 184/190 with 0 wrong;
hp1 -> PoA drops 0x001A89E9, 0x001A6FF0, 0x001AACA4; no bound override lost its evidence). PoA creates no streams,
so `STREAM_VTABLE` stays 0. Bound: SetLoopRegion 0x002B761F, GetStatus 0x002B763F, SetCurrentPosition 0x002B767B,
SetFrequency 0x002B7A60, SetMixBins 0x002B7A7C (callee match by both donors + signature), GetCurrentPosition
0x002B765B (hp2's handler), Pause 0x002B7603, PauseEx 0x002B75DF, SetFilter 0x002B756B and SetEG 0x002B754F (not
reproduced, S_OK), IDirectSound_SetMixBinHeadroom 0x002B752F (S_OK), DirectSoundCreateBuffer 0x002B8791; each
`ret n` checked. 0x002B9760 (CMcpxAPU's scalar deleting destructor, only reached through the vtable thunk
`sub_002223E0`) is allowlisted. `check_hle_coverage.py --section DSOUND --strict` passes. Not yet run; the HLE is
still off by default (steps 3-4).

**DirectSound run (Task 7, step 3; 2026-10-30, m2-boot at 2c303b8, `RECOMP_HLE_DSOUND=1 RECOMP_DSOUND_TRACE=1
RECOMP_DSOUND_WAV`, 90 s, no pad).** The logo-movie hang is gone: `eac_usa.mad`, `wb.mad`, `poa_legal_eng.big`,
`intro.mad`, then `poa_transitional.big` and `poa_fe_eng.big` (the front end) about 65 s in; Swaps never stop
(41-60 fps during the movies, 59-60 fps after). Trace: DirectSoundCreate, DownloadEffectsImage, 181
CreateSoundBuffer (180 mono with SetEG + SetMixBins, never played: the front end's sound effects; one 6-channel buffer
with SetBufferData + Play, then refilled by polling GetCurrentPosition, 8424 calls: the movie audio), 0 FALLBACK.
The recorded mix (85 s) has RMS 3297 and changes like real audio through the logos and intro (5 s windows: RMS
300-6000, zero crossings 270-1600 Hz, nothing clipped), and is silent from the front end on (65-85 s). Counts: 0
`MakeSpace from`, 0 `ICALL`, 0 `[CRASH]`, 0 unknown handle; the two `Begin/End immediate mode` lines as before.
**Open (Task 9):** all four captures (20/40/60/80 s, two in the intro, one after the front end loads) hold a single
draw, a 640x480 screen-space quad (FVF 0x144, diffuse 0xFF7F7F7F, alpha blend, no texture listed, fixed-function
pixel stage), and the final picture is flat white (254,254,254): no movie picture and no front end, and no music on
the front end. The deferred Task 5/6 checks (cleared frames, skinned draws) are still open with it.

**Display timing and XAPI (Task 8).** The CDevice vblank fields are where 5659 has them: PoA's
BlockUntilVerticalBlank 0x00267470 is instruction for instruction hp1's 0x00192090 (waits on the KEVENT at device
+0x1DBC after clearing its SignalState +0x1DC0), and VBlankCount +0x1DE8 is written only by the vblank interrupt
handler 0x00269C90 (hp1 0x0019DA10, unique match; it increments the count when the interrupt's field bit says so).
Nothing in PoA reads +0x1DE8 and nothing touches +0x1DB8: SetVerticalBlankCallback, GetDisplayFieldStatus and
GetOverlayUpdateStatus are not linked into this XBE, so their overrides stay disabled (comments say why) and
`kDevVBlankCallback` stays 0x1DB8. The HLE's Swap paces on the runtime's vblank count, which Task 7's run showed at
59-60 fps on the front end. **XAPI:** PoA's XapiInitProcess 0x000117AB (XDK 5849) has no disc authentication.
Where hp1's calls 0x00155251 right after storing a result to a global (hp1 0x374F74, PoA 0x3CB6D4), PoA maps
`\Device\Harddisk0\partition1\` (sub_0001179A on the ANSI_STRING at 0x2F9BC8), then checks the cache drive
(sub_000116E0), the utility drive (sub_00012BCD) and a version (sub_00012869), each failing to XapiBootToDash
0x00011735. hp1's auth chain (0x0015519A, 0x00155251) has no PoA match, and Task 7's run never booted to the
dashboard (no XLaunchNewImage / HalReturnToFirmware) and mapped partition1, TDATA and UDATA. hle_xapi.c's override
stays disabled with that note.

**Boot fix 1 (Task 9): flat-white frames = copy swaps (2026-10-30).** Cause, from captures that name each draw's
caller (new `from` field in frame.txt): every present goes through the engine's `sub_000345B0`: BlockOnFence,
**Swap(2)** (D3DSWAP_BYPASSCOPY), `sub_00034110` (a full-screen quad, FVF 0x144, texcoords 0..640 x 0..480, diffuse
0x7F7F7F with MODULATE2X: the back buffer copied 1:1), **Swap(4)** (FINISH), InsertFence. The title creates its
device with `MultiSampleType 0x11, SwapEffect 3` (D3DSWAPEFFECT_COPY), so XDK 5849's CDevice::Init (0x0026B380)
sets device +8 bit 0x4000 and Swap (0x0026A610) takes its copy path, `sub_0026A2D0`: COPY/BYPASSCOPY count the swap
and save the target, depth and textures (`sub_0026A050`); BYPASSCOPY then sets the front buffer (+0x1A18) as the
render target with no depth and the back buffer (+0x1A14) as texture 0, stages 1-3 empty; FINISH (`sub_0026A0E0`)
restores them and flips to the front buffer. The HLE Swap presented and rotated on every call, so Swap(2) showed the
movie frame and Swap(4) showed the copy quad drawn with no texture (white). The HLE now does the same: copy swaps
when the parameters ask for them, BYPASSCOPY binds and FINISH restores and shows the front buffer, flipping swaps do
nothing until FINISH. Run 9 (45 s): each frame is the movie quads into the back buffer (0207C000) plus the copy
quad into the front buffer (01F50000) with `t0 8207C000 fmt 12 640x480 target`; the captures at 15/26/37 s show the
intro movie (train on the viaduct, a Dementor's hand, the snow scene); 59-60 fps. One earlier run of this build
(run 7, with `RECOMP_DSOUND_WAV`) stopped logging ~2 s in and would not exit for minutes; not reproduced in runs 8-9
(gdb stacks of run 8 showed nothing blocked). Watch for it in the done run.

**Done run (Task 9, 2026-10-30, m2-boot at 73ddeb7, 180 s, launcher defaults + `RECOMP_D3D_TRACE`,
`RECOMP_CAPTURE_AFTER=10:10:17`, `RECOMP_DSOUND_WAV`; nothing pressed, though a pad was connected).** The log
grew steadily the whole run (no repeat of run 7's stall). Sequence: `eac_usa.mad`, `wb.mad`, `poa_legal_eng.big`,
`intro.mad`, then `poa_transitional.big` / `poa_fe_eng.big` (about 60 s in): the title screen ("Harry Potter and
the Prisoner of Azkaban", "Press START") in the 60 s and 70 s captures; after its idle timeout `attract.mad` plays
(captures 80-130 s: Harry in bed, a spell flash, the Knight Bus / Patronus scenes, Ron with the Monster Book, a
library troll, Harry) and returns to the title (140 s, 150 s), then attract again (160-170 s). The 10 s capture is
black (between logos). 59-60 fps outside movie starts. Counts: 0 `MakeSpace from`, 0 `ICALL`, 0 `[CRASH]`, 0
unknown handle, 0 DSOUND FALLBACK. Audio RMS per 10 s: 950-5500 through the logos, intro and attract, **0 on the
title screen** (60 s and 140 s windows): whether the console's title screen has music is for the user check (Task
11). `check_hle_coverage.py --strict --section D3D --section DSOUND`: DSOUND 0, D3D 1 gap, SetVertexData4f
0x00269AC0. Its only Begin/End use ran exactly twice in the run, both at start-up before the first movie
(`sub_00049A70` and its neighbour from `sub_0004A3C0`: SetRenderTarget, then two immediate-mode quads), so nothing on
the boot path to the title is drawn through it; what that start-up pass renders (and whether a level samples it) is
an M3 item, with immediate mode in the renderer.

**Boot fix 2 (Task 9): the stall at the first movie = a lost KTIMER expiry (static analysis, 2026-10-30).** The movie
loop at 0x0005E6E4 (`sub_0005E6D0`) spins on the timer-callback dispatcher `sub_00147BC0`, which runs callbacks as
the game clock 0x3836C4 passes their due ticks; it leaves only when a callback produces work, so a clock that stops
is an endless spin with no kernel calls (no log output), as runs 7, 8 and 11 show. The clock: engine init
(`sub_001A6D30` -> `sub_001A8AB0` -> `sub_00222C40(rate)`) starts a tick thread `sub_00222B10` that waits on the NT
event at 0x3C2C58 and increments 0x3836C0 / 0x3836C4; the event is set (`sub_00222570`, NtSetEvent) by the callback
`sub_00222B90`, which re-arms itself each time through XAPI's `timeSetEvent` (0x00247768, the only caller) with a
1 ms relative delay (its floor, 0x00222C03). XAPI's timer service thread (0x00247580) keeps 63 KTIMERs (type 1),
waits on them and a control KEVENT with KeWaitForMultipleObjects, and runs the callback on itself; `timeSetEvent`
arms one KTIMER (KeSetTimerEx, due = -delay x 10000 via `_allmul` `sub_0013E2B0`) and sets the control event. The
runtime's KeSetTimerEx (`kernel_set_timer`) armed the slot under its timer lock but reset the timer's shadow event
after dropping it; its timer thread (1 ms loop, due times in GetTickCount64 steps of ~15.6 ms, pinned to the guest
core like every guest thread) could fire the timer in between, and the late ResetEvent erased the expiry of a
one-shot whose slot was already free: the timer never fires again, XAPI's thread and the tick thread wait forever.
The window is a whole scheduler quantum when the arming thread is preempted there, which is what happens once the
main thread starts busy-spinning in the movie loop on the same core: hence the first movie. gdb of run 8 matches:
tick thread in NtWaitForSingleObjectEx (`sub_00222580`), XAPI timer thread in KeWaitForMultipleObjects, the runtime
timer thread idle in Sleep(1), main thread in `sub_00147BC0` under `sub_001AC0A0`. Ruled out: the 32-entry armed
timer table overflowing (one timeSetEvent caller, two KeSetTimerEx sites), the 4096-entry shadow table, NtPulseEvent
(the tick event is set, not pulsed), a timer restart per movie (start/stop only at engine init/shutdown). Fix
(3e7e884, toolkit `src/kernel/kernel_bridge.c`): reset + arm in one step under the lock, and an expiry signals under
the lock before its DPC (KiTimerExpiration's order). To confirm with runs once the PC is free.

**Confirmation runs conf1-conf6 (2026-10-30 12:2x, exe CF685B849B99 = 3e7e884, test\PoA_USA).** conf1-conf5 all
pass the first movie: each log opens `eac_usa.mad`, `wb.mad`, `poa_legal_eng.big`, `intro` and has `[FRAME]` lines,
no stall. conf6 stopped during startup at 12:26:55, before any movie (the session driving it ended; no Windows
Application Error for hppoa). 5/5 clean vs 3/7 stalled before the fix (chance of 5 clean in a row at the old rate
~6%): the lost-KTIMER fix is confirmed.

**Task 11 user check (2026-10-30, exe CF685B849B99, test\PoA_USA).** The user ran it and confirmed that logos,
title, front-end menu, menu sounds and music work. Not answered: whether the console's title screen is also silent,
and whether the `debug_menu` setting lets a title-screen button open the debug menu (both carried to M3).

**M2 review follow-ups (DeepCode whole-branch review, 2026-10-30; all pre-existing on main, none hit in PoA runs).**
- **Done (M3 Task 1):** `tools/xdk_port/match.py` unique-body pass (`matched[a] = at[0]`): no check that another
  donor already took the target, so two byte-identical donor wrappers can map to one PoA address. Such pairs are now
  ambiguous (`test_unique_body_shared_target_is_ambiguous`). Rerun over the 1094 hp1 library functions of
  `lib2_hp1_to_poa.json` (old vs new matcher): one real collision, hp1 0x001A5DD4 and 0x001A8D35 (two copies of a
  DSOUND float-to-volume helper: `fcomp`, returns 0xFFFFD8F0 = -10000) both on PoA 0x002B67CB, now unbound; with
  that contradiction gone, hp1 0x001A6FF0 (was callee-conflict) binds to PoA 0x002B777D (same prologue, calls the
  same 0x2B6710 counterpart). No override uses any of these addresses, so no binding changed.
- **Done (M3 Task 2):** `src/kernel/kernel_bridge.c` `kernel_set_timer`: the lazy `g_timer_lock` init and
  timer-thread start were unguarded (two first callers at once would double-init). Now `timer_init()` with
  InitOnceExecuteOnce, as `g_dpc_lock` already does; `xbox_kernel_cancel_timer` uses it too.
- `port/src/hle_dsound.c` `Play` does not clear `paused`: check what XDK Play does on a paused buffer before changing.

## M3 runs

Test folder `test\PoA_USA`, started by `poarun.ps1` (scratchpad helper: copies the exe, `HPPOA_LAUNCHER=1`,
`debug.env` = launcher defaults, kills only its own PID).

**m3t2 (Task 2, InitOnce timer init; 120 s, nothing pressed).** Hypothesis: the timer-init change keeps M2's boot.
Result: `eac_usa.mad`, `wb.mad`, `intro.mad`, title (`poa_fe_eng.big`), `attract.mad`, back to the title; 23
`[FRAME]` lines, 0 `MakeSpace from` / `ICALL` / `[CRASH]` / unknown handle; the two `not supported` immediate-mode
lines as before; 50 `[UNIMPL]` = the same pushfd/popfd no-op notes as conf5 (44 in a shorter run).

**m3t3 (Task 3, pad injection; 150 s, Start at 65 s, A at 72 s via `poatrainer.py`).** Hypothesis: injected Start
leaves the title and A picks the highlighted entry. Result: both `[TRAINER] input` lines; New Game was the
highlighted entry, so the run went straight on: `system_systemgui.hpb`, the Loading screen (the train on the
viaduct), `packages\level\hogwartsexpressizcell_pasengercar.pkg`, `ar3_ws_he_*.pkg`, speech `day5.sdt` /
`hogwarts_express.sdt`, sfx `hogex_amb_str.sdt`; the 150 s capture shows Harry in the train corridor with the HUD.
New in the level: **massive audio clipping** (heard by the user), `[ICALL] Failed to resolve VA 0x000CBEE0`
(repeating), 16 x `MakeSpace from 00268E2D`. 0 `[CRASH]`. These are Task 5's first blockers.
