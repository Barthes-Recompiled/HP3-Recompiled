# Handoff: hp3 M3 in progress (2026-10-30, evening)

Plan: `docs/superpowers/plans/2026-10-30-hp3-m3-ingame-plan.md`. Branch `m3-ingame` (local only; never push without
asking). Ledger (git-ignored): `.superpowers/sdd/2026-10-30-hp3-m3-ingame-plan/progress.md`: trust it and `git log`
over memory, and read its `Ruling:` lines before changing anything they cover. Execution: superpowers:executing-plans
(inline), one fresh whole-branch review before the merge. Evidence for everything below: `docs/bringup.md`, from
"Immediate mode (M3 Task 4)" and "## M3 runs" on.

## Where we are

**New Game reaches the first level.** The run goes title -> "I solemnly swear" screen -> main menu -> Loading ->
Harry in the Hogwarts Express corridor with the HUD, at a steady 30.0 fps, 0 crashes, 0 unresolved indirect calls.

| Task | State | Commits |
|---|---|---|
| 1 xdk_port: two donors on one target | done (one real but harmless collision found) | d8e3af9 |
| 2 kernel timer init race | done (InitOnce, file's own idiom) | 4d8c8eb |
| 3 unattended pad input (`poatrainer.py`) | done | cd053ec |
| 4 immediate mode (Begin/End/SetVertexData) | done; D3D + DSOUND coverage 0 gaps | 0a3ca8f |
| 6 game speed | done: PoA now runs at the console's 30 fps | 171d0e1 |
| 5 New Game -> first level | **in progress**: level reached; ICALL fixed (8008fcf); exit run not done | 8008fcf |
| 7 saves, 8 title sound + Play-after-Pause, 9 debug-menu key, 10 user check + merge | not started | |

## Current issues, most important first

1. **Popping in gameplay (user heard it; measured).** In `m3t6d.wav` (`RECOMP_DSOUND_WAV`), every 3-5 s a one-second
   window has RMS ~2420, 164 or 284 samples at full scale and 15-16 sample-to-sample jumps over 8000. It's the
   same numbers every time, so it's one sound being decoded or mixed wrongly, not general overload. **The first
   burst comes right on the New Game press** (user, and WAV second 99 ~ the A at 96 s), then it repeats in the level.
   Suspects, in order: a sample format our DSOUND HLE (`port/src/hle_dsound.c`) or the ADPCM decoder mishandles
   (EA `.sdt` banks: `hogex_amb_str.sdt` ambience, the menu confirm sound); a voice whose loop points or position
   wrap is wrong; headroom/volume on one voice. Next step, static first: which voices/buffers are created and played
   around the A press and in the level (`RECOMP_DSOUND_TRACE=1` in one run, matched to the WAV's burst seconds),
   then their format (PCM vs Xbox ADPCM, channels, block align, loop region). The "massive clipping" heard at 60 fps
   (run m3t3) is most likely the same burst fired twice as often.
2. **60 fps is not possible yet without speeding the game up.** PoA's logic advances per frame. It runs at 30
   because it asks for `D3DPRESENT_INTERVAL_TWO_OR_IMMEDIATE` (0x80000002), which the HLE used to misread as "every
   vblank": that was the fast cutscene. Now fixed (`port/src/scda/xdk/present_interval.h`). **Caveat:** `fps_limit`
   120+ and "unlocked" still bypass the interval and would run the game 4x fast. The launcher/F10 must not offer
   them for PoA (or must clamp to 30) until the decoupling plan lands. A real 60 needs that plan (spec: 60 is the
   v0.1.0 bar). Study the user's reference recomps first (memory `xbox-recomp-references`: takedawn, BLACK, SCDA,
   ngbrecomp, 007FRWL, Athelas, Darkwatch, DefJam FFNY, Mercenaries).
3. **"Press START" title is silent; the console may have audio there.** Our mix: RMS 0 on the "Press START" screen
   only. From the "I solemnly swear" screen on there is quiet, rising sound (menu music fading in). The user said
   the console "plays the 'I solemnly swear I am up to no good' intro and goes to the New Game... screen that has
   music". **User answers (2026-10-30): the console's Press START screen has no music (ours matches: not a bug),
   and the swear screen plays a spoken voice line.** Ours has only quiet sound there (RMS 50-800, the menu music
   fading in), so the voice line is missing: Task 8 = find why (`frontendspeech.pkg` loads right then). A DeepCode read of the music/speech path failed to
   start (two DeepCode sessions at once); rerun it alone (prompt idea in the ledger / this file's task list).
4. **Pops on the main menu (user, run m3t3 at 60 fps).** Not visible in the 30 fps recordings of the front end (no
   full-scale samples there); recheck by ear at 30 fps. May be issue 1's sound.
5. **Not yet checked:** Harry moving with the pad (the injector's stick), the pause menu, quit to title -> New Game
   again (heap reuse), saves (Task 7; a DeepCode read is running/ran into `.superpowers/dc-m3-saves.txt`), the
   immediate-mode effect in a level (`sub_00049F70`, Task 5 Step 3), the debug-menu key (Task 9).
6. **Settings with no PoA hook (M5):** intro_skip, cutscene_skip, Quit To Desktop, 16:9 Hor+, menu/loading/text-icon
   2D handling. They show in the launcher/F10 but do nothing.

## How to run (unattended, Claude's own runs only)

- `Harry Potter Xbox Decomps\tools\poarun.ps1 <name> <seconds> "<ENV=V;ENV=V>" [<pad script>]`. It refuses if any
  hppoa/hpps/hpcos/crackdown/xemu process runs (another session's Crackdown in `out\wt-pcfeel` ran twice today; wait
  for it, never close it), copies `port\build-win\hppoa.exe` into `test\PoA_USA`, runs the pad script, kills only
  its own PID, and prints the log counts.
- `poa_newgame.pad` (same folder) takes a cold start into the Hogwarts Express at 30 fps: Start 65 s (skips the intro
  movie), Start 80 s (title -> swear screen), A 88 s (main menu), A 96 s (New Game), A 104 s. At 60 fps the timings
  differ (one Start + one A sufficed in m3t3).
- `poatrainer.py pad|peek|selftest` (same folder) injects pad input through `Local\hppoa_trainer`.
- Captures: `RECOMP_CAPTURE_AFTER=s:every:count`, BMPs in `test\PoA_USA\captures\<hhmmss>\`, the full frame is the
  last `*_8207C000.bmp`. Audio: `RECOMP_DSOUND_WAV=<path>` (first 3 min).
- Builds: regen is needed whenever an override set or `port/icall_seeds.json` changes (`bash regen.sh` before cmake).
  The regen log's "47 unresolved call targets stubbed" is pre-existing (same before and after the seeds).

## Why PoA has been harder than Philosopher's Stone and Chamber of Secrets

The short version: hp1 and hp2 were brought up on a toolkit tuned for exactly those games, and PoA shares none of
their game code and only part of their SDK. Most of the work so far has been finding places where the inherited
toolkit quietly assumed hp1's behaviour.

1. **A different engine.** hp1 is Warthog's, hp2 is Eurocom's, PoA is EA UK's in-house engine (the PS2 CoS family).
   None of the siblings' game-specific hooks carry over: movie names, cutscene state, menus, camera, HUD, trainer
   offsets, quit-to-desktop. Each one has to be found again in unnamed code. The spec planned for this, but it is
   most of the remaining parity work.
2. **A newer XDK (5849 vs hp1 5659, hp2 5028).** The D3D/DSOUND HLE is keyed to library function addresses and
   bodies. After the first regen about 60 of hp1's overrides didn't bind, which needed two new matchers (callee
   anchoring, XbSymbolDatabase signatures). 5849 also behaves differently: copy swaps (`Swap(2)` BYPASSCOPY /
   `Swap(4)` FINISH) gave the white frames in M2, and the `*_OR_IMMEDIATE` presentation interval was never seen
   in hp1, so its decode bug sat unnoticed until PoA ran its logic at double speed.
3. **The engine leans on runtime features hp1/hp2 barely touched:**
   - a game clock driven by XAPI `timeSetEvent` KTIMER callbacks, which exposed a lost-timer race in the toolkit
     (the M2 movie stall);
   - immediate-mode drawing (Begin/End), which the renderer never had;
   - C++ method tables filled by static initializers plus a Flash-like APT GUI driven by FSCommand handlers, so
     functions are reached only through pointers and the disassembler misses some (the ICALL failures);
   - EA's own `.mad` movie player and `.sdt` sound banks instead of XMV and the simpler sound use of the siblings
     (PoA's DSOUND section is 37 KB against hp1's 121 KB: different calls, different paths).
4. **Frame-stepped logic at 30 Hz.** hp1 steps its logic on its own clock (25 Hz from KeQuerySystemTime), so its
   frame rate could be raised almost for free; PoA advances per frame, so 60 fps needs real decoupling work.
5. **Testing friction:** shared PC with other sessions' games (the one-instance lock and the "never close someone
   else's game" rule cost waits today), PC heat limits, and the need to drive the game unattended. The last is
   solved now (`poatrainer.py` + `poarun.ps1`).

None of these were dead ends: each was found statically, fixed at the toolkit level (where hp1/hp2 benefit too:
the KTIMER race, the interval decode and the matcher fix are toolkit bugs) and verified with a run.

## Next steps

1. Issue 1 (gameplay popping): one `RECOMP_DSOUND_TRACE=1` run with `poa_newgame.pad` and `RECOMP_DSOUND_WAV`, match
   the burst seconds to the voices playing, read that voice's format and the decoder path, fix, re-measure (the
   burst windows must disappear: no full-scale runs, no 8000+ jumps).
2. Finish Task 5's exit run (stick moves Harry, pause, quit to title + New Game again).
3. Clamp `fps_limit` above 30 for PoA (or hide those choices) so no setting runs the game fast; write it down as a
   ruling.
4. Tasks 7-10 as planned; ask the user issue 3's questions before Task 8.
5. Then a separate plan for 60 fps (decoupling), starting from the reference recomps.
