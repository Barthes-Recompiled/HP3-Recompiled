# 10 — Handoff: Phase 1 done (catalog + apworld)

Written 2026-10-31 by Claude after Phase 1 of [09](09-handoff-to-claude.md). No port code changed and no game run was
needed or done.

## What exists

| Piece | Where | State |
|---|---|---|
| Catalog generator | `port/ap/make_catalog.py` (branch `ap/impl`, off `ap/headwork`) | builds `catalog.json` **94d5b65540b6**: 220 locations, 137 item names |
| ID lock | `port/ap/ids.lock.json` | every key ever issued; never edit by hand |
| Catalog tests | `port/ap/test_catalog.py` | 9 pass (`python test_catalog.py`) |
| apworld | `C:\Users\unowi\Documents\APWorldMaker-hppoa\games\hppoa` (APWorldMaker branch `hppoa/initial`, local only, like `hpcos/initial`) | 35 tests pass; Maximum + Minimal two-player seed generates |
| Spec as built | [08 §13](08-catalog-spec.md#13-as-built-phase-1-2026-10-31) | every divergence from 08 is listed there |

The apworld lives in APWorldMaker next to hpps/hpcos rather than in this repo's `games/` (09 §4 said `games/hppoa/`);
that keeps all of Austin's worlds in one packager.

## Commands

```powershell
# regenerate (after editing a table in make_catalog.py); --apt-keys verifies every cited text key
cd C:\Users\unowi\DecompProject\hp3_ap\port\ap
python make_catalog.py --apt-keys ..\..\..\out\ap\apt-keys.tsv
python test_catalog.py
copy catalog.json C:\Users\unowi\Documents\APWorldMaker-hppoa\games\hppoa\world\hppoa\data\catalog.json

# world tests + an offline seed (AP 0.6.8 needs Python 3.13; the sandbox blocks %TEMP%, so point TEMP at the runtime)
$rt = "C:\Users\unowi\DecompProject\out\hppoa-rt"; mkdir -Force "$rt\tmp"; $env:TEMP = "$rt\tmp"; $env:TMP = $env:TEMP
cd C:\Users\unowi\Documents\APWorldMaker-hppoa\games\hppoa
C:\Users\unowi\DecompProject\out\ap-python313\Scripts\python.exe test_world.py --ap C:\Users\unowi\DecompProject\out\ap-0.6.8 `
  --runtime $rt --catalog C:\Users\unowi\DecompProject\hp3_ap\port\ap\catalog.json --generate 31
```

`--catalog` refuses to run when the world's copy differs from the port's. `port/ap/*.json` is LF in every checkout
(`.gitattributes`), so the byte compare holds on Windows.

## Shape of the world

- Regions: Menu → Hogwarts → 12 chapter regions (pass each; Intro, Hogwarts Express, Night 0 and Day 1 passes are
  pre-collected) and 10 area regions (area pass + its chapter's pass). Place-less checks sit in Hogwarts.
- Items: 8 pool chapter passes, 10 area passes, 9 spells (Flipendo at start), 9 story tokens (2 at start), then the
  group-bound items (50 cards, 16 folio pages, 10 L&F items, 6 Endurance, 3 Dark Forces chapters, 6 shop upgrades)
  only when their group is on, then filler. Progression never gets dropped; a slot with too few checks for its
  progression is refused with an OptionError.
- Options: `check_slop` (minimal ≈ 40 checks, standard 220, maximum = standard until the census groups fill),
  `include_<group>` (preset/on/off), `goal` (dark_tower / house_cup / cards), `tokens_required`, `cards_goal`,
  `shuffle_spells`.
- Slot data: `catalog`, `schema`, `start`, `mode` ("areas"), `goal`, `tokens_required`, `cards_goal`,
  `shuffle_spells`, `groups` (every group → bool; the port sends only enabled checks).

## Open (none block Phase 2)

- Spell ids are the 08 §10 candidates; one live grant decides them.
- `item_add` args are null; Phase 4 finds the grant.
- Logic guesses listed in 08 §13 (Shrieking Shack on the Night3 end, Carpe for seahorses, Mucus for snot, milestone
  and shop pass counts) want a play check.
- The port-side version fence (refuse a different catalog) is Phase 2; HP2's `port/ap/embed.cmake` is the model.

## Next three tasks

1. **Bridge foundation (Phase 2.1):** port HP2's in-process bridge (WebSocket thread, frame coordinator, sidecar,
   offline queue), embed `catalog.json` at build time with the version fence, slot-data handshake. Done = connects to
   a local server, logs one received item, refuses a bumped catalog.
2. **Travel page + retreat (Phase 2.2–2.3):** pause "Archipelago" page listing held passes; travel through
   `sub_00115390` / `0x376B44..50`; "Return to the common room". Done = live test with Austin.
3. **Set-variable observer (Phase 3.1):** override `sub_001AD3B0`, log the whitelist, map to catalog keys, send one
   card, one task and one statue check exactly once.
