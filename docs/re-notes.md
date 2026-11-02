# PoA reverse-engineering notes

Formats, data and game state found while porting, with evidence. Randomizer-relevant finds (pickups, collectibles,
spells, spawns, script loading) go here with their guest addresses and layouts. Addresses are PoA USA
(`default.xbe` SHA-1 `51b09e01...`).

## Engine family

PoA's Xbox build is EA UK's engine, the same family as the PS2 Chamber of Secrets (not the Xbox CoS, which is
Eurocom). The PS2 work (`Harry Potter Xbox Decomps\notes\ps2-cos-grounds-triggers.md`, tools in
`Harry Potter Xbox Decomps\tools\`) is the closest reference. Checked 2026-10-30:

| PS2 CoS tool / fact | PoA Xbox |
|---|---|
| `.BIG` = `BIGF`, big-endian TOC (count, header size, then offset, size, name) | same: `data/gui/poa_debugmenu.big` is `BIGF`, file size LE 0xDA0, count 3, header 0x60, entries `POA_DEBUGMENU.apt` / `.const` / `.geo` |
| `.hpb` compiled room scripts, RefPack (10FB) compressed | not compressed on Xbox; `hpbfn.py` reads them as they are: 350 of 384 `data/misc/*.hpb` parse, 95,555 functions (e.g. `day1_dadaclassroom.hpb`: `Enter`, `SwitchHarry`, `SwitchHer`); 34 parse to nothing, not looked at yet |
| `GeneratedData/BinaryOTIDB.bin` script API reflection DB | the `.pkg` files reference `D:\Dev\POA\Work\Director\Data\OTIDB\enumerations\...xml` (seen inside `packages/orchestrator/inhogex_08.pkg`, compressed); where PoA keeps the binary DB is not found yet |
| `hpbtrig.py` (CREATE_OBJECT trigger boxes) | printed nothing on `day1_dadaclassroom.hpb`; not checked further |

## Disc layout (USA)

`data/` holds `audio/` (`.sdt` sound banks, `.mus`/`.mpf` music), `english/` and `usenglish/` (speech `.ids`,
`variables/Apt_Text.txt` UI text, `LS_Text.txt`), `gui/` (`poa_*.big`: APT menus per language, `poa_debugmenu.big`),
`misc/` (`.hpb` scripts, `LOCALE_*.loc`), `movie/` (`.mad`), `packages/` (`.pkg`/`.pkgA`, ~2,100: levels and
orchestrator packages). The EU disc adds speech and `vox/` folders for Dutch, French, German, Italian, Spanish,
Swedish (3,572 entries against the USA's 3,127).

## Debug menu

`data/gui/poa_debugmenu.big` is an APT movie (the same UI system as the real menus), so the developer menu's screen
survives on the disc. Whether code still opens it: see the M2 survey in docs/bringup.md.

Code side (Codex survey, 2026-10-30): `sub_00256C50` registers seven debug-menu handlers through `sub_001AE6C0`:
DM_Activate `sub_0001F770`, DM_DeActivate `sub_0001F780`, DM_DPadUp `sub_0001F790`, DM_DPadDown `sub_0001F7A0`,
DM_Selection `sub_0001F7B0`, DM_ChapterSelection `sub_0001F7D0`, DM_BackSelection `sub_0001F7C0`. The string
"ROOM SELECTION: X - Select debug start point" is used at 0x000233A3 (`sub_000232D4`); "DebugMenu" at 0x00020315
(`sub_00020310`). Menu resource strings: `debug_menu_depth`, `DebugMenuCustomControlValue`, `StartLoadingGame`,
`FSCommand:StartGame`. A chapter/room select is the obvious test tool for M2-M4 and a candidate entry point for the
randomizer's warps.

**Opening it (run m3dbg, 2026-10-30, M3 Task 9).** With `debug_menu = true` (`RECOMP_DEBUG_MENU=1`) the front end logs
"[DEBUG] front end asked for the debug menu: allowed"; on the main menu (New Game / Load Game / ...) the **left
trigger** opens it: the log reads `poa_debugmenu.big` right after the LT press (B, X, Y, Black, White on the swear
screen / main menu did nothing). It shows "CHAPTER SELECTION: 'X' - Select Room, 'D' - Start Chapter", "Build: May  8
2004, 15:54:23", chapters Intro, Night0, Day1, Night1, Day2, Night2, and a block of developer placeholder text
(captures 182353-182403). Choosing a chapter/room and the 0x376B44/48/4C writes are not checked yet (M4 level-warp tool).
The static decode failed: the .apt bytecode refers to a per-function constant pool (u32 index lists into
POA_STARTUP_XBOX.const: index 5 Key, 6 getCode, 7 POA_DEBUGMENU.swf) and its push opcodes are EA's own.

How it opens (Explore trace, 2026-10-30, Task 10): `sub_00256C50` is a static constructor (CRT initializer table at
.data 0x2F8F18) that fills the APT FSCommand table with `{name, handler}` pairs through `sub_001AE6C0`; the DM_*
entries sit at 0x30F118-0x30F148 and "DebugMenu" at 0x30F180 (name 0x2C9360, handler `sub_00020310`). The front
end's APT scripts (`poa_startup_xbox.big`, `poa_fe.big`) call `FSCommand:DebugMenu` (RequestDebugMenu) and, in a
key handler (`onKeyDown` / `Key.getCode`), open `POA_DEBUGMENU.swf` through `extern.TransitionTooMovie` when the APT
variable `DebugMenu == "true"`. `sub_00020310` sets that variable through the APT set-variable call at [0x38A2F8]
(`sub_001AD3B0`) to the constant "false" (0x2C8718); "true" is at 0x2C8730. That is the only gate: no flag, config
file or command line. The handlers' worker is the singleton at 0x30E2D4 (+0x1C chapter, +0x20 room, +0x24 cursor,
+0x28 active): Activate `sub_0001B560` (inits "ChapterRoom" and "DebugStartPoint"), DeActivate `sub_0001C720`,
DPadUp/Down `sub_0001B6F0`/`sub_0001B750`, Selection `sub_0001B800` (chapter -> room -> start point, writing
**0x376B44** chapter, **0x376B48** room, **0x376B4C** start-point name, which the level loader reads around
recomp_0000.c 73036-73998), ChapterSelection `sub_0001B7D0`, BackSelection `sub_0001B900`. The text renderer
`sub_000231D0` ("Build: %s, %s" May 8 2004, CHAPTER/ROOM SELECTION) is the custom control
"DebugMenuCustomControlValue" (`sub_0001DE30`, registered at .data 0x2FA178). `hle_debug.c` answers "true" when
`debug_menu` is on (F10 > Debug). Not known yet: which key or button the APT key handler waits for (decode the
ActionScript in poa_startup_xbox.big near file offset 0x12394 or poa_fe.big near 0x64EB4).

## File I/O

- `sub_001A85A0` / `sub_001A85E0` open `D:\%s` through the fopen-like `sub_0013F680`.
- `.big`: `sub_001AB760` builds `%s%s.big`, resolves it (`sub_0013B090`) and reads it (`sub_0014A050`,
  `sub_00149C50`); `sub_0014B840` checks the `BIGF` magic.
- Packages named in the XBE: `data/packages/audio/{audioend,frontend,eventsids,frontendspeech}.pkg`,
  `frontend.pkgA`, `frontendspeech.pkgA`, `data/packages/plfs/frontend-startup.pkg`,
  `data/packages/scplfs/frontendaudio.pkg`, `data/packages/end.pkg`. `.pkg`/`.pkgA`/`.hpb` readers: not found yet.
- `.sdt` names are built by `sub_0015D660`; music `<Music>fe.mpf`; movies `<Movie>%s.mad` (`sub_001AC0A0`).

## Randomizer-relevant finds (2026-10-31, static)

Full audit and design in [`docs/ap/`](ap/README.md). Addresses are USA; in this image `.rdata` virtual = file
offset + 0x27C0.

- **APT variable setter `sub_001AD3B0(name, value)`** — behind the pointer at `0x38A2F8`; 265 call sites;
  definition `recomp_0011.c:14219`. Every UI state write goes through it: `WizardCards`, `Beans_Amount`,
  `Housepoints`/`House_Points`, `FolioBrutiPagesCollected`, `Lost_And_Found`, `Task_Done_1…4`, `ChapterId`,
  minigame availability. The cheapest check-detection hook in the game (passive, log-only).
- **Cards**: 52 slots. The album gets a 52-character `'0'/'1'` string as `WizardCards`
  (`sub_00020010…0034`, `recomp_0000.c:58859-59310`); ownership is two heap bit arrays whose pointers are
  `0x3661C0`/`0x3661C4`, read by `sub_00071840(id)` (`recomp_0002.c:74938`). Ten sets; the narrator names 50
  cards (5 per set); the two extra slots are unexplained (likely the rare shop cards).
- **Chapter/room table**: `data/misc/system_chapterroom.hpb`, `Init` constant pool — chapter names
  (Intro, HogwartsExpress, Night0…Day6, Minigames, duel clubs) and room names/ids. No Day4/Night4 despite the
  speech banks.
- **Travel/load**: `0x376B44` chapter, `0x376B48` room, `0x376B4C` start-point name, `0x376B50` pending flag;
  loader `sub_00115390(chapter, room, startPoint, flag)` (`recomp_0000.c:73029-73175`). The debug menu's
  `DM_Selection` (`sub_0001B800`) writes the same globals.
- **Chapter lifecycle**: each chapter has `*_initialisechapter.hpb` (`InitialiseVars`, `InitialiseChapter`,
  `DeinitialiseChapter`, `SetStartRoom`; `intro` also `AddSpells`) and `*_endchapter.hpb`
  (`EndChapter_StartNextChapter`). `day3_chaptersavestate.hpb` / `night5_chaptersavestate.hpb` are explicit
  start-state scripts (`_Support_InitCapabilitySets`, 952 bytes) — the model for AP's seeded chapter start.
- **Tasks**: `GUI_TASKLIST_<chapter>_00N` in `Apt_Text.txt` — 35 tasks over 11 chapters; the Tasks page is driven
  by `Task_Done_1…4` / `Task1_Display…4`.
- **Still open**: the item-grant/inventory function (HP1 `sub_00041D20` / HP2 `sub_00081A10` analogue); the save
  progress block (16 KB `SaveGame-0`, serialise/deserialise unmapped); the `.hpb` VM dispatcher (95,554 functions
  called by name); pickup placements (`.pkgA` manifests list classes only).
