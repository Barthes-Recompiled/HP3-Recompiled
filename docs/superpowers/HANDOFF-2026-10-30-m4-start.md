# Handoff 2026-10-30: M3 merged, M4 next

`main` = merge 3a169bc of `m3-60fps` (M3 in-game + 16:9 Hor+ + 60 fps default), pushed to `github/main` 2026-10-30.

## M3 result (Austin's play-check 2026-10-30: "Play felt good")

- New Game reaches the Hogwarts Express; pause, quit to menu, New Game again; saves write and Load Game restores
  them (`saves\UDATA\4541004c\...`, run m3load).
- Audio: SetMixBinVolumes binding fixed (pops), 5.1 ring folded to stereo (voices, the swear-screen narrator line).
- 16:9 Hor+ for the 3D (311a8c2). 60 fps default; the game clock's per-frame floor is 1 ms above 30 fps, 33 ms at
  30 (hle_video.c `sub_001A7450`; docs/bringup.md "60 fps").
- Debug menu: `debug_menu = true`, left trigger on the main menu -> chapter selection (re-notes, run m3dbg).
- DeepCode review follow-ups: docs/bringup.md, "M3 review follow-ups".

## Next, in order

1. 16:9 for menus, loading screens, cutscene bars, inventory/map (`hppoa_MenuMovie` / `LoadingScreen` / `TextIcon`
   stubs, the `kMenuX` tag in d3d12.cpp `Squeeze2D`).
2. Audit the other anchored DirectSound bindings (Play 0x002B75A3, SetI3DL2Listener 0x002B8584) by body.
3. M4: start to finish on USA; the debug menu's chapter/room warp (0x376B44/48/4C) as the test tool; EUR retarget.
4. Small: FOV slider (cam+0x6C), cutscene state for the skip (`hle_skip.c cutscene()`), immediate-mode effect in a
   level (`sub_00049F70`).
5. Austin's background-input change waits on this merge reaching the remote.

Test runs: `Harry Potter Xbox Decomps\tools\poarun.ps1` (refuses while any HP game runs; when Austin's hpps is open,
start hppoa directly the same way, one PoA only), pad scripts `poa_newgame/load/save/dbgkey.pad`.
