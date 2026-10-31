# 10 — Implementation log

## Phase 1: catalog + apworld (2026-10-31, Claude)

No port code changed; no build and no game run were needed.

### What was built

| Piece | Where |
|---|---|
| Catalog generator | `port/ap/make_catalog.py` |
| Catalog | `port/ap/catalog.json` and its verbatim copy `apworld/hppoa/data/catalog.json`, version **6f022b02f338** |
| ID lock | `port/ap/ids.lock.json` (every key ever issued; never edit by hand) |
| Catalog tests | `port/ap/test_catalog.py` |
| apworld | `apworld/hppoa/` (`archipelago.json`, `__init__.py`, `options.py`, `data/`, `docs/`, `test/`, `test_world.py`) |

The generator reads every name from the game's `Apt_Text.txt` under `--game-dir` (default
`C:\Users\unowi\DecompProject\hp3_recomp\game`, or `HPPOA_GAME_DIR`); the committed catalog holds names, keys and
IDs only. The full as-built contract, including every place it differs from 08 §1–12, is
[08 §13](08-catalog-spec.md#13-as-built-phase-1-2026-10-31).

### Counts

221 locations, 137 item names. tasks 28 · chapters 12 · spellbooks 4 · cards 50 · card_sets 10 · folio 16 ·
lost_and_found 10 · statues 9 · potions 11 · minigames 8 · marauders_challenge 10 · fred_and_george 12 · shop 13 ·
secrets 7 · house_points 10 · bean_milestones 11; the census-open groups are empty. Minimal = 44 checks,
Standard = Maximum = 221 until the open groups fill.

Item pool: 8 chapter passes, 10 area passes, 9 spells, 9 story tokens (Intro/HOGEX/Night0/Day1 passes, Flipendo and
the HOGEX/Night0 tokens are pre-collected); with their groups on, 50 cards, 16 folio pages, 10 L&F items,
6 Endurance upgrades, 3 Dark Forces chapters and 6 shop upgrades; filler for the rest.

### Commands and results

```powershell
cd C:\Users\unowi\DecompProject\hp3_ap
python port\ap\make_catalog.py            # writes both catalog copies and the lock; --check verifies
python port\ap\test_catalog.py            # 10 tests, OK

# AP 0.6.8 needs Python 3.13. The sandbox blocks %TEMP% for that interpreter, so point TEMP at the runtime folder.
$rt = "C:\Users\unowi\DecompProject\out\hppoa-rt"; mkdir -Force "$rt\tmp"; $env:TEMP = "$rt\tmp"; $env:TMP = $env:TEMP
C:\Users\unowi\DecompProject\out\ap-python313\Scripts\python.exe apworld\hppoa\test_world.py `
  --ap C:\Users\unowi\DecompProject\out\ap-0.6.8 --runtime $rt --generate 31
```

- `test_catalog.py`: 10 tests OK (IDs unique and in range, keys, well-formed locations, presets monotone, counts,
  spells, canonical version, bumped version detected, apworld copy verbatim, ID stability, catalog current).
- `test_world.py`: 37 tests OK. Per option set (standard, minimal, maximum, overrides, trimmed standard, cards goal,
  house cup goal, no spell shuffle): AP's own fill / all-state-reaches-everything / empty-state checks, plus counts
  and a balanced pool, IDs, every group has an option, no spell locks itself, a bumped or edited catalog is refused,
  seeded chapter ends open at the start, slot-data contract.
- Seed 31: one Maximum and one Minimal slot generate offline (267 locations including the two goal events).

### Deviations and why

- **Spellbooks group.** 08 §5 has no group for the four spellbooks that are not tasks, but the nine spell checks
  must exist in every preset, so they are an always-on group without an option.
- **Chapter ends of seeded chapters.** The decision is 12 chapter-end checks, but Intro/HOGEX/Night0 are seeded
  complete. Their checks are reachable from the start, and the port sends them when a seed begins.
- **Item kinds.** Collectibles (cards, folio, L&F) are kind `upgrade` with a `collect` field, keeping §2's six kinds.
- **Hermione's homework** (every 3 statues) is not a check yet; the statue count is the decided 9. The F&G statue
  track (3/6/9) is in `fred_and_george`.
- **Minigames** are one check per mode until the grade tiers are known.
- **Doc fixes:** 7 secrets (not 6) and 6 Endurance card sets (not 5), from `GUI_LOCTXT_*` and `GUI_COLLECT_*`.
- The apworld is in this repo (`apworld/hppoa`), the source of truth; `test_world.py` zips it straight from here,
  so no APWorldMaker mirror is needed.
- The port-side refusal of a different catalog version is Phase 2 (HP2's `port/ap/embed.cmake` is the model). The
  apworld already refuses a catalog whose version is not the hash of its content.

### Open (none block Phase 2)

- Spell ids are the 08 §10 candidates; one live grant decides them.
- `item_add` args are null until Phase 4 finds the grant.
- The logic guesses in 08 §13 want a play check.
- Save block layout: Austin supplied PoA saves from other platforms (PS2 `.max` EU BESLES-52440 and US
  BASLUS-20926, GameCube `.gci` GAZE69) in `C:\Users\unowi\.claude\uploads\d04bd280-0a45-4841-bbe1-96865fd45e90\`.
  The GCI carries `GUI_SAVE_TXT_013` / `GUI_TASKLIST_DAY6_*` tags and looks compressed. Diffing them against an Xbox
  save from the same day may pin the progress block (Phase 2/4). Keep them out of git.

### Next three tasks (Phase 2)

1. **Bridge foundation:** port HP2's in-process bridge (WebSocket thread, frame coordinator, sidecar, offline queue),
   embed `catalog.json` at build time with the version fence, slot-data handshake. Done = connects to a local server,
   logs one received item, refuses a bumped catalog.
2. **Pause "Archipelago" travel page:** list the held chapter/area passes; travel through `sub_00115390` /
   `0x376B44..50`. Done = travel between two chapters with no black screen, live with Austin.
3. **Return to the common room (Floo retreat):** same loader to `GryffindorCommonRoom` or the chapter start, from
   pause and a shortcut, refused in cutscenes/loading/minigames. Done = retreat from a mission, live with Austin.
