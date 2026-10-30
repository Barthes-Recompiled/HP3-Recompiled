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

## Globals and code addresses carried by evidence (verify in M2)

Derived from matched function pairs: the same instruction in hp1's and PoA's copy of a function, one vote per
occurrence (`tools.xdk_port.match.data_map`), or by reading the same table in both images.

| Name (file) | hp1 | PoA | Evidence |
|---|---|---|---|
| D3D__pDevice (hle_d3d8.cpp kDevicePtr, main.c fence) | 0x001A13D8 | 0x002763E8 | 101 votes, no other candidate |
| static CDevice (kDevice) | 0x001A13E0 | 0x002763F0 | 3 votes; pDevice + 8 in both |
| D3D__DirtyFlags | 0x001A0F38 | 0x00275F48 | 59 votes |
| D3D__TextureState | 0x001A0F40 | 0x00275F50 | 6 votes; DirtyFlags + 8 in both |
| D3D__RenderState | 0x001A1140 | 0x00276150 | 11 votes; TextureState + 0x200 in both |
| g_Stream | 0x001A3898 | 0x00278490 | 6 votes. CDevice to g_Stream is 0x24B8 in hp1, 0x20A0 in PoA: the device layout changed |
| D3DSIMPLERENDERSTATEENCODE (kSimpleEncode) | 0x001FB2B0 | 0x002C7AD8 | 1 vote: check |
| D3D_DestroyResource | 0x00194A70 | 0x0026A810 | matched function |
| default state init (kInitDefaultState) | 0x00196D50 | 0x0026B180 | matched function |
| XPP device type table begin / end | 0x001F4204 / 0x001F420C | 0x002BF924 / 0x002BF92C | 2 votes each |
| g_DeviceType_Gamepad | 0x001F422C | 0x002BF9F4 | the gamepad's XPP_DEVICE_TYPE_INFO (+4) in both tables; same layout |
| entry point (main.c) | 0x00154331 | 0x000111DB | the XBE header |
| title .text end (hle_d3d8.cpp stack scan) | 0x00190714 | 0x00264C64 | the XBE section table |

**Unverified for 5849:** every CDevice field offset hle_d3d8.cpp uses (PsTextureModes +0x790, m_VertexShader +0x794,
ZScale +0x948, SuperX/Y +0x95C/+0x960, transforms +0xC60, viewport +0xEE0, textures +0xF88, palettes +0xF98,
RT/depth +0x1A04/+0x1A08, vblank count +0x1DE8, VBlankCallback +0x1DB8, swap count +0x2478). An offset that a
matched function also uses is confirmed (it is part of the bytes that matched); the rest must be checked against
PoA's code in M2. The g_Stream distance above shows the device did change size.

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
