# HP: Prisoner of Azkaban port: changes that alter the game

Every change that makes the port behave or look different from the Xbox, or that replaces game code, and what else it
could affect. When something looks wrong, check this list first and switch the suspects off before hunting
elsewhere. Add an entry for every new change of this kind. (Paths are under `port/`.)

Columns: what changed, where, how to turn it off, what it could plausibly break.

The port is a fork of the Philosopher's Stone port: the renderer and system rows below come with it and have not been
checked against PoA yet (the game has not run; docs/bringup.md).

## Game logic replaced or changed

| Change | Where | Off switch | Could affect |
|---|---|---|---|
| Game clock: XAPI's QueryPerformanceCounter (rdtsc) answered by the host, scaled while a cutscene fast-forwards (never yet for PoA: no cutscene state found) | `src/hle_skip.c` over `sub_00195129` (the matched QPC) | `cutscene_skip = false` / `RECOMP_CUTSCENE_SKIP=0` | anything timed by the game clock |
| Stick radial range compensation | input path | `RECOMP_STICK_RANGE` | walk/run thresholds, all stick input |
| Settings: fps, keyboard/mouse, colour-blind, controller | `src/scda/app`, launcher | per setting | timing, input |
| Frame-rate caps above 60 (120/144/165/240): Swap paced by a high-resolution waitable timer, BlockUntilVerticalBlank does not wait. Whether PoA's logic is frame-locked is not known yet (it picks 25/30 fps from the video standard) | `scda/xdk/hle_d3d8.cpp` `PaceAbove`, `scda/app`, launcher | `fps_limit = 0` (the game's rate) | game speed if PoA steps logic per frame |

## Camera / 16:9

No PoA camera hook yet (Philosopher's Stone's widened the engine's camera setter). The renderer's 2D handling
(`Squeeze2D`, `WideFrame` in `d3d12.cpp`) comes with the fork and is unverified for PoA.

## Renderer (D3D8 HLE + D3D12, `src/scda/`)

| Change | Where | Off switch | Could affect |
|---|---|---|---|
| Pipeline recording: every pipeline made goes into `shadercache/pipelines_list.bin`, and all of them are made at startup on the build threads (2026-10-24, from HP:CoS) | `pipeline_cache.cpp`, `d3d12.cpp` `StartPrebuild` | `RECOMP_PREBUILD=0` | startup work on the build threads |
| Per-game Mesa shader cache under Wine (`run/shadercache/mesa`, 4 GB): Wine's vkd3d has no pipeline library, and the shared 1 GB cache evicted the game's pipelines | `run_win.sh`, `run_launcher.sh` | set `MESA_SHADER_CACHE_DIR` yourself | first run after the change compiles everything once |
| Lines drawn as the NV2A draws them: a geometry shader makes each segment a quad one console pixel across, its ends on console pixel edges, half a pixel down/right for the NV2A's integer pixel centres (2026-10-24, MiddleDucky, from their HP:PoA work: PoA's shadow fade mask relies on it) | `shaders.cpp` `LineGsHlsl`, `d3d12.cpp` `EnsureVertexShader` / `EnsurePipeline` | `RECOMP_LINE_GS=0` | every line draw (thicker when upscaled, as on the console) |
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
| Factory game region (0x104) answered from the XBE certificate (the USA release: 1) | toolkit copy `src/kernel/kernel_xbox.c` | `RECOMP_GAME_REGION=<n>` | language choice, region checks |
| DirectSound replaced by host mixer (on by default; PoA's XDK DirectSound crashes on the emulated APU) | `src/hle_dsound.c` | off with `RECOMP_HLE_DSOUND=0` | all sound; `RECOMP_AUDIO_LATENCY_MS` |
| 5.1 surround output (2026-10-24): F10 Audio > Speakers (`speakers = 6`); each mixbin goes to its own speaker (front L R, centre, LFE, back L R; crosstalk bins to their positions), a multichannel buffer channel by channel, 6-channel XAudio2 output with mask 0x3F. Stereo (the default) mixes as before | `src/hle_dsound.c` `levels6`, `scda/app` | `speakers = 2` | all sound when 5.1 is on; needs a 5.1 Windows speaker setup |
| Display vblank counter kept by the host (unbound for PoA so far) | `src/hle_video.c` | none | game timing tied to vblank |
| Factory AV region answered (NTSC) | toolkit `kernel_xbox.c` | none | video standard, game clock step |
| Console time zone = the PC's (bias, daylight dates and biases, names; also the whole user block, index 0xFF, which XAPI's GetTimeZoneInformation reads) - was a fixed UTC+5 | toolkit `kernel_xbox.c` `ExQueryNonVolatileSetting` | none | local time: save timestamps, anything the game shows from the clock |
| ISO/XISO unpacked by the launcher | `src/launcher/xiso.c` | none | game files on first run |
| Release hardening (`HPPOA_RELEASE`), saves in `<exe dir>\saves` | build, `kernel_path.c` | debug build | debug keys/tools absent in release; save location |
