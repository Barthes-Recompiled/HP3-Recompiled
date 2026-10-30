# hp3_recomp M2 (boot to the title screen) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `hppoa.exe` on the USA disc plays (or skips) the EA/WB logos and reaches the title screen and front-end
menu with correct rendering and audio, with every D3D8/DSOUND entry point the title uses bound to the HLE.

**Architecture:** M1 left about 60 hp1 overrides unbound (`#if 0` in place, listed in `docs/bringup.md`). This plan
first measures what the title actually reaches: a baseline run, plus a static coverage check over the generated code.
It then binds what is missing with two new, evidence-only matching sources in `tools/xdk_port`: callee anchoring
(tiny wrappers that differ only in what they call) and XbSymbolDatabase's XDK 5849 signatures. Then it ports hp2's
vertex shader entry points, turns DirectSound HLE on, and runs a boot-fix loop until the title screen is clean. The
renderer already translates vertex shader programs (`scda/gpu/shaders.cpp`, the same as hp2's). Only the D3D entry
points that feed it are missing.

**Tech Stack:** MSYS2 UCRT64 GCC + CMake + Ninja; Python 3 + capstone (tools, standalone `py -3 <test>.py`
self-checks like `tools/xdk_port/test_match.py`); XbSymbolDatabase (local clone, read as data, not vendored).

**Spec:** `docs/superpowers/specs/2026-10-30-hp3-port-design.md` (M2 row of section 7; risks in section 8).
Bring-up input: `docs/bringup.md`, `docs/re-notes.md`.

## Global Constraints

- Repo `C:\Users\unowi\DecompProject\hp3_recomp`. Work on branch `m2-boot` from `main` (b8d14a4) and merge it locally
  at the end. **Never push** without asking (origin is still empty; the first push is the user's call).
- Canonical disc = USA, `default.xbe` SHA-1 `51b09e01169fda86f2cb7e5ce45ee80a10268918`. EUR is M4.
- Names: `hppoa.exe`, `hppoa_launcher.exe`, `hppoa_trainer.exe`, `hppoa.toml`, `HPPOA_LAUNCHER=1`, CMake prefix `HPPOA_`.
- Generated code is never hand-edited. Overrides go in `port/src/hle_*.c` / `port/src/scda/xdk/hle_d3d8.cpp`. Hooks
  inside a generated function go in `port/tools/patch_gen.py`. New override files go in regen.sh `--exclude-manual`.
- Every address in hand-written code is a PoA function start or a PoA-referenced literal
  (`port/tools/check_no_foreign_addrs.py`, run by regen.sh). A binding that is not a unique body match carries an
  `// xdk_port: <how> match` comment, as M1's do.
- Nothing from the game in git: no addresses-only maps (`tools/xdk_port/output/`), captures, logs, WAVs or
  XbSymbolDatabase files.
- Working rules: `port/DEVELOPING.md` section 8 verbatim (translate faithfully; debug data and other recomps first;
  one run answers everything; saves in `<exe dir>\saves`).
- Test runs: test folder `C:\Users\unowi\DecompProject\Harry Potter Xbox Decomps\test\PoA_USA` (exes, `game` junction
  to `hp3_recomp\game`, `hppoa.toml` with `debug_logs = true`, `debug.env`). Announce each run before starting it
  (it opens on the user's screen). Set `HPPOA_LAUNCHER=1` yourself. Kill only your own PID, never `hpctl stop`. Never
  drive the user's runs with scripted input.
- Build (PowerShell):
  `$env:MSYSTEM="UCRT64"; C:\msys64\usr\bin\bash.exe -lc "cd /c/Users/unowi/DecompProject/hp3_recomp/port && bash regen.sh && cmake --build build-win -j$(nproc)"`
- Delegate bulk reading (generated-code sweeps, long logs, XbSymbolDatabase layout) to Codex / agy read-only, or to
  an Explore subagent when they are not reachable. Claude Code is the only writer.
- Before Task 1 and before Task 7, run `git ls-remote https://github.com/MiddleDucky/HP-POA-Xbox-Recomp`. If it is no
  longer empty, stop and tell the user (they may want to coordinate or rebase on it).
- Commits end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. **A new matcher binds the wrong function** (callee anchoring or a signature hit on a look-alike). Expected: only
   unique results are accepted. Where both sources name a function they must agree, and a disagreement leaves it
   unbound and listed. Pinned by Task 3's `test_callee_anchor_*`, Task 4's `test_oovpa_*`, and Task 4 Step 6.
2. **DirectSound HLE on while some title-called entry point is still unbound** (host mixer and XDK share voices:
   wrong or missing sounds, no crash). Expected: the default only flips once `check_hle_coverage.py --strict
   --section DSOUND` passes. Pinned by Task 7 Step 4.
3. **Indirect calls the static check cannot see** (function pointers and vtables into D3D/DSOUND). Expected:
   address-taken library functions count as called, and a run logs no `d3d: MakeSpace from` line (some caller still
   writes the push buffer). Pinned by Task 2's `test_address_taken_counts` and Task 9 Step 3.
4. **Idling on the title screen** starts the `attract` movie after its timeout. Expected: it plays (YUY2 textured
   quad) and returns to the title with no hang. Pinned by Task 9 Step 2 (the run idles past the timeout).
5. **No controller at boot / logo skip by button.** Expected: the game shows its own prompt or the title and does not
   hang, and a button press during logos skips like on the console. Pinned by Task 9 Step 2 (the unattended run has
   no pad) and Task 11 Step 2 (the user presses through).

---

### Task 1: Test folder and baseline run

**Files:**
- Create: `Harry Potter Xbox Decomps\test\PoA_USA\` (outside the repo: exes copied, `game` junction, `hppoa.toml`,
  `debug.env`)
- Modify: `docs/bringup.md` (new section "## M2 runs")

- [ ] **Step 1:** `git switch -c m2-boot`. Build (Global Constraints command). Expected: links, regen prints
  0 failed functions.
- [ ] **Step 2:** Make the test folder: copy `port/build-win/hppoa*.exe`, run
  `cmd /c mklink /J game C:\Users\unowi\DecompProject\hp3_recomp\game`, write `hppoa.toml` with `debug_logs = true`.
  Write `debug.env` with `RECOMP_D3D_TRACE=1` and `RECOMP_CAPTURE_AFTER=20:20:4` (captures at 20, 40, 60, 80 s).
- [ ] **Step 3:** Announce, then run `hppoa.exe` with `HPPOA_LAUNCHER=1` for 90 s and kill your PID. Expected: some
  outcome is logged (crash, hang, or picture). Collect `hppoa_run.log`, `xbox_kernel.log`, `captures\`.
- [ ] **Step 4:** Under `## M2 runs` in `docs/bringup.md`, record the date, the build commit, how far it got, the first
  failure (log line or `[CRASH]` stack, symbolised), the counts of `MakeSpace from` callers, `ICALL` failures and
  unknown shader handles, and what each capture shows. This is the baseline every later task is measured against.
- [ ] **Step 5:** Commit `M2: baseline boot run on PoA USA`. If the run hit an `ICALL` failure or a crash outside the
  library sections, fix it now under superpowers:systematic-debugging (seed via `icall_seeds.json`, regenerate,
  rerun). Commit each fix with the log line it answers.

### Task 2: Static HLE coverage check

**Files:**
- Create: `port/tools/check_hle_coverage.py`, `port/tools/test_check_hle_coverage.py`, `port/hle_coverage_allow.txt`
- Modify: `port/regen.sh` (run it in report mode after the foreign-address check)

**Interfaces:**
- Consumes: `check_no_foreign_addrs.live_lines(text)` and its `_NAME` regex (import them; don't copy), the section
  table in `game/default_analysis.json` (`sections`: name, va, size).
- Produces: `py -3 port/tools/check_hle_coverage.py --gen port/src/recomp/gen --analysis game/default_analysis.json
  --overrides <files...> --allow port/hle_coverage_allow.txt [--section D3D] [--section DSOUND] [--strict]`. It prints
  one line per gap: `<SECTION> 0x<target> <direct callers> <address-taken sites> <reason>`. Exit 1 only with
  `--strict` and at least one gap. Later tasks run it with `--strict`.

Gap definitions (the plan's decision):
- **called** = a `RECOMP_ABI_CALL(0x<T>u, ...)` inside a `void sub_<C>(void)` body with C in `.text`, or a `0x<T>u`
  literal in `.text` code that equals a function start in the section (address taken).
- **DSOUND gap**: called, not overridden, not allowlisted.
- **D3D gap**: called, not overridden, not allowlisted, and its generated body reaches, through non-overridden D3D
  functions, a call to `D3DDevice_MakeSpace` 0x0026C7F0, `D3D_MakeRequestedSpace` 0x0026C6B0 or
  `D3DDevice_BeginStateBig` 0x0026C800 (it writes hardware commands the host renderer never reads).
- Allowlist lines: `0x<addr> <name>: <reason>`, e.g. functions that only fill CPU-side structures.

- [ ] **Step 1: Write the failing tests** in `test_check_hle_coverage.py` with tiny synthetic gen text and an
  in-memory section table: `test_direct_call_gap`, `test_overridden_not_a_gap`, `test_hle_under_if0_is_a_gap`,
  `test_allowlisted_not_a_gap`, `test_address_taken_counts`, `test_d3d_gap_only_if_reaches_makespace`,
  `test_reach_stops_at_override`, `test_strict_exit_code`.
- [ ] **Step 2:** `py -3 port/tools/test_check_hle_coverage.py` fails (module missing).
- [ ] **Step 3:** Implement `check_hle_coverage.py`.
- [ ] **Step 4:** The tests pass. Then run it on the real tree. Expected: a gap list that includes the 12 unnamed,
  title-called DSOUND entry points from the Codex survey (0x002B752F ... 0x002B8791) and at least the D3D Clear call
  sites. Paste the summary counts into `docs/bringup.md` "## M2 runs".
- [ ] **Step 5:** Hook it into regen.sh after `check_no_foreign_addrs.py`, report mode only (bring-up must keep
  regenerating). Commit `tools: HLE coverage check over the generated code`.

### Task 3: xdk_port callee anchoring

**Files:**
- Modify: `tools/xdk_port/match.py` (`match_functions`), `tools/xdk_port/__main__.py` (report label), `tools/xdk_port/README.md`
- Test: `tools/xdk_port/test_match.py`

**Interfaces:**
- Produces: `match_functions(...)` returns an extra key `"callee": {donor_addr, ...}`, alongside `"anchored"`. Map JSON
  entries gain `"how": "unique" | "anchored" | "callee"`. Override comments use `// xdk_port: callee match`.

Rule: for a donor function still ambiguous after neighbour anchoring, read each candidate's rel32 call/jmp targets
in order and compare them with the donor's, mapped through `matched`. Drop a candidate if any mapped donor callee
differs from the candidate's callee at the same position. Accept when exactly one candidate survives, at least one
callee was compared, and the target is not already taken. Iterate with neighbour anchoring until nothing changes
(a newly matched callee can settle further wrappers).

- [ ] **Step 1: Write the failing tests:** `test_callee_anchor_picks_wrapper` (three byte-identical wrappers calling
  three different matched functions resolve to the right one each), `test_callee_anchor_needs_a_mapped_callee` (no
  mapped callee: stays ambiguous), `test_callee_anchor_conflict_stays_ambiguous` (two candidates survive: unmatched),
  `test_callee_chain_settles` (a wrapper of a wrapper settles in the second round).
- [ ] **Step 2:** They fail.
- [ ] **Step 3:** Implement the rule in `match_functions`.
- [ ] **Step 4:** All tests pass, old ones too. Known answer: rerun the README's hp1 PS EUR -> PS USA check. Expected:
  still every match equal to `hp1_recomp/port/region/ps-usa.json`, no new wrong ones (more matches are fine).
- [ ] **Step 5:** Rerun hp1 -> PoA and hp2 -> PoA (README commands) into `tools/xdk_port/output/`. Note in
  bringup.md how many of the "ambiguous" rows in "Overrides not bound yet" are now callee matches. Commit
  `xdk_port: settle look-alike wrappers by their callees`.

### Task 4: XbSymbolDatabase signatures as a second source

**Files:**
- Create: `tools/xdk_port/oovpa.py`, `tools/xdk_port/test_oovpa.py`
- Modify: `tools/xdk_port/README.md`, `.gitignore` if the clone lands inside the repo (it should not: use
  `C:\Users\unowi\DecompProject\out\XbSymbolDatabase`)

**Interfaces:**
- Produces: `py -3 -m tools.xdk_port.oovpa --db <XbSymbolDatabase>/src/OOVPADatabase --xbe game/default.xbe
  --functions tools/disasm/output/functions.json --libs D3D8:5849,DSOUND:5849,XAPILIB:5849 --known <map.json>...
  -o tools/xdk_port/output/oovpa_poa.json`. Output: `{"0xADDR": {"name": ..., "lib": ..., "version": N}}` plus an
  `unmatched` list. Functions: `parse_oovpa(text) -> list[Sig]` with `Sig(name, version, pairs: list[(off, byte)],
  xrefs: list[(off, name)])`, and `scan(image, sections, starts, sigs, known) -> (hits, unmatched)`.

Rules: read both source styles found in the clone (`OOVPA_XREF/NO_XREF(name, ver, n) { {off, byte}, XREF_ENTRY(off,
xref) ... }` and the `OOVPA_SIG_HEADER_*` / `OOVPA_SIG_MATCH(...)` form). For each symbol use the highest version
≤ the library's version (5849). An XREF entry is checked when its name is in `known` (an earlier hit or an override
name) and the rel32/abs32 at that offset resolves to it. Otherwise it is skipped, and the signature then needs ≥ 8
byte pairs to count. A hit must be at a function start and unique among them in the library's section (XAPILIB:
`.text`). Never guess.

- [ ] **Step 1:** Clone XbSymbolDatabase (Cxbx-Reloaded) to `DecompProject\out\XbSymbolDatabase`. Read its licence
  and record the commit hash in the README section you add. The repo only reads it, never copies it.
- [ ] **Step 2: Write the failing tests** with inline signature text and a synthetic image: `test_oovpa_parse_both_styles`,
  `test_oovpa_highest_version_not_above`, `test_oovpa_unique_hit`, `test_oovpa_duplicate_rejected`,
  `test_oovpa_xref_checked_when_known`, `test_oovpa_xref_unknown_needs_8_pairs`.
- [ ] **Step 3:** They fail. Implement `oovpa.py`. They pass.
- [ ] **Step 4:** Known answer: run it on hp1's PS EUR XBE with `--libs D3D8:5659,DSOUND:5659,XAPILIB:5659`.
  Expected: every name it shares with hp1's overrides is at hp1's address. Any disagreement is a bug to fix before
  going on.
- [ ] **Step 5:** Run it on PoA with `--known` = the Task 3 maps.
- [ ] **Step 6:** Cross-check against the Task 3 maps: list agreements, OOVPA-only names, and disagreements in
  `docs/bringup.md`. A disagreement stays unbound until resolved by reading both functions. Commit
  `xdk_port: XbSymbolDatabase signature scan as a second source`.

### Task 5: Bind the D3D gaps

**Files:**
- Modify: `port/src/scda/xdk/hle_d3d8.cpp` (the `#if 0  /* PoA: hp1 ... */` blocks), `docs/bringup.md`

- [ ] **Step 1:** For each disabled D3D block (Clear 0x00194EC0, CreatePixelShader 0x001952B0,
  SetPixelShaderConstant 0x00195500, GetRenderTarget2 0x00192BC0, IsFencePending 0x00195B90, and the unnamed hp1
  0x00191A30 / 0x00191AC0 / 0x00192070), take the PoA address from Tasks 3-4. Name the unnamed ones by reading hp1's
  override body and its hp1 callers.
- [ ] **Step 2:** Replace `#if 0` with the PoA `HLE(ADDR, n)`. Check `n` against the PoA function's `ret n` in the
  disassembly. Add `// xdk_port: <how> match` where it is not unique. A function with no PoA match and no title
  caller (Task 2's report) stays disabled, with its comment changed to `no PoA caller (check_hle_coverage)`.
- [ ] **Step 3:** Regenerate and build. Expected: `check_no_foreign_addrs` passes, and
  `check_hle_coverage.py --section D3D --strict` passes or lists only what the allowlist explains (add each reason
  there).
- [ ] **Step 4:** Rerun the Task 1 run (announce). Expected: compared with the baseline, the captures show cleared
  frames (no stale-frame smear), and `MakeSpace from` callers drop. Record the numbers in "## M2 runs". Commit
  `Bind D3D Clear, pixel shader, fence and render target entry points for PoA`.

### Task 6: Vertex shader entry points from hp2

**Files:**
- Modify: `port/src/scda/xdk/hle_d3d8.cpp` (shaders section near line 1016), `docs/bringup.md`
- Donor: `hp2_recomp/port/src/renderer/xdk/hle_d3d8.cpp` lines 1001-1093

**Interfaces:**
- Consumes: `gpu::VertexShader`, `gpu::ParseDeclaration`, `mem::AllocSystem`, `g_vs` (already in hp3's renderer).
- Produces: HLE for `D3DDevice_CreateVertexShader` at **0x00266A90** (4 args),
  `SetVertexShaderConstantNotInlineFast` at **0x00266BD0** (`__fastcall`: ecx register, edx data, 1 stack arg =
  dwords), and SetVertexShaderConstant1/4 if Tasks 3-4 find them in PoA (`__fastcall`, 0 stack args). `VsConstants`
  also writes the XDK mirror `kVsConstants` (PoA address to find).

- [ ] **Step 1:** Find PoA's facts and record each with its evidence in bringup.md: the constant mirror base (the
  store target in PoA's generated SetVertexShaderConstantNotInlineFast body, hp2's equivalent is 0x0013F7D0), the
  `m_VertexShader` device offset (the store in PoA's SetVertexShader 0x00267090 body; hp3 has 0x794 from 5659), and
  whether the title calls Constant1/4 (Task 2 report).
- [ ] **Step 2:** Port hp2's CreateVertexShader and constant setters with PoA addresses and `kVsConstants`. Keep
  hp2's CoS-only fog trace (`0x00017F55`) out.
- [ ] **Step 3:** Regenerate and build. Run (announce) with `RECOMP_D3D_TRACE=1`. Expected log: `d3d: vertex shader
  <handle>, ... decl tokens, ... instructions` at least once (renderer init `sub_00034750` assembles the skinning
  program), and no `SetVertexShader with unknown handle`. The skinned-model draws in the captures' `frame.txt` show a
  vertex program.
- [ ] **Step 4:** Commit `Vertex shader entry points for PoA (from hp2)` with the log lines in the message.

### Task 7: DirectSound bound and on by default

**Files:**
- Modify: `port/src/hle_dsound.c` (disabled blocks, `STREAM_VTABLE`/`STREAM_VTABLE2`, the default at line ~76),
  `port/src/scda/app/settings*` only if a setting describes DSOUND as off, `docs/bringup.md`, `port/DEVELOPING.md` 6.2

- [ ] **Step 1:** Bind every DSOUND gap from Task 2's report: use the Tasks 3-4 maps, name the unnamed hp1 overrides
  from their bodies, and give each its `ret n` check and `// xdk_port:` comment. If the title creates streams (a
  `CreateSoundStream` caller in the report or the survey's TbMapStreamer), find PoA's CDirectSoundStream vtables
  (the constructor's vtable store) and set `STREAM_VTABLE`/`2`. Otherwise leave them 0 and say why in the comment.
- [ ] **Step 2:** Regenerate and build. `check_hle_coverage.py --section DSOUND --strict` passes.
- [ ] **Step 3:** Run (announce) with `RECOMP_HLE_DSOUND=1 RECOMP_DSOUND_TRACE=1 RECOMP_DSOUND_WAV=1` for 90 s.
  Expected: the trace shows DirectSoundCreate, DownloadEffectsImage, buffers created and played, and no FALLBACK
  lines for bound functions. The recorded WAV is not silent:
  `py -3 -c "import wave,array,math,sys;w=wave.open(sys.argv[1]);a=array.array('h',w.readframes(w.getnframes()));print(math.sqrt(sum(x*x for x in a)/max(1,len(a))))" <wav>`
  prints > 100 (16-bit PCM; `audioop` is gone from Python 3.13+).
- [ ] **Step 4:** Only now flip the default to on (`RECOMP_HLE_DSOUND=0` turns it off), update the comment and
  DEVELOPING.md 6.2's row, and regenerate (the regen.sh coverage report shows 0 DSOUND gaps).
- [ ] **Step 5:** Commit `DirectSound HLE bound for PoA and on by default`.

### Task 8: Display timing and XAPI

**Files:**
- Modify: `port/src/hle_video.c`, `port/src/hle_xapi.c`, `port/src/scda/xdk/hle_d3d8.cpp` (`kDevVBlankCallback`, the
  vblank count use), `docs/bringup.md`

- [ ] **Step 1:** Verify the CDevice vblank fields for 5849: name the D3D function that reads and writes
  `edi+0x1DE8` (`recomp_0018.c` ~line 37247), and find the store of the callback that hle_d3d8.cpp calls `m_VBlankCallback`
  (+0x1DB8). Record both in bringup.md with the PoA function names. If either differs, fix the constant.
- [ ] **Step 2:** Bind GetDisplayFieldStatus and GetOverlayUpdateStatus from Tasks 3-4 if found. If they are not
  found and Task 2 shows no title caller, leave them disabled with a `no PoA caller` comment. Rewrite hle_video.c's
  header comment: it still describes Philosopher's Stone's THP movie player and loading thread (PoA's movies are
  `.mad` via `sub_001AC0A0`, drawn as YUY2 quads).
- [ ] **Step 3:** XAPI disc authentication: find PoA's XapiInitProcess (Task 4 hit) and the call in its body that
  matches hp1's call to 0x00155251. Bind `hle_xapi.c` to it.
- [ ] **Step 4:** Regenerate, build, run (announce). Expected log: `[XAPI] disc authentication: skipped, the disc
  counts as authentic` once, and with `RECOMP_XMV_TRACE=1` a field status that advances about 60 times a second.
  Commit `Display timing and XAPI disc check bound for PoA`.

### Task 9: Boot-fix loop to the title screen

**Files:**
- Modify: whatever each fix needs (overrides, `icall_seeds.json`, `port/tools/patch_gen.py`), `docs/bringup.md`

- [ ] **Step 1:** Use superpowers:systematic-debugging for each blocker. One run per hypothesis set (DEVELOPING 8:
  one run answers everything), and look at other original-Xbox recomps before a home-grown fix. Each fix is its own
  commit citing the log line, capture or stack it answers.
- [ ] **Step 2:** The done run (announce; no pad connected, nothing pressed): 180 s with
  `RECOMP_CAPTURE_AFTER=10:10:18`. Expected: the logos (`eac_usa`, `wb`, `intro` `.mad`) play with picture and sound,
  then the title screen. Idling past the attract timeout plays `attract.mad` and returns to the title. No hang, no
  `[CRASH]`. Any controller prompt is the game's own.
- [ ] **Step 3:** In that run's log: zero `d3d: MakeSpace from`, zero `ICALL` failures, zero `unknown handle`.
  `check_hle_coverage.py --strict --section D3D --section DSOUND` passes. Pick the captures for the logo, title and
  attract frames and describe them in "## M2 runs" (no images in git).
- [ ] **Step 4:** Commit `M2: boots to the title screen` with the run summary.

### Task 10: Debug menu (time-boxed: one read, one run)

**Files:**
- Modify: `docs/re-notes.md` (Debug menu section), optionally `port/src/hle_skip.c` or a new `port/src/hle_debug.c`
  (then add it to regen.sh `--exclude-manual`)

- [ ] **Step 1:** Delegate a read-only trace of how `sub_00256C50`'s handlers get registered and opened: who calls
  `sub_00256C50`, which flag or button gates it, and what `DM_ChapterSelection` / "ROOM SELECTION" do. Record the
  answer with addresses in re-notes.md.
- [ ] **Step 2:** If one flag or call opens it, add a `debug_menu` setting (default off, F10 > Debug) that sets it, and
  check it in one announced run (the menu's `poa_debugmenu.big` screen appears). If it needs more than that, record
  what is missing and move it to the M3 plan. Commit `Debug menu: how it opens` (plus the setting if made).

### Task 11: User check and merge

**Files:**
- Modify: `docs/bringup.md`, `port/DEVELOPING.md` 6.1/6.2/7.5 (facts now proven), memory `hp3-recomp`

- [ ] **Step 1:** Rebuild and copy the exes to the test folder, `debug_logs = true` kept.
- [ ] **Step 2:** Ask the user to start it from `hppoa_launcher.exe` with their controller and check: the logos play
  (and a press skips them, as on the console), the title screen and front-end menu draw correctly, menu navigation
  and its sounds work, music plays. Wait for their report. Don't drive their run.
- [ ] **Step 3:** Fix what they report (Task 9 Step 1 rules), or record it as an M3 item if it lies past the front end.
- [ ] **Step 4:** Update bringup.md (what M2 proved), DEVELOPING.md (move proven D3D globals and offsets out of
  "unverified"), and the memory file. Commit `docs: M2 results`.
- [ ] **Step 5:** Use superpowers:finishing-a-development-branch: merge `m2-boot` into `main` locally. Ask the user
  before any push.
