# Handoff: hp3 after M2 merge, M3 not planned yet (2026-10-30, afternoon)

## State

- `main` (local only, NOT pushed; ask before the first push to GitLab barthes_recompiled/hp3_recomp) has M2 merged
  (248f051) + review follow-ups (f951e0d). Boots logos -> legal -> intro -> title -> attract loop at 60 fps.
- Movie-start stall fixed: lost KTIMER expiry in `kernel_set_timer` (3e7e884), confirmed 5/5 runs (conf1-5 logs in
  `Harry Potter Xbox Decomps\test\PoA_USA`).
- User check (Task 11) passed: logos, title, front-end menu, menu sounds, music.
- Test folder `Harry Potter Xbox Decomps\test\PoA_USA` has the current exe (hash CF685B849B99...). Recopy after
  any rebuild. Build: `$env:MSYSTEM="UCRT64"; C:\msys64\usr\bin\bash.exe -lc "cd /c/Users/unowi/DecompProject/hp3_recomp/port && cmake --build build-win -j4"`.

## Next: write the M3 plan

Use superpowers:brainstorming / writing-plans, with the M2 plan as the template
(`docs/superpowers/plans/2026-10-30-hp3-m2-boot-plan.md`). Goal per user decisions: v0.1.0 = hp1/hp2 feature parity
at a correctly paced 60 fps. Candidate M3 items:
- Get from the title into gameplay (New Game -> first level); whatever breaks there is the main work.
- Immediate mode (SetVertexData4f; two start-up quads from `sub_0004A3C0`).
- Debug menu key (APT bytecode in `poa_debugmenu.big`; `debug_menu` setting answers the gate `sub_00020310`).
- Title-screen music: ours is silent (RMS 0); unknown whether the console is too. Ask the user or check Xemu/footage.
- Review follow-ups in `docs/bringup.md` ("M2 review follow-ups"): match.py duplicate target, timer lazy-init race,
  dsound Play-after-Pause.

## Working rules (save Claude usage this week)

- Send bulk reading and reviews to DeepCode: from the repo folder, `deepcode -x -p "<one-line prompt>"`. The prompt
  must be ONE line and must start with "Do NOT use bash or any shell command, and do NOT read any path outside the
  current folder; use only file read/list/search tools". Put big inputs (diffs) in `.superpowers/` and point at them.
  Run it in the background; a whole-branch review took ~15 min.
- Verify every DeepCode finding against the code (and `git merge-base --is-ancestor` to tell new vs pre-existing).
- Static analysis first; game runs only to confirm a hypothesis. Keep runs short (PC heat), tell the user before
  a run, don't run while another session's game is running (check `Get-Process`; Crackdown 2 may be Austin's).
- Gotchas from M2 handoff still apply (`HANDOFF-2026-10-30-m2.md`, "Gotchas").
