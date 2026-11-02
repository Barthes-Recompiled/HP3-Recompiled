# 01 — Audit: what the disc, the scripts and the recompilation give us

All addresses are PoA **USA** (`default.xbe` SHA-1 `51b09e01…`, 3,190,784 bytes). "Virtual" addresses are the
addresses the recompiled code uses; in this image's `.rdata`, **virtual = file offset + 0x27C0** (cross-checked
against the known `"false"`/`"true"` strings at 0x2C8718/0x2C8730, which are file 0x2B5F58/0x2B5F70).

## 1. The disc

```
game/default.xbe            3.1 MB, USA
game/data/
  audio/                    .sdt sound banks, .mus/.mpf music
  english/ usenglish/       speech .ids + .sdt, variables/Apt_Text.txt (UI + dialogue), LOCALE_*.loc
  gui/                      APT menus as .big (front end, pause, quest pages, card album, minigames)
  misc/                     384 .hpb room scripts, 34 non-function tables (mostly *_definitions.hpb), LOCALE_*.loc
  movie/                    .mad
  packages/                 2,095 .pkg/.pkgA files
```

`packages/` groups, by what they are:

| Group | Pairs | Contents |
|---|---|---|
| `plfs/` | 232 | **rooms** — `day1-entrancehall.pkg` + `.pkgA`; the playable spaces |
| `orchestrator/` | 157 | mission/cutscene orchestration (`d1broom1_01`, `d1dada_01`, …) |
| `cutsceneeffects/` | 163 | cutscene-only effect packages, one per scene id |
| `effects/` | 55 | shared effects (`carperetractumchallenge`, …) |
| `groper/` | 65 | "groper" object/trigger packages (`bean_groper`, `charhit_regroper`, …) |
| `audio/` | 70 | per-room audio packages |
| `scplfs/` | 2 | script-only level packages |

### 1.1 `.pkg` / `.pkgA` — the chunked container

Both are `PKGH` files: `PKGH` + u32 size, then chunks (`PKGD`, `PKGCD`, `PKGE`, …). The `.pkgA` is the **manifest**:
an `SSAS` string (the original build path, `D:\dev\POA\DVD\xbox-us\diskfiles\data\packages\plfs\day1-<room>.sas`),
then a `PKGD` list of asset references:

```
<maps>dadaclassroom.ccr              collision
<maps>dadaclassroom.rig / .hct / .cpb
<scriptbinaries>\day1_dadaclassroom.hpb
<models>ar3_wp_pup_wizardcard.hpm    pickup models
<textures>…                          (every texture the room streams)
<animations>…                        .fa2 / .btf / .asb
```

Consequences for the census:

- The manifest **names the pickup classes a room can use** (card, bean, cauldron cake, chocolate frog, Folio Bruti
  page, Wiggenweld, antidote, dungbomb, stink pellet, chest, goblet, jar) and links the room to its script. It does
  **not** carry placements or counts: the shared pickup set appears in ~212 of 232 room manifests regardless of what
  is actually placed.
- Placements therefore live either in the room's `.pkg` payload (object tables, format not yet decoded) or in the
  room's `.hpb` script (spawn calls with model names and coordinates). Both are open leads; the scripts already
  contain model-name strings, so the script route looks cheaper.

### 1.2 `.hpb` — room scripts

RefPack-compressed on PS2 CoS, **not** compressed on this Xbox disc; the PS2-era `hpbfn.py` reads them as-is.
Each file is a sequence of function records: `u8 namelen, name, u32 code_len_in_halfwords, code, u16 0, u16
const_count, constants`. The constants include strings, floats and integers; the string constants are how the VM
references other script functions (`SetUp_Day1_Speech`, `PursuitSpeech`, …).

Census of the 384 files:

| Metric | Count |
|---|---|
| function records | **95,554** |
| distinct function names | **6,163** |
| distinct identifier-like string constants | **10,207** |
| files | 384 (350 parse; 34 are tiny data tables, 60–278 bytes) |

Per chapter (functions, distinct names):

