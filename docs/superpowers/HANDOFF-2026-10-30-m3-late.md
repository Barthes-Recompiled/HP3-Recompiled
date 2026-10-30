# Handoff: hp3 M3, late evening (2026-10-30)

Follows `HANDOFF-2026-10-30-m3-mid.md` (read its "How to run" and "Why PoA is harder"). Plan:
`docs/superpowers/plans/2026-10-30-hp3-m3-ingame-plan.md`; ledger `.superpowers/sdd/2026-10-30-hp3-m3-ingame-plan/progress.md`.
Evidence for everything below: `docs/bringup.md`, "M3 audio pops" and the runs after it.

## Done this session (branch `m3-ingame`)

| What | Commit | Evidence |
|---|---|---|
| Gameplay popping / NaN silences: 0x002B7587 is IDirectSoundBuffer_SetMixBinVolumes, not SetVolume (the anchored xdk_port match). Its DSMIXBINS pointer was read as a volume = +inf gain | bd7a29e | m3nan (diagnosis), m3fix: 0 clipped samples, no silent gaps |
| Frame-rate choices limited to the game's rate (30) and 30; caps above 60 / unlocked in old settings map to the game's rate | c6ba8d4 | m3fps: unlocked holds 30.0 fps |
| Task 5 exit run: stick moves Harry, pause menu, Quit Game -> main menu -> New Game reloads | docs | m3quit, m3quit2 |
| Task 7 first look: Save Game writes `saves\UDATA\4541004c\<id>\SaveGame-0` (16 KB) + metadata | docs | m3save |

The DeepCode saves read finished: `.superpowers/dc-m3-saves.txt` (title ID 0x4541004C, U:/T: per-title mapping).
There was no uncommitted kernel_bridge.c change: the tree was clean at the start (4d8c8eb already has the kernel work).

Pad scripts (outside the repo, `Harry Potter Xbox Decomps\tools\`): `poa_exit.pad` (stick + pause), `poa_quit2.pad`
(quit to menu + New Game again), `poa_save.pad` (pause -> Save Game -> slot 1). The capture clock
(`RECOMP_CAPTURE_AFTER`) runs about 2-10 s behind the run script's wall clock; check the capture folder times.

## Next

1. Task 7 rest: Load Game from the main menu restores the m3save save (it is still in `test\PoA_USA\saves`).
2. Task 8: the swear screen's missing voice line (`frontendspeech.pkg`); rerun the DeepCode music/speech read alone.
3. The other anchored DirectSound bindings (`docs/bringup.md` "Bindings to suspect first": Play 0x002B75A3,
   SetI3DL2Listener 0x002B8584) deserve the same body check that caught 0x002B7587.
4. Tasks 9-10 (debug-menu key, user check + merge), the immediate-mode effect in a level (`sub_00049F70`).
5. A separate plan for real 60 fps (decoupling), from the reference recomps.
6. `m3-ingame` is 5 commits ahead of `github/m3-ingame`; not pushed (ask Austin).
