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
`FSCommand:StartGame`. How it is opened is not known yet. A chapter/room select is the obvious test tool for M2-M4
and a candidate entry point for the randomizer's warps.

## File I/O

- `sub_001A85A0` / `sub_001A85E0` open `D:\%s` through the fopen-like `sub_0013F680`.
- `.big`: `sub_001AB760` builds `%s%s.big`, resolves it (`sub_0013B090`) and reads it (`sub_0014A050`,
  `sub_00149C50`); `sub_0014B840` checks the `BIGF` magic.
- Packages named in the XBE: `data/packages/audio/{audioend,frontend,eventsids,frontendspeech}.pkg`,
  `frontend.pkgA`, `frontendspeech.pkgA`, `data/packages/plfs/frontend-startup.pkg`,
  `data/packages/scplfs/frontendaudio.pkg`, `data/packages/end.pkg`. `.pkg`/`.pkgA`/`.hpb` readers: not found yet.
- `.sdt` names are built by `sub_0015D660`; music `<Music>fe.mpf`; movies `<Movie>%s.mad` (`sub_001AC0A0`).