| Chapter | Files | Functions | Distinct | `talk` | `pursuit` | `card` | `remembrall` | `task` |
|---|---|---|---|---|---|---|---|---|
| intro | 4 | 691 | 647 | 45 | 3 | 0 | 0 | 1 |
| night0 | 11 | 3,754 | 1,540 | 275 | 126 | 0 | 33 | 4 |
| day1 | 23 | 8,745 | 2,259 | 640 | 377 | 63 | 78 | 8 |
| night1 | 22 | 7,488 | 1,620 | 616 | 365 | 60 | 75 | 6 |
| day2 | 27 | 9,038 | 1,770 | 618 | 327 | 76 | 94 | 6 |
| night2 | 23 | 7,385 | 1,648 | 411 | 103 | 63 | 78 | 3 |
| day3 | 30 | 11,067 | 2,372 | 820 | 557 | 81 | 103 | 8 |
| night3 | 27 | 10,214 | 2,203 | 722 | 444 | 75 | 94 | 6 |
| day5 | 29 | 11,244 | 1,987 | 888 | 695 | 81 | 102 | 8 |
| night5 | 32 | 11,240 | 1,989 | 806 | 453 | 89 | 111 | 8 |
| day6 | 29 | 9,705 | 1,865 | 522 | 89 | 82 | 102 | 6 |
| minigames | 8 | 1,201 | 419 | 68 | 12 | 0 | 8 | 0 |
| multiplayer / singleplayer duel | 8 | 545 | 491 | 16 | 6 | 0 | 0 | 0 |
| testchapter | 86 | 3,191 | 635 | 62 | 30 | 0 | 0 | 5 |
| system / global | 25 | 46 | 28 | 1 | 0 | 0 | 0 | 0 |

The counts are substring matches over function **names**, so they measure vocabulary, not checks. (The `task`
column is functions with "Task" in the name; the canonical 35 story tasks are in §3.) The point is that the
systems are present in every chapter and that the per-chapter scripts are the place to hook.

### 1.3 `.big` — APT (Flash-like) UI

`data/gui/` holds 60+ bundles. The quest half is the interesting one:

| Bundle | What it is |
|---|---|
| `poa_startup_xbox.big`, `poa_fe.big` | front end, debug-menu key handler, save/load flow |
| `poa_ig_quests.big` | Quests page (uses `Task1_Display`…`Task4_Display`, `Task_Done_1…4`) |
| `poa_ig_tasks.big` | Tasks page |
| `poa_quest_card_album.big` + `pqca_*.big` ×10 | card album, **10 sets**: Witches, Wizards, Vampires, Goblins, Hags, Giants, Dragons, Beasts, Classical Beasts, Quidditch |
| `poa_quest_folio_bruti.big` | Folio Bruti (16 beasts) |
| `poa_quest_chall_scores.big` | challenge scores/ratings for the minigames |
| `poa_quest_confis_items.big` | Fred & George's confiscated-items list |
| `poa_quest_herm_homework.big` | Hermione's statue-list homework |
| `poa_ig_remembral.big` | Remembrall screen (statues, tasks) |
| `poa_ig_inventory.big` | inventory |
| `poa_ig_maps.big` | map page |
| `poa_mini_*.big` ×5 | broom flight, duelling, fishing, hippogriff, owl racing |
| `poa_debugmenu.big` | developer chapter/room select |

Note: the M4 handoff says "no inventory/map screen exists in PoA (pause = Resume/Save/Options/Quit)". The bundles
above say otherwise — there is an Inventory page and a Maps page. Whether they are reachable in the retail build
(button, or only from the debug menu) is an **open question** and matters for the AP UI (checks page, travel page).

### 1.4 Text

`data/english/variables/Apt_Text.txt` is one `&`-separated table of `KEY=value` pairs: **6,571 keys** — 4,356
`SV3_*` dialogue keys and 2,215 `GUI_*` UI keys. This is where the game names its own systems; the census in
[02-census.md](02-census.md) is built from it.

`LS_Text.txt` is loading-screen text; `LOCALE_*.loc` are per-language tables.

### 1.5 Save

