# Handoff 2026-10-30: 16:9 Hor+ done, 60 fps next

Branch `m3-widescreen` = local `m3-ingame` + 311a8c2 (not pushed; ask Austin before pushing). Details and evidence:
docs/bringup.md, "Widescreen 16:9 Hor+".

- **Widescreen:** RECOMP_ASPECT=16:9 (launcher/F10 aspect "16:9") now widens PoA's perspective projections by 4/3 at
  the sides in hle_video.c (sub_000376E0, sub_00037000, sub_000518D0). In-game checked on the Hogwarts Express
  (run ws1). Still open for a later pass: menus/APT GUI, loading screens, cutscene bars, inventory/map in 16:9
  (hppoa_MenuMovie/LoadingScreen stubs), and a possible field-of-view slider (cam+0x6C is the horizontal FOV in degrees).
- **For the 60 fps thread:** nothing in the widescreen hook is per-frame state, so it doesn't interact with frame
  pacing. Start points are unchanged from HANDOFF-2026-10-30-m3-late.md ("Later the same evening", item 2): timer
  sub_001A7450, 33/40 ms floor at 0x001A74A0, fps clamp from c6ba8d4. Test runs: `Harry Potter Xbox Decomps\tools\poarun.ps1`
  (checks no other game is running, closes its own PID); one PoA instance at a time.
