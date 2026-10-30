# HP: Philosopher's Stone port: changes that alter the game

Every change that makes the port behave or look different from the Xbox, or that replaces game code, and what else it
could affect. When something looks wrong, check this list first and switch the suspects off before hunting
elsewhere. Add an entry for every new change of this kind. (Paths are under `port/`.)

Columns: what changed, where, how to turn it off, what it could plausibly break.

## Game logic replaced or changed

| Change | Where | Off switch | Could affect |
|---|---|---|---|
| Start-up logo movies left out: the EA logo (`movies/ea_<language>.thp`) and Warner Bros. (`movies/splashwb.thp`) are not played; the attract cutscene and title follow at once (boot to attract 7 s instead of about 12 s) | `src/hle_video.c` over `sub_00164450` (the movie player) | `intro_skip = false` (hppoa.toml, F10 > Gameplay, launcher) or `RECOMP_INTRO_SKIP=0` | start-up only; every other movie (menu background, trailer, Quidditch) plays |
| Cutscene fast-forward: holding Start or Back 0.5 s while the letterbox bars are down ([0x32E1FC] 1/2) runs the cutscene to its own end at 16x: the game clock (`sub_0018DD83`, its rdtsc QPC) and the DirectSound mixer run 16x (output silent), frame pacing is off and the renderer presents a frame every 250 ms; ends when the bars start to go. Start/Back still reach the game | `src/hle_skip.c` over `sub_0018DD83`; hooks in `hle_input.c`, `hle_dsound.c`, `scda/xdk/hle_d3d8.cpp` (Pace, BlockUntilVerticalBlank), `scda/gpu/d3d12.cpp` (Present) | `cutscene_skip = false` (hppoa.toml, F10 > Gameplay, launcher) or `RECOMP_CUTSCENE_SKIP=0` | anything timed by the game clock or the mixer while a cutscene fast-forwards; a cutscene whose bars lift between shots needs the hold again |
| Door animations play 1.8x as fast (doors take ~7 s on the console) | `src/hle_anim.c` over `sub_00142DB0` (door state machine `sub_000D86D0`, push at `0xD7332`) | `RECOMP_DOOR_SPEED=1` | anything sharing those animation calls; Harry's push timing |
| Cloisters corridor pupil (stands in mid-air at height 900) never drawn | `src/hle_world.c` | `RECOMP_HIDE_CHARACTERS` | that entity only (by name) |
| XAPI DVD-X2 disc authentication skipped | `src/hle_xapi.c` over `sub_00155251` | `RECOMP_HLE_XAPI=0` | boot only |
| "Quit To Desktop" row on main/pause menu | `src/hle_frontend.c` (menu input `sub_0001F200`, draw `sub_0001C380`) | `RECOMP_QUIT_ITEM=0` | menu row count, wrapping, Extras row |
| Stick radial range compensation | input path | `RECOMP_STICK_RANGE` | walk/run thresholds, all stick input |
| Trainer hooks (warp, items, autoplay) | `src/hle_trainer.c` over `sub_00095370` | only with the trainer / `RECOMP_AUTOPLAY` | Harry's update each frame |
| Settings: fps, keyboard/mouse, colour-blind, controller | `src/scda/app`, launcher | per setting | timing, input |
| Frame-rate caps above the console's 60 (120/144/165/240): Swap paced by a high-resolution waitable timer, BlockUntilVerticalBlank does not wait. The game steps its logic at a fixed 25 Hz on its own clock (dt 0.04 at 0x21B7BC is a constant), so this only draws more frames; measured: Harry 25.1 updates/s and the same walk speed at 60, 144 and 607 fps (2026-10-30) | `scda/xdk/hle_d3d8.cpp` `PaceAbove`, `scda/app`, launcher | `fps_limit = 0` (the game's rate) | anything that counts rendered frames rather than game steps |

## Camera / 16:9

| Change | Where | Off switch | Could affect |
|---|---|---|---|
| True 16:9: cameras widened (x scale 3/4) | `src/hle_video.c` Camera_SetScale, `scda/gpu/d3d12.cpp` | aspect 4:3 | anything computed from the projection |
| Wide camera's near plane cut to 0.75x so walls don't show outside the level | `d3d12.cpp` | `RECOMP_WIDE_NEAR=1` | near clipping, depth precision close up |
| 2D squeeze, centred panels, spinning marker, cutscene bars, WideFrame crop | `Squeeze2D`, `WideFrame`, `d3d12.cpp` | 4:3 | any draw classed as 2D, cutscene framing |
| Menu background movie / loading screen crops | `src/hle_video.c` | `RECOMP_MENU_MOVIE_CROP`, `RECOMP_LOADING_CROP` | those screens |

## Renderer (D3D8 HLE + D3D12, `src/scda/`)