`SaveGame-0/1/2` (16 KB each) under `saves\UDATA\4541004c\<save-id>\`, title id `0x4541004C`. The in-memory
progress block and the file layout are **not mapped yet**; the HP1/HP2 ports both needed this for save lineage and
AP delivery. For PoA the save entry points are known (`sub_001A85A0`/`sub_001A85E0` open `D:\%s` through the
fopen-like `sub_0013F680`) but the serialise/deserialise functions are an open item.

## 2. The chapter and room model

`data/misc/system_chapterroom.hpb`'s `Init` (10,960 bytes of code) contains the master table. The **names** are
certain; the id pairing in my extraction drifts where two names share one preceding integer, so treat ids as
"small integers, verify live from the debug menu":

**Chapters (12 playable + extras):**
`Intro`, `HogwartsExpress`, `Night0`, `Day1`, `Night1`, `Day2`, `Night2`, `Day3`, `Night3`, `Day5`, `Night5`,
`Day6`; plus `Minigames`, `SinglePlayer_DuelClub`, `MultiPlayer_DuelClub`; plus 18 `TestChapter_*` developer
chapters (AH, AW, BB, CW, GW, JTB, PB, PS, SW, TG×6, WJ, WL, TM).

**Rooms** (~50 named, matching `plfs/`): EntranceHall, GreatHall, GrandStaircase, the floors
(1,2,3,4,6,7 — no 5), GryffindorCommonRoom, DadaClassroom, PotionsClassroom, TransfigurationClassroom,
CharmsClassroom, HistoryClassroom, MuggleStudies, HospitalWing, FredAndGeorgesOffice, DungeonCorridor,
LowerDungeon, Exterior, Exterior_OwlRacing, Exterior_CMOC, Exterior_HippogryphFlying, Exterior_CarpeRetractum,
QuidditchStadium, QuidditchPractice, GreatHallForHouseCup, GlaciusChallenge, MaraudersChallenge,
CarpeRetractumChallenge, CarpeRetractumTheReturn, PatronusChallenge, ShriekingShack/Interior/Tunnel,
DementorsBattle, Exterior_RescueBlack, Exterior_N5BSTART_01, plus test rooms.

Room counts per chapter (from `plfs/`): intro 2, night0 8, day1 20, night1 19, day2 24, night2 20, day3 26,
night3 24, day5 26, night5 28, day6 26, minigames 5, duel clubs 2.

**Day 4 / Night 4 do not exist as chapters.** Speech banks `day4.sdt` and `night4.sdt` exist on the disc, but there
is no `day4_*.hpb`, no `day4-*.pkg`, and no `Day4`/`Night4` entry in the chapter table. Either cut content or an
alias for another chapter — open, and harmless for the design (treat as absent).

### 2.1 Chapter scripts

Each chapter has an `*_initialisechapter.hpb`; 10 of the 12 also have an `*_endchapter.hpb` (Minigames has one too;
Intro and the Hogwarts Express have none):

- `InitialiseVars`, `InitialiseChapter`, `DeinitialiseChapter`, `SetStartRoom`.
- `intro_initialisechapter` additionally has `AddSpells` — spell granting is chapter-script state, not a global.
- `*_endchapter.hpb` has `EndChapter_StartNextChapter`.
- `day3_chaptersavestate.hpb` and `night5_chaptersavestate.hpb` are explicit **chapter start-state snapshots**:
  `Enter`, `Sh_CreatePlayerCamera`, `Sh_CreateCombatManager`, `_Support_SetupHarryRonHermione`,
  `_Support_InitCapabilitySets` (952 bytes — the capability/spell set). These are the closest thing the game has
  to "start chapter N with the right state", and the obvious model for AP's seeded story state.

All chapter scripts also carry the shared boilerplate `Sh_*` and `_Support_*` functions. That is the chapter
bootstrap every AP load must go through.

## 3. The task / mission layer

`GUI_TASKLIST_<chapter>_00N` keys are the game's own mission list — **35 tasks across 11 chapters**, at most 4 per
chapter, displayed on the Tasks page via `Task_Done_1…4` / `Task1_Display…Task4_Display`.

| Chapter | Tasks |
|---|---|
| HOGEX | Follow Ron to find Scabbers · Defeat Malfoy, Crabbe and Goyle · Get Harry to Professor Lupin |
| Night0 | Show the Folio Bruti to Ron · Practise Expelliarmus · See Fred and George · Find the Marauder's Map |
| Day1 | DADA classroom · Collect the Glacius spellbook · Potions class (Wiggenweld) · Broom Flying class |
| Night1 | Rescue Neville from the ghoul · Return to the common room |
| Day2 | Care of Magical Creatures class |
| Night2 | Follow Ron into the Dungeons · Escape to the Entrance Hall |
| Day3 | Collect Antidote ingredients · Mix the Antidote · Charms class · Collect the Carpe Retractum spellbook |
| Night3 | Meet Lupin (History of Magic) · Get the Expecto Patronum spellbook · Open the Gunhilda statue · Return to the common room |
| Day5 | Collect Girding ingredients · Mix the Girding Potion · Collect the Quidditch ball crate · Take it to the stadium |
| Night5 | Visit Hagrid · Protect Sirius · Repel the Dementors · Fly Buckbeak to the Dark Tower |
| Day6 | Complete the collector's cards · Earn house points · Attend the House Cup |

`GUI_TASKLIST_ENDDAY_001` = "End Day". Chapters end by going to bed (`GotoBed` / `GotoBedAvailable` variables,
`GoToBed_StartReminderTimer` in the scripts) — that is the chapter gate, not a level exit.

This gives the AP design its **mission vocabulary**: 35 canonical tasks, each with a name, a chapter and (usually)
one room. They are the natural "story mission = check + gate" units the design calls for.

## 4. Verified code seams

Every address below is a detected function start in the USA image.

### 4.1 The APT variable setter — `sub_001AD3B0` (pointer at `0x38A2F8`)

The front end, the quest pages and the gameplay scripts all publish state to the UI by name through the function
pointer at `0x38A2F8`; its value is `sub_001AD3B0(name, value)` (102 bytes, 2 cdecl args; `recomp_0011.c:14219`).
`hle_debug.c` already calls it to set `DebugMenu` to "true".

| Property | Value |
|---|---|
| name | `const char*` at `[ebp+8]` |
| value | `const char*` at `[ebp+0xC]` (strings; the card string is a 52-char bitstring) |
| call sites reading the pointer | **265** in the generated code |
| evidence | `recomp_0000.c:58927` (sets `WizardCards`), `recomp_0011.c:14219` (definition), `hle_debug.c` (existing use) |

Why it matters: this one callee sees every UI-visible state change, including the ones the census needs —
`WizardCards`, `Beans_Amount`, `Housepoints`/`House_Points`, `FolioBrutiPagesCollected`, `Lost_And_Found`,
`Task_Done_1…4`, `ChapterId`, `GotoBedAvailable`, minigame availability flags, potion-full flags. A passive
override that logs `(name, value)` and calls `_gen` is the cheapest possible check detector and cannot desync the
game.

### 4.2 Chapter / room travel — `sub_00115390`

The front end's load path reads four globals and calls the loader:

```
0x376B44  chapter id
0x376B48  room id
0x376B4C  start-point name (char*)
0x376B50  "load requested" flag
```

Call site: `recomp_0000.c:73029-73175` (`sub_00115390(chapter, room, startPoint, flag)`). The debug menu writes the
same three values in its `DM_Selection` handler (`sub_0001B800`) and sets `0x376B50`; the loader consumes them.
So **travel is a solved problem**: any AP screen can write the four globals, or call `sub_00115390` directly.

### 4.3 Cards — 52 slots behind two bit arrays

- `0x3661C0` and `0x3661C4` are **pointers** to two byte arrays; `sub_00071840(id)` (`recomp_0002.c:74938`) computes
  `1 << ((id-1) & 7)` and tests `array[(id-1) >> 3]` in both.
- `sub_00020010`, `_00020022`, `_00020024`, `_00020028`, `_0002002A`, `_00020030`, `_00020034`
  (`recomp_0000.c:58859-59332`; the last is a split tail that jumps back into `_00020030`) each build a
  **52-character** `'0'/'1'` string and publish it as `WizardCards` (seven UI pages/categories).
- A script-visible query exists at `sub_0008D3A0`: string → id (`sub_001A8C30`) → `sub_00071840` → result.
- The arrays are heap allocations (pointer globals) with a refcount at `0x3661C8` and teardown in
  `sub_0008F2E0` — i.e. a small card module, not a fixed table.

**52 slots, while the narrator text names 50 cards (10 sets × 5).** The two extra are presumably the "Rare
Collector's Card" rewards from Fred & George. Open item: confirm the per-set sizes from the card table or a 100%
save.

### 4.4 Quest queries

The XBE string table (0x2B5E00–0x2B7D00 region) is the APT variable/function vocabulary. Quest-relevant names:

`WizardCards`, `CheckCard`, `HasFolioBrutiPage`, `FolioBrutiPagesCollected`, `FolioBrutiPageCount`, `FolioBruti`,
`GEOnQueryTaskList`, `GEOnQueryStatuesInRemembrall`, `GEOnRequestRememberallAvailability`, `GEOnQueryLostAndFound`,
`Lost_And_Found`, `RequestSaveGameAvailability`, `SaveGameAvailable`, `SwitchToInventoryScreen`,
`AptCallFncActivateInventory`, `DisableInventory`, `Housepoints`, `House_Points`, `beans`, `Beans_Amount`,
`Task_Done_1…4`, `Task1_Display…4`, `ChapterId`, `GotoBed`, `GotoBedAvailable`,
`HousePointGradeDistinction/APlus/A/B/C`, `SeaHorseFishing`, `HippogriffFlyingAvailable`, `OwlRacingAvailable`,
`BroomFlyingAvailable`.

These are string addresses, not hook points — the hook is the setter in 4.1 that receives them.

### 4.5 Chapter start / end

`CScriptSystem-StartChapter` and `CRoomManager-StartChapter` profiling markers exist
(`---BEGIN---`/`---END---`). The markers themselves are not referenced from generated code directly (probably
through a macro/table), but they confirm the two classes and that a chapter start goes through both.

## 5. What the recompilation permits

The port is a faithful whole-function translation. Hand-written code **replaces** functions and may call the
generated body as `sub_XXXXXXXX_gen`; generated code is never edited. Consequences for AP:

| Want | Legal? | How |
|---|---|---|
| observe a UI state write | yes | override `sub_001AD3B0`, log, call `_gen` |
| travel to a chapter/room | yes | write 0x376B44/48/4C/50 or override/call `sub_00115390` |
| test if a card is owned | yes | override `sub_00071840` or read through `0x3661C0/C4` |
| observe a script function (`WizardCard_Callback_OnPickup`) | **not yet** | needs the `.hpb` VM's call entry — the VM is not mapped |
| give an item / add to inventory | **not yet** | the grant function is not identified; the card module only tells us where cards live |
| read/write save state | **not yet** | save serialise/deserialise not mapped |
| inline-patch a generated function | no | must replace the whole function (or use `patch_gen.py`, which is for vtable/entry patches) |

Rules the hook must satisfy: every address a function start (`check_no_foreign_addrs.py`), new override files
listed in `regen.sh --exclude-manual`, no game data in git, no behaviour change when AP is off, one game instance.

## 6. Gaps (what this audit does **not** know)

1. **Item grant.** HP1 `sub_00041D20(item)`, HP2 `sub_00081A10(item)` have no PoA equivalent yet. The script
   strings show the game's item ids are handed around by name (`WiggenweldPotionFull`, `Gryffindor_Badge`, …), and
   the card module is separate — so the grant is probably a script-side inventory add with a native helper.
2. **Progress block / save layout.** 16 KB file, unknown structure; needed for session persistence and for
   "restore AP state after a chapter reset".
3. **Script VM.** 95,554 functions, called by name; the dispatcher is not mapped. Without it, script-level checks
   (task completion, mission starts, card pickup callbacks) must be inferred from UI variables.
4. **Pickup placement.** Manifests list classes, not instances; the object tables are in `.pkg` (format
   undecoded) or built by scripts (model-name strings present).
5. **Card per-set counts**, challenge-shield count, Day4/Night4, and whether the Inventory/Quests pages are
   reachable in the retail UI.
6. **Chapter-init side effects.** Whether `InitialiseChapter` resets the AP counters (beans, points, cards, spells)
   or only story flags. This decides whether AP state can be re-applied after load or must be replayed.
7. **Mission completions.** `Task_Done_n` is UI state; the underlying task system may be script variables. The
   chapter's task list is the canonical 35, but each task's completion signal needs a live trace.

## 7. Evidence appendix

| Claim | Where |
|---|---|
| virtual = file + 0x27C0 in `.rdata` | `"false"` file 0x2B5F58 → 0x2C8718; `"true"` 0x2B5F70 → 0x2C8730 (both already used by `hle_debug.c`) |
| `sub_001AD3B0(name,value)` | `recomp_0011.c:14219`; pointer read at 0x38A2F8 (265 sites) |
| loader `sub_00115390(chapter,room,start,flag)` | `recomp_0000.c:73029-73175` |
| card test `sub_00071840(id)` | `recomp_0002.c:74938`; arrays 0x3661C0/0x3661C4 |
| 52-char `WizardCards` | `recomp_0000.c:58859` (`cmp esi,0x34`), seven variants to 59332 |
| chapter/room table | `data/misc/system_chapterroom.hpb`, `Init` constant pool |
| task lists | `Apt_Text.txt` keys `GUI_TASKLIST_*` (35 entries) |
| 10 card sets | `Apt_Text.txt` `GUI_COLLECTORS_FETXT_003…012`; `pqca_*.big` ×10 |
| 16 Folio beasts | `GUI_BRUTI_TITLE_001…016` |
| 10 Lost & Found items | `GUI_QST_LAF_001…010`; XBE strings `Glass_Jar` … `Gryffindor_Badge` |
| 9 statues | `GUI_STATUES_FETXT_001…009` |
| 8 ingredients | `GUI_QST_POT*` ×8 |
| chapter save states | `day3_chaptersavestate.hpb`, `night5_chaptersavestate.hpb` (`_Support_InitCapabilitySets`) |
| debug-menu travel | `hle_debug.c`, re-notes "Debug menu"; `DM_Selection` `sub_0001B800` |
