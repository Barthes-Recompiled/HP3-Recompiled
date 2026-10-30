# Handoff 2026-10-30: M4 step 1 (16:9 menus) and step 2 (DirectSound audit) done

Branch `m4-widescreen-menus` (off main 0256ed1), PR on Barthes-Recompiled/HP3-Recompiled. Evidence: docs/bringup.md,
"Widescreen 16:9: menus, loading screens, cutscenes (M4)" and "M4 binding audit".

## Done

- **16:9:** PoA's own copy-swap quad was squeezed a second time (title, main menu, Bonus menus, loading screen 0.5625
  wide); fixed (`frame_copy` in d3d12.cpp). 4:3 screens without the scene show as their middle 4:3 with black sides.
  Pause / Options / Quit Game are one centred 4:3 menu over the widened scene (parchment-band detection,
  `WideFrame::menu`). Checked with window screenshots (runs ws4-ws7).
- **DirectSound audit:** Play (0x002B75A3), SetI3DL2Listener (0x002B8584) and SetBufferData (0x002B7FAE) confirmed
  by body. No code change needed.

## Open / decisions

- **Cutscenes:** left at 2.72:1 letterbox (Hor+ with PoA's own bars at rows 57.5/371). A zoom to fill 16:9 would
  crop ~13% of the original's sides. Asked Austin; nothing done.
- Left edge of the pause menu: the menu's left panel now starts at the 4:3 edge, so a strip of undimmed scene shows
  left of it. Cosmetic.
- No inventory/map screen exists in PoA (pause = Resume/Save/Options/Quit).

## Next (from HANDOFF-2026-10-30-m4-start.md)

3. M4: start to finish on USA with the debug menu's chapter/room warp (0x376B44/48/4C); EUR retarget.
4. Small: FOV slider (cam+0x6C), cutscene state for the skip (`hle_skip.c cutscene()`), immediate-mode effect in a
   level (`sub_00049F70`).
5. Austin's background-input change waits on main reaching the remote.

## Test kit

`Harry Potter Xbox Decomps\tools\poarun.ps1` (refuses while any HP game runs; don't take focus from Austin's live
game), pads `poa_newgame/quit/menus/bonus.pad`. Window screenshots: a PrintWindow loop on hppoa.exe's window (the
guest captures' final.bmp is the 640x480 frame before Present's crop, so it can't show what reaches the screen).
