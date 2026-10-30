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
  SetPixelShader 0x00270D00, LoadVertexShader 0x00266D40, SetShaderConstantMode 0x00266E40). Port hp2's shader
  path if the survey (docs/bringup.md, Task 8) confirms PoA draws with shaders.

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