| Change | Where | Off switch | Could affect |
|---|---|---|---|
| Pipeline recording: every pipeline made goes into `shadercache/pipelines_list.bin`, and all of them are made at startup on the build threads (2026-10-24, from HP:CoS) | `pipeline_cache.cpp`, `d3d12.cpp` `StartPrebuild` | `RECOMP_PREBUILD=0` | startup work on the build threads |
| Per-game Mesa shader cache under Wine (`run/shadercache/mesa`, 4 GB): Wine's vkd3d has no pipeline library, and the shared 1 GB cache evicted the game's pipelines | `run_win.sh`, `run_launcher.sh` | set `MESA_SHADER_CACHE_DIR` yourself | first run after the change compiles everything once |
| Lines drawn as the NV2A draws them: a geometry shader makes each segment a quad one console pixel across, its ends on console pixel edges, half a pixel down/right for the NV2A's integer pixel centres (2026-10-24, from HP:PoA, whose shadow fade mask relies on it) | `shaders.cpp` `LineGsHlsl`, `d3d12.cpp` `EnsureVertexShader` / `EnsurePipeline` | `RECOMP_LINE_GS=0` | every line draw (thicker when upscaled, as on the console) |
| Whole renderer: D3D8 calls replaced by HLE | `scda/xdk/hle_d3d8.cpp` | `RECOMP_HLE_D3D=0` | everything drawn |
| 3D draws with depth on the log2(1+w) scale | `UseLogDepth` | `RECOMP_LOGDEPTH_ALL=0` | depth ties, effects vs geometry |
| Target read as a no-alpha format reads alpha 1 | `BindTargetTexture` | none | alpha of render-to-texture reads |
| Loading-bar track fix on the loading picture | `d3d12.cpp` (`load_hognight.tga`) | none | that texture |
| Colour-blind correction in the gamma pass | `d3d12.cpp` | setting off | final colours |
| GPU failsafes: lost/hung GPU ends the game; draw counts past a limit dropped as garbage | `d3d12.cpp` | none | a genuinely huge draw |
| Pipeline hold limit raised (first frames on a new PC) | `d3d12.cpp` | none | frames held while shaders build |
| Shipped shader cache `base_shaders.bin` | `d3d12.cpp`, release | delete file | first-use stutter only |
| Draw-all / culling, P8 palettes as BGRA, border addressing in the shader | `d3d12.cpp`, `shaders.cpp` | none | textures at edges, palettes |
| 2D seam fix: quad texture coordinates held inside the quad's rectangle (`qclamp`); texel units only for linear textures (2026-10-22 fix from CoS: guessing from coordinates > 1.5 flattened scrolling 2D layers after a long run) | `shaders.cpp` `QuadHlsl` | none | edges of 2D quads, scrolling 2D layers |

## Audio / system

| Change | Where | Off switch | Could affect |
|---|---|---|---|
| Heap reclaim on by default in the toolkit (freed memory reused, as the console does; RtlHeap decommits once instead of ~38k failed retries) | toolkit copy `src/kernel/xbox_memory_layout.c` | `RECOMP_HEAP_RECLAIM=0` | addresses of every later allocation |
| Cache partition (Z:) kept between boots: toolkit partition table moved to sector 0, XAPI's cache database in sector 4 left to the title; dismount (FSCTL 0x90020) succeeds | toolkit copy `src/kernel/kernel_path.c`, `kernel_bridge.c`, `kernel_file.c` | none | Z: contents, first boot formats once |
| Factory game region (0x104) answered from the XBE certificate: this PAL release gets 4 | toolkit copy `src/kernel/kernel_xbox.c` | `RECOMP_GAME_REGION=<n>` | language choice, region checks |
| DirectSound replaced by host mixer | `src/hle_dsound.c` | `RECOMP_HLE_DSOUND=0` | all sound; `RECOMP_AUDIO_LATENCY_MS` |
| 5.1 surround output (2026-10-24): F10 Audio > Speakers (`speakers = 6`); each mixbin goes to its own speaker (front L R, centre, LFE, back L R; crosstalk bins to their positions), a multichannel buffer channel by channel, 6-channel XAudio2 output with mask 0x3F. Stereo (the default) mixes as before | `src/hle_dsound.c` `levels6`, `scda/app` | `speakers = 2` | all sound when 5.1 is on; needs a 5.1 Windows speaker setup |
| Display vblank counter kept by the host | `src/hle_video.c` | none | game timing tied to vblank |
| Factory AV region answered (NTSC) | toolkit `kernel_xbox.c` | none | video standard, game clock step |
| Console time zone = the PC's (bias, daylight dates and biases, names; also the whole user block, index 0xFF, which XAPI's GetTimeZoneInformation reads) - was a fixed UTC+5 | toolkit `kernel_xbox.c` `ExQueryNonVolatileSetting` | none | local time: save timestamps, anything the game shows from the clock |
| ISO/XISO unpacked by the launcher | `src/launcher/xiso.c` | none | game files on first run |
| Release hardening (`HPPOA_RELEASE`), saves in `<exe dir>\saves` | build, `kernel_path.c` | debug build | debug keys/tools absent in release; save location |
