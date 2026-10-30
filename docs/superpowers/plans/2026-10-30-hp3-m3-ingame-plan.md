# hp3_recomp M3 (in game) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** On the USA disc, New Game goes from the title into the first level and it plays at the right speed:
Harry moves with the pad and with keyboard/mouse, and a save made in the game lands in `<exe dir>\saves` and loads
back after a restart. M2's leftovers are closed: the title screen's sound, immediate mode, the debug-menu key and
the review follow-ups.

**Architecture:** M2 ends at the front end. This plan first gives Claude a way to drive its own unattended runs past
the title: a PoA copy of the siblings' shared-memory pad injector (`poatrainer.py pad`, the same trainer block v5
that `hle_trainer.c` already serves). Then it adds immediate mode to the D3D HLE. A baseline run goes New Game ->
first level, and a fix loop like M2 Task 9 follows: static analysis first, one run per hypothesis set. Then come the
game speed check, saves, the title sound and the debug-menu key. The user's play-test closes the milestone.

**Tech Stack:** MSYS2 UCRT64 GCC + CMake + Ninja; Python 3 + capstone (`py -3 <test>.py` self-checks like
`tools/xdk_port/test_match.py`); PowerShell for runs; gdb in UCRT64 for stalls.

**Spec:** `docs/superpowers/specs/2026-10-30-hp3-port-design.md` (M3 row of section 7: "New Game reaches the first
level; control, save/load to `<exe dir>\saves` work"; goals in section 2, risks in section 8). Bring-up input:
`docs/bringup.md` (from "## M2 runs" on), `docs/re-notes.md`, `docs/superpowers/HANDOFF-2026-10-30-m3-start.md`.

## Global Constraints

- Repo `C:\Users\unowi\DecompProject\hp3_recomp`. Work on branch `m3-ingame` from `main` (1be1f79) and merge it
  locally at the end. **Never push** without asking (the first push to GitLab barthes_recompiled/hp3_recomp is the
  user's call).
- Canonical disc = USA, `default.xbe` SHA-1 `51b09e01169fda86f2cb7e5ce45ee80a10268918`. EUR is M4.
- Names: `hppoa.exe`, `hppoa_launcher.exe`, `hppoa_trainer.exe`, `hppoa.toml`, `HPPOA_LAUNCHER=1`, shared block
  `Local\hppoa_trainer` (magic `0x52545048`, version 5), CMake prefix `HPPOA_`.
- Generated code is never hand-edited. Overrides go in `port/src/hle_*.c` / `port/src/scda/xdk/hle_d3d8.cpp`; hooks
  inside a generated function go in `port/tools/patch_gen.py`. A new override file needs a CMakeLists entry, regen.sh
  `--exclude-manual`, and an `extern void sub_X_gen(void);` line (the generator keeps only `_gen` bodies it sees
  declared).
- Every address in hand-written code is a PoA function start or a PoA-referenced literal
  (`port/tools/check_no_foreign_addrs.py`, run by regen.sh).
- Nothing from the game in git: no captures, logs, WAVs, save files, decoded APT bytecode or `tools/*/output/`.
- Working rules: `port/DEVELOPING.md` section 8 verbatim: translate faithfully, no hacks over host quirks. Check debug
  data and other recomps first. One run should answer every open question. Compare with the console before "fixing"
  odd behaviour. Saves go in `<exe dir>\saves`.
- **Static first:** every run tests a hypothesis written down beforehand in `docs/bringup.md` "## M3 runs". Runs are
  short (PC heat: 180 s unless a step says otherwise) and closed afterwards. Announce each run to the user before
  starting it (it opens on their screen).
- **Before each run:** `Get-Process` shows no `hppoa`, `hpps`, `hpcos` or `crackdown*` process (another session's
  game holds the PC, and the one-instance lock). If one is running, wait; never close it. Kill only your own PID,
  never `hpctl stop` (the shared `hpctl.pid` may belong to another session).
- Test folder `C:\Users\unowi\DecompProject\Harry Potter Xbox Decomps\test\PoA_USA` (exes, `game` junction,
  `hppoa.toml` with `debug_logs = true`, `debug.env`). Recopy the exes after every rebuild. Set `HPPOA_LAUNCHER=1`
  yourself. Scripted input (`poatrainer.py pad`) is for Claude's own unattended runs only, never the user's.
- Build (PowerShell; `-j4` for heat):
  `$env:MSYSTEM="UCRT64"; C:\msys64\usr\bin\bash.exe -lc "cd /c/Users/unowi/DecompProject/hp3_recomp/port && cmake --build build-win -j4"`
  (add `bash regen.sh &&` before cmake when an override set or `patch_gen.py` changed).
- Bulk reading (generated code, long logs, diffs, APT bytecode) goes to DeepCode from the repo folder:
  `deepcode -x -p "<one line>"`. The prompt is ONE line and starts with "Do NOT use bash or any shell command, do NOT
  use web search or any network tool, and do NOT read any path outside the current folder; use only file
  read/list/search tools". Big inputs go in `.superpowers/`. Run it in the background and verify every finding
  against the code (`git merge-base --is-ancestor` tells new from pre-existing). Claude Code is the only writer.
- Randomizer-minded RE (spec 6.7): pickups, collectibles (beans, cards, challenges), spells, `.hpb` script loading
  met on the way are recorded in `docs/re-notes.md` with guest addresses and evidence.
- Commits end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. **No controller connected when New Game starts, or the pad unplugged in the level.** Expected: the game's own
   "reconnect controller" prompt or pause, no hang, play resumes when the pad comes back. Pinned by Task 5 Step 3
   (the exit run starts with no pad, and the injector stands in) and Task 10 Step 2 (the user unplugs once).
2. **First run with no `saves` folder, or an empty one.** Expected: Load Game shows no saves (no crash, no endless
   "checking storage"), and the first save creates `saves\UDATA\<title id>\...` beside the exe. Nothing appears under
   `%LOCALAPPDATA%\xboxrecomp`. Pinned by Task 7 Steps 2-4.
3. **Game speed at a 60 Hz present.** PoA's frame timer has a 33 ms minimum delta (`sub_001A7450`, 0x001A74A0). If
   that floor becomes the time step while frames come every 16.7 ms, everything runs twice as fast. Expected: in-game
   time runs at wall-clock speed. Pinned by Task 6.
4. **Going back to the title from the pause menu, then New Game again in the same process** (heap reuse,
   `RECOMP_HEAP_RECLAIM`, level unload). Expected: the second load reaches the level like the first. Pinned by
   Task 5 Step 3.
5. **Keyboard and mouse only, no pad.** Expected: menus navigate, Harry walks and the camera turns with the mouse,
   through the same `app_InputPad` mapping as hp1. Pinned by Task 10 Step 2 (the user plays both ways).

---

### Task 1: xdk_port refuses two donors on one target

**Files:**
- Modify: `tools/xdk_port/match.py` (unique-body pass, `matched[a] = at[0]`)
- Test: `tools/xdk_port/test_match.py`

**Interfaces:**
- Produces: the unique-body pass leaves a PoA address unbound and reports it as ambiguous when more than one donor
  maps to it. Existing outputs and callers are unchanged.

- [ ] **Step 1: Write the failing test** `test_unique_body_shared_target_is_ambiguous`: two donor functions with
  byte-identical bodies (different names) and one PoA function with that body. Assert the PoA address is in neither
  donor's `matched` entry and appears in the ambiguous list, naming both donors. Also add
  `test_unique_body_single_donor_still_binds` (one donor, one target -> bound), which pins the old behaviour.
- [ ] **Step 2:** `py -3 tools/xdk_port/test_match.py` -> the first new test FAILS (the target is bound to one donor).
- [ ] **Step 3:** In the unique-body pass, count the donors per target and demote any target that has more than one
  donor to ambiguous.
- [ ] **Step 4:** `py -3 tools/xdk_port/test_match.py` -> all pass. Rerun the D3D/DSOUND binding report
  (`port/regen.sh`) and diff the bound set against main's. Expected: identical (the bug was never hit in PoA). Any
  difference is a real collision: list it in `docs/bringup.md` "M2 review follow-ups" and stop to look at it.
- [ ] **Step 5: Commit** `xdk_port: two donors on one target are ambiguous` (tick the follow-up in bringup.md).

### Task 2: Kernel timer lock and thread start at kernel init

**Files:**
- Modify: `src/kernel/kernel_bridge.c` (`kernel_set_timer` lazy init of `g_timer_lock` + timer thread)
- Modify: the kernel init that runs before any guest thread (`xbox_kernel_init`, called from `port/src/main.c`)

**Interfaces:**
- Produces: `g_timer_lock` and the runtime timer thread exist before the first guest instruction. `kernel_set_timer`
  no longer initialises anything.

- [ ] **Step 1:** Move the lock init and thread start into a `static void timer_init(void)` called once from kernel
  init, before the bridge starts the title thread. Keep 3e7e884's one-step reset + arm under the lock.
- [ ] **Step 2:** Build, then run the existing toolkit self-checks (`ctest` in `port/build-win` if configured,
  otherwise the build alone) -> pass.
- [ ] **Step 3:** One 120 s run (announce; nothing pressed) -> logos, intro, title, attract as in M2's conf runs: the
  log opens `eac_usa.mad`, `wb.mad`, `intro`, has `[FRAME]` lines and no stall. Record it in "## M3 runs".
- [ ] **Step 4: Commit** `kernel: timer lock and thread start at kernel init` with the run name.

### Task 3: Unattended pad input for PoA

**Files:**
- Create: `C:\Users\unowi\DecompProject\Harry Potter Xbox Decomps\tools\poatrainer.py` (outside the repo, beside
  its siblings `pstrainer.py` / `costrainer.py`, same structure)
- Create: `C:\Users\unowi\DecompProject\Harry Potter Xbox Decomps\tools\poa_shm_offsets.c` (layout check helper)
- Modify: `port/DEVELOPING.md` (debugging tools section: one paragraph on `poatrainer.py`)

**Interfaces:**
- Produces: `py -3 poatrainer.py pad <buttons hex> [ms] [analog a,b,...] [stick lx,ly,rx,ry]` (the pstrainer
  argument order; analog index 0 = A, Start = `0x0010`, Back = `0x0020`, D-pad up/down/left/right =
  `0x0001/0x0002/0x0004/0x0008`); `py -3 poatrainer.py peek <guest hex> [n]` (n dwords);
  `py -3 poatrainer.py selftest <offsets.txt>`. It exits non-zero with a message when the block is missing or the
  magic or version doesn't match.

- [ ] **Step 1:** `poa_shm_offsets.c` includes `port/src/trainer_shm.h` and prints `name offset` lines for `magic`,
  `version`, `mem_offset`, `inject_seq`, `inject_ms`, `inject_buttons`, `inject_analog`, `inject_stick` and `sizeof`.
  Build it with UCRT64 gcc and write `offsets.txt` to the scratchpad.
- [ ] **Step 2:** Write `poatrainer.py` with the ctypes struct copied from `port/src/trainer_shm.h` (not from
  pstrainer: the two may drift). Block name `Local\hppoa_trainer`.
- [ ] **Step 3:** `py -3 poatrainer.py selftest offsets.txt` -> `layout OK` (every offset and the size equal).
- [ ] **Step 4:** One 150 s run (announce): at 70 s (title) `pad 0010 200`, at 75 s `pad 0000 200 255` (A).
  Expected: the log has `[TRAINER] input 0010 ...` and `... analog FF...` lines, and a capture at 80 s shows the
  front-end menu (New Game / Load Game) instead of the attract movie. Record it in "## M3 runs".
- [ ] **Step 5: Commit** (repo side only) `docs: poatrainer.py for unattended runs`.

### Task 4: Immediate mode in the D3D HLE

**Files:**
- Modify: `port/src/scda/xdk/hle_d3d8.cpp` (stubs at 1366-1372: `D3DDevice_Begin` 0x00269B20, `D3DDevice_End`
  0x00269B60, `SetVertexData2f` 0x00269A80; add `SetVertexData4f` 0x00269AC0)
- Modify: `docs/bringup.md` ("Renderer gaps"; tick SetVertexData4f off the D3D coverage list)

**What uses it (DeepCode read, 2026-10-30; check the addresses before relying on them):** one effect object
(constructor `sub_0004B190`; 32 slots of 0x2500 bytes at `this+0x140`, count at `+0x4A1C4`, three 32x32 texture
wrappers at `+0x4A1C8` / `+0x4A210` / `+0x4A258` with surfaces at wrapper+0x30). Its frame renderer `sub_0004A3C0`
returns early while `[0x313A6C] != 0` and otherwise calls:
- `sub_00049A70`: once (guard byte `+0x4A2A4`, reset by `[0x2FA894]`). Into the `+0x4A288` target with FVF `0x42`,
  it draws `Begin(6)`, a 6-vertex triangle strip (a vertical alpha gradient), and `Begin(4)`, a 2-vertex
  transparent line strip. These are M2's two start-up batches (call sites 0x00049DD4 / 0x00049EF8).
- `sub_00049F70`: every time the effect renders. Into the `+0x4A240` target with FVF `0x144`, it draws two 4-vertex
  strips (a 32x32 copy each of `+0x4A1C8` and `+0x4A210`). The per-slot draw `sub_00043C00` then samples all three
  textures.
M2's boot never got past the early return, so this effect is a level-side feature (likely trails or a spell effect).
Without immediate mode it would draw from three never-filled textures. Name the effect when Task 5 meets it.

**Interfaces:**
- Consumes: the `DrawVerticesUP` path (`hle_d3d8.cpp:1305-1340`: copy into the UP ring, `g_backend->Draw`), and
  `gpu::FvfInputs` (`scda/gpu/vertex.cpp:406-435`) for the layout. `gpu::Convert` (`vertex.cpp:20-76`) defines the
  vertex types.
- Produces: `D3DDevice_Begin(prim)` starts a list. `SetVertexData2f/4f(reg, ...)` set register `reg` (D3DVSDE_DIFFUSE
  = 3, D3DVSDE_TEXCOORD0 = 9, ...; 2f fills z = 0, w = 1). Register `0xFFFFFFFF` (D3DVSDE_VERTEX) writes v0 and
  emits one vertex carrying every register set so far. `D3DDevice_End()` packs the list in the current stream-0
  layout (FVF via `FvfInputs`, or the declared vertex shader when `vs_handle & 1`) and issues one draw. The Xbox
  primitive value passes through unchanged (`gpu::Assemble` already handles strips, fans and quad lists).

- [ ] **Step 1:** Record the reads above in bringup.md "Renderer gaps", after spot-checking the Begin arguments,
  `SetVertexShader` values and target fields in `gen/` at the addresses given.
- [ ] **Step 2:** Implement it in `hle_d3d8.cpp` beside `DrawVerticesUP`:
  - Bind `SetVertexData4f` 0x00269AC0 (5 args; add its `extern void sub_00269AC0_gen(void);` line).
  - Add `static void StoreVertexType(uint32_t type, const float v[4], uint8_t* out)`, the inverse of `gpu::Convert`:
    float types `0x12/0x22/0x32/0x42` copy 1-4 floats; D3DCOLOR `0x40` packs clamped B,G,R,A; `0x72` is x,y,w. Any
    other type gets `LOG_FIRST` and a skipped draw, never raw floats.
  - Copy into the UP ring with a host-source variant of `UpVertices`, keeping its fence bookkeeping.
  - Call `SyncDeferred()` before the draw, restore `streams[0]` after it, and set `caller` = Begin's return address.
  - Cap the list at a fixed size and log once if it overflows.
- [ ] **Step 3:** `check_hle_coverage.py --strict --section D3D --section DSOUND` -> 0 gaps.
- [ ] **Step 4:** One 30 s run with `RECOMP_CAP_DRAWS=1` over the start-up pass (announce). Expected: two draws with
  callers 0x00049DD4 / 0x00049EF8 in `frame.txt` (6-vertex strip and 2-vertex line, 16-byte stride), the saved
  `+0x4A288` target shows the gradient, there is no `not supported` line, and the boot to the title is unchanged.
- [ ] **Step 5: Commit** `d3d: Begin/End immediate mode` with the run name.

### Task 5: New Game -> first level (baseline + fix loop)

**Files:**
- Modify: whatever each fix needs (overrides, `icall_seeds.json`, `port/tools/patch_gen.py`, kernel), `docs/bringup.md`

- [ ] **Step 1: Baseline run** (announce; 240 s, `RECOMP_CAPTURE_AFTER=60:10:18`, `RECOMP_DSOUND_TRACE=1`; check that
  `RECOMP_HEAP_RECLAIM=1` is set by the launcher env or `debug.env`). Drive it with `poatrainer.py`: Start on the
  title, A on New Game, A through every confirmation and profile/slot screen the captures show, Start/A to skip the
  opening cutscene. Then record in "## M3 runs" how far it got: the last file opened, the last capture, `[CRASH]`,
  `ICALL`, `MakeSpace from`, `unknown handle`, `UNIMPL` and stall counts, and gdb `thread apply all bt 25` if it
  stops logging.
- [ ] **Step 2: Fix loop.** Use superpowers:systematic-debugging for each blocker: static analysis first (DeepCode
  for generated-code sweeps), then other original-Xbox recomps (hp1, hp2, xemu source), then a fix. Each fix is its
  own commit citing the log line, capture or stack it answers.
- [ ] **Step 3: Exit run** (announce; 300 s, no physical pad, the injector only). Expected:
  - New Game -> the first level's gameplay: Harry is on screen and a 3 s left-stick push (`pad 0000 3000 - 0,32767,0,0`)
    moves him, which the captures before and after show.
  - Start opens the pause menu. Quitting to the title, then New Game again, reaches the level a second time.
  - Zero `[CRASH]`, `ICALL` failures, `MakeSpace from` and `unknown handle` lines.
  - Any "controller disconnected" prompt is the game's own and goes away once the injector sends input.
  - If the immediate-mode effect (Task 4, `sub_00049F70`) draws in the level: one capture shows its 32x32 copies in
    `+0x4A240` and the main target restored afterwards (`sub_0002DA40`). If it isn't restored, look at how the
    `SetRenderTarget` HLE (`hle_d3d8.cpp:648-668`) and the game's own current-target field (`[0x30F948]` +0x2E14)
    disagree before changing anything.
  Name the first level's files (`.pkg`, `.hpb`) and its loader entry in re-notes.md.
- [ ] **Step 4: Commit** `M3: New Game reaches the first level` with the run summary.

### Task 6: Game speed at a 60 Hz present

**Files:**
- Modify (only if Step 2 says so): the override or `patch_gen.py` hook for the frame-time floor (`sub_00096520`
  sets it at renderer init; `sub_001A7450` applies it at 0x001A74A0)
- Modify: `docs/bringup.md`

- [ ] **Step 1: Static:** read `sub_001A7450` and its caller `sub_00026A50`. Does the floor make the loop *wait*
  until 33 ms have passed (the simulation then runs at 30 Hz and each frame is presented twice), or does it *report*
  33 ms as the step (the game then runs fast at 60 Hz)? Write the answer with addresses in bringup.md.
- [ ] **Step 2: Measure** in Task 5's exit run (or one 120 s run in the level, announced): read the game clock
  0x3836C4 with `poatrainer.py peek` twice, 60 s of wall time apart, plus the in-game step count per second if Step 1
  found one. Expected for "right speed": the in-game time advances 60 s ± 2 %.
- [ ] **Step 3: Decide:**
  - If the game runs at the right speed (whatever its sim rate), record it and stop. A true 60 Hz simulation is the
    v0.1.0 pacing work and gets its own plan after M3.
  - If it runs fast or slow, fix the floor so it is honest at the present rate (the console's behaviour at 30 Hz is
    the reference) and repeat Step 2.
- [ ] **Step 4: Commit** `docs: PoA game speed at 60 Hz` (or `timing: ...` with the fix) with the measurement.

### Task 7: Saves in `<exe dir>\saves`

**Files:**
- Modify: whatever the save path needs (`src/kernel/kernel_path.c`, `src/kernel/kernel_file.c`, XAPI override), `docs/re-notes.md`

- [ ] **Step 1: Static** (DeepCode): how PoA saves. Its save strings (`UDATA`, `SaveMeta.xbx`, `SaveImage.xbx`, the
  save name), the drive it opens (`U:` through XAPI's recorded link, or `E:\UDATA` directly), whether it signs saves
  (XCalculateSignature with the XBE cert's key), and where in the game a save happens (autosave at a checkpoint, the
  pause menu, or a save book). Record the title ID (from the XBE certificate), the save folder and file layout, and
  the save trigger in re-notes.md "## Saves" (randomizer-relevant: the save holds progress).
- [ ] **Step 2:** Rename `saves\` in the test folder to `saves.m3bak` (keep it; restore it at the end), and check that
  `%LOCALAPPDATA%\xboxrecomp` has no PoA folder (note what is there and leave it alone).
- [ ] **Step 3: Run** (announce; 240 s, injector): title -> Load Game first (expected: the game's own "no saves"
  state, no crash, no endless storage check) -> back -> New Game -> the first save the Step 1 trigger allows -> quit.
  Expected: `saves\UDATA\<title id>\<save>\` beside the exe holds the files Step 1 names. Nothing new appears under
  `%LOCALAPPDATA%\xboxrecomp`.
- [ ] **Step 4: Run** (announce; 150 s): Load Game -> that save -> the level resumes where it was saved (capture).
- [ ] **Step 5:** If either run failed, fix it with superpowers:systematic-debugging, each fix its own commit. Then
  commit `M3: saves in <exe dir>\saves` with both runs. Restore the test folder's `saves` from `saves.m3bak` if the
  user had anything there.

### Task 8: Title-screen sound ("I solemnly swear") and Play after Pause

The user (2026-10-30): on the console, the title plays the "I solemnly swear I am up to no good" line and then moves
on to the New Game / Load Game screen, which has music. Ours: RMS 0 on the title (M2 run 10, 60 s and 140 s
windows), music in the front-end menu.

**Files:**
- Modify: whatever the fix needs (`port/src/hle_dsound.c`, an override), `docs/bringup.md`

- [ ] **Step 1: Static** (DeepCode): what the title's APT movie (`poa_fe.big` / `poa_startup_xbox.big`) asks for
  when "Press START" shows (FSCommand names: speech/sound/music requests). Name the handler for each, and what the
  handler loads (`frontendspeech.pkg` / `.pkgA`, an `.sdt`, `<Music>fe.mpf`). Note any handler that returns early
  or fails a file open.
- [ ] **Step 2:** From M2 run 10's log (`RECOMP_DSOUND_TRACE` was off; check), or one 90 s run with
  `RECOMP_DSOUND_TRACE=1` (announce): are buffers for the line created, played and fed (silent data), or never
  created (the request dies before DSOUND)? Write the hypothesis and result in "## M3 runs".
- [ ] **Step 3: Play after Pause** (M2 review follow-up): read PoA's own XDK 5849 `CDirectSoundBuffer::Play` body
  (the guest code the HLE replaces) for what it does to a paused buffer. If XDK Play resumes a paused buffer, make
  `hle_dsound.c` `Play` clear `paused`; if not, record that and leave it. Either way, tick the follow-up.
- [ ] **Step 4:** Fix, then one 90 s run (announce): the line plays on the title (audible in `RECOMP_DSOUND_WAV`,
  RMS > 0 in the title window), then the front-end music as before.
- [ ] **Step 5: Commit** `dsound: ...` / `audio: title speech` with the run name.

### Task 9: Debug-menu key (time-boxed: one decode, one run)

**Files:**
- Modify: `docs/re-notes.md` ("Debug menu"), `port/DEVELOPING.md` (how to open it), `port/src/hle_debug.c` only
  if the gate needs more than the variable

- [ ] **Step 1: Static** (DeepCode, inputs in `.superpowers/`, not committed): decode the APT ActionScript key
  handler near file offset 0x12394 of `poa_startup_xbox.big` and 0x64EB4 of `poa_fe.big`. Which `Key.getCode` value
  opens `POA_DEBUGMENU.swf`, and how does PoA map pad buttons to APT key codes (find the code that posts key events
  to APT)?
- [ ] **Step 2: One run** (announce; `debug_menu = true`, injector): press that button on the title. Expected: the
  CHAPTER/ROOM SELECTION screen ("Build: ..., May 8 2004"). Choose a chapter and room, and check that 0x376B44/48/4C
  change (`poatrainer.py peek 376B44 3`) and that the chosen room loads.
- [ ] **Step 3:** Whether it works or not, record it and stop (no second run). If it works, it is M4's level-warp tool
  and a randomizer entry point (re-notes). Commit `docs: PoA debug menu key`.

### Task 10: User check, review and merge

- [ ] **Step 1:** Copy the current exes to `test\PoA_USA` (check that no game is running there first) and ask the user
  to play with `hppoa_launcher.exe`.
- [ ] **Step 2:** The user checks, with their own pad and then keyboard + mouse:
  - The title's "I solemnly swear" line, then New Game, then the first level.
  - Harry walks, the camera turns and the menus navigate both ways.
  - Unplug the pad once in the level (expected: the game's own prompt), then plug it back in.
  - Save, quit to desktop with Alt+F4 (Quit To Desktop is M5), restart, Load Game.
  - Does the game speed look right?
  Record the answers in bringup.md.
- [ ] **Step 3:** Fix what the user reports (fix loop as Task 5 Step 2), then ask them to check again.
- [ ] **Step 4:** Whole-branch review by DeepCode (diff `main...m3-ingame` in `.superpowers/m3-review.diff`, one-line
  prompt, background). Verify each finding. Fix the new ones, and list the pre-existing ones as "M3 review
  follow-ups" in bringup.md.
- [ ] **Step 5:** Merge `m3-ingame` to main locally (`--no-ff`, message `Merge m3-ingame: New Game reaches the first
  level`). Write `docs/superpowers/HANDOFF-<date>-m4-start.md` (state, open items, the 60 Hz pacing plan as the next
  candidate next to M4). Ask the user before any push.

## Not in M3 (carried)

- True 60 Hz game logic, if Task 6 finds a 30 Hz simulation (own plan; v0.1.0 bar).
- Start to finish on USA, EUR via retarget (M4). Intro/cutscene skip, Quit To Desktop, 16:9 Hor+ and the 2D
  handling (`hppoa_MenuMovie` / `LoadingScreen` / `TextIcon` return 0 today), the trainer's PS-shaped pages (M5).
