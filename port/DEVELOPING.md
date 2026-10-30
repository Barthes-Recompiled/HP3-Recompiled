# Developing the HP: Prisoner of Azkaban port

Everything needed to go from a fresh clone to a built, running and packaged game. Players want
[README.txt](README.txt) instead.

The port is a static recompilation of the Xbox title (`default.xbe`, XDK 5849) built on the
**xboxrecomp** toolkit (repository root: `src/`, `tools/`, `templates/`, see
[README.xboxrecomp.md](../README.xboxrecomp.md)), with the game project in `port/`.
Rendering is the D3D8 HLE → Direct3D 12 renderer from
[SCDA-XBERecomp](https://github.com/TGP482/SCDA-XBERecomp) (MIT, `port/src/scda/LICENSE.SCDA`),
adapted to this title. The project is a fork of the Philosopher's Stone port (hp1_recomp, MiddleDucky's
work and barthes_recompiled's) with fixes from the Chamber of Secrets port (hp2_recomp); see
[docs/toolkit-sync.md](../docs/toolkit-sync.md).

**State (2026-10-30):** the recompiled code regenerates and links; the game has not been run yet. What the boot
milestone has to finish is in [docs/bringup.md](../docs/bringup.md).

**Nothing from the game is in this repository**: no game files, no disassembly, no recompiled code, no
artwork. You need your own copy of the Xbox disc; the pipeline regenerates everything else from it.


## 1. Layout

```
<repo>/
  game/                   YOUR disc files (default.xbe, data/) - ignored by git
  src/ tools/ templates/  xboxrecomp toolkit: runtime libraries (kernel, D3D, audio, input, video, USB...)
                          and the Python pipeline (xbe_parser, disasm, func_id, abi_analysis, recomp)
  cmake/ include/ docs/ tests/ LICENSES/   toolkit
  port/
    CMakeLists.txt        the game: hppoa.exe, hppoa_launcher.exe, hppoa_trainer.exe
    regen.sh              XBE -> port/src/recomp/gen/*.c (generated, ignored by git)
    icall_seeds.json      indirect-call targets the disassembler can't find by itself (regen.sh input)
    src/                  hand-written code
      main.c              startup: single instance / launcher check, debug.env, settings, XBE load, kernel
      hle_*.c             overrides of recompiled game/XDK functions (see 6.2)
      scda/               renderer (gpu/d3d12.cpp, shaders.cpp, pipeline_cache.cpp), XDK D3D8 HLE
                          (xdk/hle_d3d8.cpp), settings + F10 panel + input (app/)
      launcher/           hppoa_launcher.exe (Dear ImGui over D3D11)
      trainer/            hppoa_trainer.exe, the debug trainer
      recomp/             recomp_types.h etc.; gen/ is generated
      difftest.c          Unicorn-based original-vs-lifted comparison (HPPOA_DIFFTEST builds)
    res/base_shaders.bin  shader bytecode shipped with releases (see 5)
    tools/                package_release.sh, xbx_to_ico.py, check_tls_refs.py, memtool, warp
    third_party/imgui/    Dear ImGui (MIT)
    README.txt            the players' readme (shipped with releases)
```


## 2. Setup (Windows)

Builds are native Windows builds with MinGW-w64 GCC from **MSYS2, UCRT64 environment**.

1. Install MSYS2 (`winget install --id MSYS2.MSYS2 -e`, or msys2.org).
2. In the **MSYS2 UCRT64** terminal (type commands; pasted text can carry invisible characters):
   ```
   pacman -Syu
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja \
                      mingw-w64-ucrt-x86_64-python mingw-w64-ucrt-x86_64-python-capstone
   ```
   (`pacman -Syu` may close the window; open it again and repeat it once.) GCC 16.2 is what the port has been
   built and checked with.
3. Copy the disc's files into `<repo>/game/` so that `<repo>/game/default.xbe` exists.

From PowerShell, any MSYS2 command runs as
`$env:MSYSTEM="UCRT64"; C:\msys64\usr\bin\bash.exe -lc "cd /d/<path-to-repo> && <command>"`.

If files were copied from another machine with a clock ahead of this one, Ninja loops with
"manifest 'build.ninja' still dirty"; reset the future timestamps (any file dated later than now) and configure
again.


## 3. Regenerate the recompiled code

```
bash port/regen.sh          # from the repo root, in MSYS2 UCRT64; a few minutes
```

It disassembles `game/default.xbe` (seeded with `port/icall_seeds.json`), identifies library functions, analyses
calling conventions and writes `port/src/recomp/gen/recomp_000N.c` + headers.

Every hand-written override file is passed with `--exclude-manual`: a `sub_XXXXXXXX` defined in one of them keeps
the generated body under `sub_XXXXXXXX_gen`, which the override calls when it wants the original behaviour.
**A new override file must be added to regen.sh** (and its `.c` is picked up by CMake's source glob). Never hand-edit
`gen/`: regenerate. A hook that has to sit *inside* a generated function (a debug trace at one instruction) goes in
`port/tools/patch_gen.py`, which regen.sh runs last; it finds its spot by a unique label and fails loudly if the
label moves.

If the game jumps through a pointer to code the disassembler never saw (an `ICALL` failure in the log), add the
target to `icall_seeds.json` and regenerate. `port/tools/scan_code_ptrs.py` lists code pointers found in data.


### 3.1 Regions

The USA disc is canonical. The European disc's `default.xbe` is the same program (same size, 534 bytes differ); its
profile is not made yet (M4), so regen.sh refuses it for now and lists the supported SHA-1.

| Disc | `default.xbe` SHA-1 | Profile |
|---|---|---|
| USA (NTSC) | `51b09e01169fda86f2cb7e5ce45ee80a10268918` | `poa-usa` (canonical) |
| Europe (PAL, En/Fr/De/Es/It/Nl/Sv) | `c8841e3c1d2d5ec20dbe286e5bec9bb5104b41f9` | not yet |

The hand-written sources (overrides, `icall_seeds.json`, `tools/patch_gen.py`) always use the canonical XBE's
addresses. `regen.sh` starts by running `tools/region/retarget.py`, which hashes `game/default.xbe` and picks the
matching profile in `port/region/`:

- **Canonical:** nothing changes. A leftover `src/recomp/region/` folder is removed.
- **The other region:** it writes copies of every file whose addresses change into `src/recomp/region/`
  (git-ignored). `sub_`/`loc_` names, macro address arguments and in-image hex literals are rewritten through the
  profile's address map. Those copies are what regen.sh and CMake (`port/region/region.cmake`) use.
- **Any other XBE:** it stops and lists the supported SHA-1s.

A literal that can't be mapped stops the run with file:line. So does a number-like value (e.g. `0x40000`) in code
that isn't classified yet: list it in `port/region/retarget.json` under `numbers` (a plain number, kept) or
`addresses` (mapped). New override code needs no other care. Write it in canonical addresses as before and run
regen once for each region.

After the first level loads, the log has one line, e.g. `[REGION] ps-usa: ... -> ok`. It confirms that
the game state the overrides read sits where the profile says. `SUSPECT` means the profile needs a look.

Rebuilding the address map, for example for a new revision of a disc, is described in `tools/region/README.md`.
Each checkout has a single `src/recomp/gen/`, so switching discs means rerunning `regen.sh`.

## 4. Build

```
cd port
cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-win -j$(nproc)
```

Outputs in `port/build-win/`: `hppoa.exe`, `hppoa_launcher.exe`, `hppoa_trainer.exe`.

* The exe icon is made at configure time from `game/default.xbe`'s title image (`$$XTIMAGE`) by
  `tools/xbx_to_ico.py`. Without the XBE, and always in release builds (`HPPOA_NO_ART`), the port's own icon
  `res/app.ico` is used instead: a wand and sparkle drawn from shapes by `tools/make_icon.py`, original artwork.
  `-DHPPOA_GAME_XBE=<path>` points at another XBE.
* The launcher shows `src/launcher/launcher_bg.png` behind its page when that file exists (not in the repo: game
  art). One can be made from an F12 capture of the main menu (logo texture over the menu background).
* Generated code is compiled at **-O2** on purpose: GCC 16 at -O3 hoisted the address of a thread-local array
  out of a loop, reading the TLS template instead of the thread's copy. The post-build step
  `tools/check_tls_refs.py` fails the build on any such access ("58 thread local variables, no global address
  reads" is the good result).
* CMake options: `HPPOA_RELEASE` (players' build: hardening, SSE4.1, no debug keys), `HPPOA_NO_ART` (no game
  artwork inside the exes: no icon, no launcher picture, the launcher shows the title as text in Windows' Georgia;
  package_release.sh always sets it, so nothing of Warner Bros.' is in a release), `HPPOA_DIFFTEST` (see 7.4),
  `HPPOA_CALL_PROFILE` (per-target call counts, F7 dumps).


## 5. Run, test, release

**Run folder.** The exe looks for `game\default.xbe` in its own folder. Make a run folder (e.g. `port/run/`,
ignored by git) with the exes and a junction to the game files:
```
mkdir port\run
mklink /J port\run\game game          (cmd, from the repo root; no admin needed)
copy port\build-win\*.exe port\run\
```
It creates `hppoa.toml` (settings), `saves\` (Xbox UDATA/TDATA, one folder per game, beside the exe) and
`shadercache\` there.

**Starting.** `hppoa.exe` only runs when started by `hppoa_launcher.exe` (which sets `HPPOA_LAUNCHER=1` and the
`RECOMP_*` defaults the game expects, see `StartGame` in launcher.cpp). For scripted test runs set
`HPPOA_LAUNCHER=1` yourself. Only one copy runs at a time (named mutex, `src/single_instance.h`); a second start
brings the running one forward. Alt+F4 ends the process at once (`fb_present.c`).

**debug.env** beside the exe: `KEY=VALUE` lines set as environment variables however the game is started (an
existing variable wins, `#` comments). This is how trace switches are turned on.

**Logs.** Only with F10 > Debug > "Write debug logs" (`debug_logs = true` in hppoa.toml, from the next start):
`hppoa_run.log` (the game's console output, written by the launcher) and `xbox_kernel.log` (kernel layer).
Crash reports (`[CRASH]` with symbolised guest functions; hppoa.exe keeps its symbol table) land in hppoa_run.log.

**Shader cache.** Shaders are generated HLSL keyed by a hash of their source, compiled with D3DCompile and kept in
`shadercache\shaders.bin`; pipelines in `shadercache\hppoa_pipelines.bin` (per driver). A release ships
`res/base_shaders.bin` as `shadercache\base_shaders.bin` (read only, after the player's own) so a new PC compiles
almost nothing; bytecode a driver refuses is recompiled locally (`DropBaseCode`). Draws whose pipeline is still
building are skipped (`async_shaders`), and such a frame is not shown: the last whole frame stays up for up to 3 s
(`kHoldMax`) - without that, the first visit to an area showed the level's clear colour (light blue). **Refresh
`res/base_shaders.bin`** (copy a well-played `shadercache\shaders.bin` over it) whenever shader generation changes;
stale entries are harmless but useless.

**Release.** `bash port/tools/package_release.sh` (MSYS2) configures `build-release` with `HPPOA_RELEASE=ON`,
builds, and fills `port/dist/hppoa-<date>/` with hppoa_launcher.exe, hppoa.exe, README.txt and
`shadercache/base_shaders.bin`. It checks ASLR/NX/high-entropy, the stack protector, and that every imported DLL
ships with Windows 10/11 (C/C++ runtimes are static; XInput is `xinput1_4`). **It never deletes the folder**
(testers drop game files into it): only its own files are overwritten. Keep it that way.


## 6. How the port works

### 6.1 Title facts

EA UK's own engine (build path `D:\dev\POA\builds\applications\harrypotter\xbox_release\appharrypotter.exe`; the
same engine family as the PS2 Chamber of Secrets: `.hpb` room scripts, `.big` archives, APT menus), XDK 5849 (D3D8,
D3DX8, DSOUND, LIBCMT, XAPILIB, XBOXKRNL, XGRAPHC), title id 4541004C ("Harry Potter: POA"). USA XBE: entry 0x111DB,
`.text` 0x11000-0x264C64, library sections D3D 0x264C80, D3DX 0x278A20, XGRPH 0x295480, DSOUND 0x2B66E0, XPP
0x2BF920 (no XMV: movies are EA's own `.mad` player). Guest memory is mapped at host = guest + 0x10000; the 64 MB
contiguous window at 0x80000000.
D3D: g_pDevice 0x275FE8, CDevice 0x275FF0, DirtyFlags 0x275B48, TextureState 0x275B50, RenderState 0x275D50,
g_Stream 0x278490, carried over from hp1 by matched code and unverified, as are the CDevice field offsets (see
docs/bringup.md). Frame timing follows the video standard (25 fps PAL, 30 fps NTSC).

### 6.2 Overrides (hand-written code over recompiled functions)

| File | What it does |
|---|---|
| `scda/xdk/hle_d3d8.cpp` | the XDK's D3D8 entry points onto the D3D12 backend; Swap, frame pacing, captures (a few unbound, docs/bringup.md) |
| `hle_dsound.c` | DirectSound on XAudio2; **off by default** (`RECOMP_HLE_DSOUND=1`) until all its entry points are bound |
| `hle_video.c` | display timing for the HLE device (vblank overrides unbound for PoA); the renderer's screen hooks (answer 0) |
| `hle_input.c` | XInput onto the PC pad (`app/input.cpp`: remap, deadzones, keyboard and mouse); rumble forwarded on change only |
| `hle_xapi.c` | XAPI disc authentication (unbound for PoA) |
| `hle_skip.c` | XAPI's QueryPerformanceCounter (the game clock) and the cutscene fast-forward (no PoA cutscene state yet) |
| `hle_trainer.c` | the debug trainer's game side: input injection, debug report |

Every address in these files must be a PoA function start: `tools/check_no_foreign_addrs.py` (run by regen.sh)
fails otherwise. Addresses from another title are moved with `tools/xdk_port` (see its README).

### 6.3 Renderer notes (scda/gpu/d3d12.cpp)

* Depth: W-buffered draws (ZEnable 2) write `log2(1+w)/32` (`/20` saturated ~75 units ahead); others plain z.
* Texture borders are emulated in the shader (`border2d`) - vkd3d/D3D12 border colours came out black.
* No-alpha console formats (R5G6B5, X8R8G8B8...) read alpha 1 in the shader (`kTexOpaque`).
* `D3D_BlockOnNonSurfaceResource` (0x196140, HLE in hle_d3d8.cpp) waits for the GPU (`WaitResource`, render
  thread): without it the CPU-skinned characters' ring vertex buffer was overwritten while still being drawn
  (missing/flickering NPCs).
* 16:9 (`Squeeze2D`, `WideFrame`, `WideCrop`): the 3D camera is widened; 2D keeps 4:3 shape - HUD in the left/right
  thirds anchors to the screen edges, centred 2D stays centred, rotated quads ("markers", the aiming circle) keep
  their own middle; cutscenes (letterbox bars) are shown zoomed to the original 4:3 framing with subtitles mapped
  into the band; HUD is hidden in cutscenes. Menu cameras (2.4/3.2) stay 4:3 and are tagged.
* F12 / Insert writes a capture (see 7.1).

### 6.4 Threads and kernel

Each guest thread gets its own KTHREAD copy and id (the game keys per-thread heap context on
`GetCurrentThreadId`); `RECOMP_HEAP_RECLAIM=1` is needed for New Game (contiguous arena). Saves: `E:\UDATA` /
`E:\TDATA` → `<exe dir>\saves` (`src/kernel/kernel_path.c`). Save files carry an HMAC-SHA1 signature (key: 16 zero
bytes) over the block after the 24-byte header.


## 7. Debugging tools

### 7.1 Keys and captures

* **F10** options panel (also Alt+Enter windowed/borderless).
* **F12 or Insert** captures the next frame to `captures\<HHMMSS>\`: `final.bmp`, `frame.txt` (every draw: state,
  textures, shaders, 16:9 squeeze), textures. `RECOMP_CAP_DRAWS=1` also saves the target before every draw;
  `RECOMP_CAP_VERTS=<tex addr hex>` dumps the vertices of draws using that texture;
  `RECOMP_CAPTURE_AFTER=<s>[:<every>:<count>]` captures unattended.
* `RECOMP_DEBUG_KEYS=1`: F8 log depth for all draws, F9 alpha test off, F7 depth test always passes.
  F6 (non-release builds) moves Harry to the camera.

### 7.2 Trainer and reports

`hppoa_trainer.exe` (beside hppoa.exe, either started first) talks to the game through the shared mapping
`Local\hppoa_trainer` (`trainer_shm.h`). For PoA only input injection and the report work so far; its other pages
are still Philosopher's Stone's and do nothing. **Ctrl+Shift+R** writes `reports\<date_time>\`: report.txt, guest
memory images, logs, settings, all thread stacks and a small state file (from a separate thread, so a hung game
still answers), plus a capture. `tools/memtool` (hppoa_mem.exe) reads/writes guest memory of a running game.

### 7.3 Traces (debug.env)

`RECOMP_DS3D_TRACE`,
`RECOMP_DSOUND_TRACE` (`RECOMP_DSOUND_WAV` records the mix), `RECOMP_XMV_TRACE`, `RECOMP_D3D_TRACE`,
`RECOMP_PAD_TRACE`, `RECOMP_RUMBLE_TRACE`, `RECOMP_VSC_TRACE`, `RECOMP_WATCH`, `RECOMP_PROFILE` (sampling
profiler). F11 writes every thread's native stack plus held guest locks and I/O queues to `stacks.txt`
(symbolise with `nm -n hppoa.exe`).

### 7.4 Difftest

`-DHPPOA_DIFFTEST=ON` (own build folder) builds a hppoa.exe that runs each called function's **original x86** in
Unicorn from the live state, then the lifted C, and compares registers, x87 and memory; needs `unicorn.dll`
(Unicorn Engine 2.x, x64) beside it. F5 starts/stops a sweep, `RECOMP_DIFF_AUTO=start_s,len_s` runs one,
`RECOMP_DIFF_ONLY=<addr,...>` restricts it, `RECOMP_DIFF_SAMPLES` / `RECOMP_DIFF_STRIDE` thin it; results in
`difftest.txt`. Known noise (from Philosopher's Stone; PoA's not collected yet): the rdtsc timer family, the FPU
control word's IC bit, recursive functions. Don't combine a
sweep with captures (it once overloaded the GPU driver).

### 7.5 Game facts for debugging

None proven yet for PoA. Formats and data: [docs/re-notes.md](../docs/re-notes.md); library state carried from hp1:
[docs/bringup.md](../docs/bringup.md).


## 8. Working rules

* **Translate faithfully.** Bridge what the game asks for with the console's semantics (e.g. rumble speeds persist
  until changed, so forward changes only). No keep-alives, synthetic events or hacks that mask a host quirk; if the
  host misbehaves, say so and fix it there.
* **Debug information first, then other recomps.** Gather captures, logs, traces, stacks, difftest results; then
  look for the same XDK function / lifter bug / renderer behaviour in other original-Xbox recomps
  (SCDA-XBERecomp, limeylicious/Nightfire-Windows-Port, DOAXBV, DefJam FFNY, Black, ...) and port a proven fix;
  move on quickly if one has nothing. Only then derive a fix from first principles. Check original-hardware
  behaviour before "fixing" something: several reports (slow doors, eyelids in the Quidditch cutscenes, crab range,
  hog flames) turned out to be how the Xbox game behaves.
* **One test run should answer everything.** Make traces independent of how the game is started (debug.env),
  self-test new instrumentation, trigger captures automatically, and collect every open question in one run.
* Never name a local variable after an x86 register in hand-written override files (`recomp_types.h` defines
  `esp`, `eax`, ... in generated code).
* Saves stay in `<exe dir>\saves`, per game - never in the game folder or a shared folder.
* Don't delete whole output folders (dist/, run/); overwrite your own files.
* Nothing from the game goes into the repository (game files, gen/, tools/*/output/, captures, reports, art).


## 9. Open issues

* The game has not been run yet: [docs/bringup.md](../docs/bringup.md) lists what is unbound or unverified.
* **Controllers:** only XInput pads (Xbox and XInput-mode pads). PS4/PS5/generic pads need Steam Input or
  DS4Windows; the toolkit's SDL2 backend (`src/input/xinput_device.c`, non-Windows today) could be enabled on
  Windows, statically linked.
