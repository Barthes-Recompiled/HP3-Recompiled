# HP: Prisoner of Azkaban — Xbox to PC port (hp3_recomp) — design

Date: 2026-10-30. Status: draft for review. Repo: https://gitlab.com/barthes_recompiled/hp3_recomp (empty).
This file moves to `docs/superpowers/specs/2026-10-30-hp3-port-design.md` in the hp3 repo as its first doc commit.

## 1. Intent

A native Windows port of the original Xbox *Harry Potter and the Prisoner of Azkaban* (2004), made by static
recompilation like the sibling ports hp1_recomp (Philosopher's Stone) and hp2_recomp (Chamber of Secrets). It joins
them as the third game of the barthes_recompiled set: the three will be installed through one shared launcher, and
later get Archipelago (randomizer) support and further QoL.

Why: a faithful, good-looking, high-frame-rate PC version of PoA that matches the other two ports, so the trilogy
can be offered and maintained as one family.

## 2. Goals, non-goals, anti-goals

**Goals (first public release, v0.1.0)** — feature parity with what hp1/hp2 ship today:

- Both discs supported: USA and Europe (multi-language), via the region retarget mechanism.
- Playable start to finish with no known crashes/blockers on either disc.
- Resolutions up to 4K, MSAA/SMAA, anisotropic filtering, true 16:9 (widened 3D, 2D kept in shape),
  windowed/borderless/fullscreen.
- 60 fps, correctly paced, with game logic behaving as at the original rate (user, 2026-10-30: 60 is the bare
  minimum and the v0.1.0 bar; 120/144 and above come in a later release). If PoA's logic is frame-locked, the
  decoupling work is post-v0.1.0 unless 60 itself needs it.
- Keyboard and mouse with rebinding, controller remapping, deadzones, vibration strength.
- Quit To Desktop in the game's menus.
- Cutscene fast-forward/skip and start-up logo skip (as hp1/hp2: hold Start/Back; `intro_skip`, `cutscene_skip`).
- Settings via the launcher and the F10 panel, as in hp1/hp2.
- Toolkit/renderer-level extras that come free with the base: colour-blind filters, 5.1 surround, FPS counter.

Game-specific extras the siblings have (HUD anchoring in 16:9, per-shot cutscene zoom, draw-all-objects, trainer
gameplay features) are **not** in the v0.1.0 bar; they are rebuilt for PoA's engine afterwards (Claude's call).
- A release zip from `package_release.sh` that also satisfies the future shared launcher's contract (section 6).

**Later releases (not v0.1.0):** 120/144 fps and above where game logic allows; a themed in-game settings menu;
Archipelago client + apworld; other QoL. The order after v0.1.0 is QoL, then Archipelago.

**Non-goals:** the shared launcher itself (separate repo, "coming soon", built by others/later); a shared toolkit repo
across hp1/hp2/hp3 (approach C, deferred); PS2/GameCube versions; any port of the EA engine to source.

**Anti-goals (a failure even if it runs):**

- Game content of any kind in git (disc files, `gen/`, extracted assets, captures, artwork).
- Hacks that mask a host bug instead of translating the console's behaviour (see section 4 working rules).
- Diverging from hp1/hp2 conventions (layout, settings, launcher handshake, packaging) without a reason; the three
  must stay maintainable as one family.
- Losing the upstream credit chain (xboxrecomp by sp00nz, SCDA by TGP482, MiddleDucky's port work).

## 3. What we know about the game (found 2026-10-30)

| Fact | Value |
|---|---|
| ISOs | `Harry Potter Xbox Decomps\isos\Harry Potter and the Prisoner of Azkaban (USA).iso`, `... (Europe) (En,Fr,De,Es,It,Nl,Sv).iso` |
| `default.xbe` SHA-1 | USA `51b09e01169fda86f2cb7e5ce45ee80a10268918`, EUR `c8841e3c1d2d5ec20dbe286e5bec9bb5104b41f9` |
| XBE size / difference | both 3,190,784 bytes; only 534 bytes differ (region retarget should be small) |
| XDK libraries | D3D8, D3DX8, DSOUND, LIBCMT, XAPILIB, XBOXKRNL, XGRAPHC — all **5849** (hp1 is 5659) |
| Build path | `D:\dev\POA\builds\applications\harrypotter\xbox_release\appharrypotter.exe` |
| Engine | EA UK's in-house engine (same family as PS2 CoS), **not** Warthog (hp1) or Eurocom (hp2) |
| Disc layout | `data/` with `.pkg`/`.pkgA` (~2,100), `.sdt` audio (491), `.hpb` compiled room scripts (384), `.big` (65, incl. APT GUI `data/gui/poa_*.big`), `.mus/.mpf` music, `.mad` movies, per-language `speech/`, `variables/Apt_Text.txt` |
| Curiosities | `data/gui/poa_debugmenu.big` (possibly a developer menu); `runtime.cfg`, `filesys.dat` at root |
| File counts | USA 3,127 entries; EUR 3,572 (extra languages: dutch/french/german/... speech, vox) |

Prior work that transfers: the PS2 CoS reverse engineering in `Harry Potter Xbox Decomps\notes\ps2-cos-grounds-triggers.md`
and tools `Harry Potter Xbox Decomps\tools\` (`ps2bigx.py`, `refpack.py`, `hpbfn.py`, `hpbtrig.py`) — same engine
family, so BIG/RefPack/`.hpb` knowledge is expected (not yet proven) to apply. hp1/hp2's game-specific overrides do
**not** transfer; their toolkit, renderer, launcher, settings, trainer, region and QoL infrastructure does.

## 4. Constraints

- Toolchain and layout as hp1/hp2: MSYS2 UCRT64 GCC + CMake + Ninja + Python/capstone; toolkit at repo root,
  game project in `port/`; `port/regen.sh` generates `port/src/recomp/gen/` from `game/default.xbe`; overrides in
  `port/src/hle_*.c` listed in regen.sh `--exclude-manual`.
- Working rules from hp1 `port/DEVELOPING.md` section 8 apply verbatim: translate faithfully; debug data and other
  original-Xbox recomps before home-grown fixes; one test run should answer every open question; check original
  hardware (or xemu) before "fixing" odd behaviour; saves in `<exe dir>\saves`; never delete whole output folders;
  nothing from the game in the repo.
- Testing: the user play-tests on this PC with their own controller. Announce a test run before starting it (it
  opens on their screen), close your own runs afterwards, never close a game someone else started, never drive their
  test runs with scripted input. Unattended runs of your own (trainer, debug.env traces) are fine when announced.
- Licensing: port code MIT; keep `LICENSE`, `LICENSES/`, `NOTICE`, README credits; this is aided by Claude Code.
- No game data or generated code in git; users supply their own disc.
- Compute: farm bulk reading out to agy (Gemini, read-only) and Codex (read-only reviews; scoped implementation on
  `wip/*` branches), as in Crackdown — e.g. the hp1/hp2 toolkit diff triage, the XDK 5849 signature survey, sweeps of
  generated code and logs, and PoA file-format checks against the PS2 CoS tools. Claude Code builds, tests and merges.

## 5. Approaches considered

- **A. Fork a sibling port with history, strip to a skeleton — CHOSEN by the user.** Keeps toolkit, renderer
  (SCDA-derived), launcher, settings/F10 panel, trainer, region retarget, QoL code; removes the game-specific hooks.
  Toolkit fixes flow between repos as git merges; credit history preserved. Cost: a clean-up pass and carrying over
  fixes only the other sibling has.
- **B. Fresh from upstream xboxrecomp + SCDA.** Clean, but redoes weeks of work and likely reintroduces lifter/kernel
  bugs hp1/hp2 already fixed. Rejected.
- **C. Extract the toolkit into one shared repo for hp1/hp2/hp3.** Best long-term, but restructures two shipped
  projects before PoA boots. Deferred until after the PoA beta, if ever.

Why A over B: speed to a booting game and inheriting proven fixes. **Base = hp1_recomp (Claude's call, revised
after the builder check; first draft said hp2):** hp1's XDK is 5659 vs hp2's 5028 (PoA is 5849, so hp1's D3D8/DSOUND
HLE is closer); hp1 has the same two-region retarget; hp1 v0.2.0 carries the newer upstream toolkit fixes
(clock/time zone/region/cache, 5.1 surround, shader prebuild); hp1 already has above-60 frame pacing (`PaceAbove`
in `port/src/scda/xdk/hle_d3d8.cpp`) and fps caps. hp2-only fixes (about 12 `src/` + 22 `tools/` files differ;
renderer lives at `port/src/renderer/` in hp2 vs `port/src/scda/` in hp1) are reviewed and ported where they are
toolkit-level.

## 6. Decided

1. **Repo setup:** clone hp1_recomp with history into `hp3_recomp`, set `origin` to the GitLab hp3 repo, keep hp1
   (`hp1`) and hp2 (`hp2`) as remotes for toolkit fixes. Remove PS-specific hooks (anim, world, frontend specifics,
   trainer game offsets...). Rename the game: exes `hppoa.exe`, `hppoa_launcher.exe`, `hppoa_trainer.exe`; CMake
   options `HPPOA_*`; launcher env `HPPOA_LAUNCHER=1` (follow the siblings' naming pattern).
2. **Canonical disc = USA** (user, 2026-10-30): overrides, seeds and patch labels use the USA XBE's addresses
   (`51b09e01...`); EUR is the retargeted profile. Release testing: one full user play-through on USA; EUR gets the
   lighter check in M4 (Claude's proposal; the user named USA as main without objecting to it).
3. **First release bar:** v0.1.0 ships only at hp1/hp2 parity (section 2). Internal milestones along the way.
4. **Launcher, now:** keep the base's per-game launcher (ImGui over D3D11 + `xiso.c` disc unpacker) with minimal changes;
   don't invest in it — the shared launcher replaces it.
5. **Launcher, future contract:** the shared launcher (separate repo, e.g. `barthes_recomp_launcher`) takes the
   player's ISO, extracts it and downloads the matching port build from this repo's GitLab Releases. So each release
   publishes a zip with a stable name (`hppoa_v<VERSION>_windows.zip`) containing the port's built files only (exes,
   shaders, readme, licences — no disc content), plus a small machine-readable manifest (e.g. `release.json`: game id,
   version, supported `default.xbe` SHA-1s per region, expected game-folder layout, entry exe). Exact manifest schema
   is aligned with the launcher work when it starts; until then, write the obvious one and keep it stable.
6. **Project order:** port → v0.1.0 (parity QoL) → further QoL (high refresh, themed menu, more) → Archipelago.
   Launcher work happens in its own repo in parallel.
7. **Randomizer-minded RE while porting:** when the port touches pickups, collectibles (beans, cards, challenges),
   spells, script loading (`.hpb`), record guest addresses and layouts with evidence in `docs/re-notes.md` (new in
   this repo). Prefer hooks that intercept data loading over hardcoded patches.

## 7. Milestones (internal)

| M | Done when |
|---|---|
| M0 Skeleton | Fork renamed, CoS hooks removed, builds against an empty/stub gen with PoA naming; hp1-only toolkit fixes ported. |
| M1 Regen | `regen.sh` runs clean on the canonical PoA XBE: XDK 5849 library functions identified (D3D8/DSOUND/XAPI HLE signatures checked or added), 0 unresolved calls, links. |
| M2 Boot | Reaches the EA/WB logos (or skips them) and the title/front-end menu with correct rendering and audio. |
| M3 In game | New Game reaches the first level; control, save/load to `<exe dir>\saves` work. |
| M4 Playable | Start to finish on the USA disc by user play-test. EUR via retarget: Claude loads every level unattended (trainer / debug-menu warps) and checks logs, then the user does a short session (title, save + load, a level or two, a language switch). |
| M5 Parity | All section 2 features working and play-tested; `package_release.sh` produces the zip + manifest. Tag v0.1.0. |

## 8. Risks and unknowns

- XDK 5849 coverage in the HLE (D3D8 push-buffer/state paths, DSOUND) — hp1 (the base) is 5659, hp2 is 5028; signatures for 5849 must be checked or added.
- EA engine threading/timing (frame-locked logic, worker threads) for unlocked fps; the 16:9 and widening approach
  must be found anew (different engine).
- APT (Flash-like) GUI rendering may stress the renderer differently (vector shapes, many small draws).
- EA audio formats (`.sdt`, `.mus`) and `.mad` movies — expected to be decoded by game code on the CPU, so likely
  free; confirm.
- `poa_debugmenu.big` may give a developer menu useful for testing (level select) — investigate early.

## 9. Left to the builder

- How hp1-only toolkit fixes are brought over (cherry-pick vs. manual port) and in what order.
- Exact list of hp2 files that are game-specific vs. infrastructure, beyond the obvious ones named above.
- Release manifest field names (section 6.5) until the shared launcher defines them.
- Internal tooling (trainer features, debug.env switches) as needed to reach each milestone.
