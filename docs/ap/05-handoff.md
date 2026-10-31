# Handoff — HP3 Archipelago headwork, 2026-10-31 (night)

You asked for the audit + design headwork, docs only, on a branch, without touching `main`. This is what is on
`ap/headwork` in `hp3_recomp` (worktree `hp3_ap`).

## Where things are

- Branch **`ap/headwork`**, worktree `C:\Users\unowi\DecompProject\hp3_ap`, based on `github/main` **09d89af**
  (which is three commits ahead of the local `main` — settings-parity was merged on GitHub).
- Deliverables: `docs/ap/README.md`, `01-audit.md`, `02-census.md`, `03-design.md`, `04-roadmap.md`, this file.
- Also edited: `docs/INDEX.md` (Archipelago section), `docs/re-notes.md` (randomizer-relevant finds).
- Nothing else changed; no game data, no generated code, no build.

## The short version

The game is much richer in check material than either HP1 or HP2, and most of it is **named in the game's own UI
text**, so the census did not need guesswork:

- **12 chapters** (Intro, HogwartsExpress, Night0, Day1, Night1, Day2, Night2, Day3, Night3, Day5, Night5, Day6)
  plus Minigames and two duel clubs; **~50 rooms**; the full chapter/room table is in
  `data/misc/system_chapterroom.hpb`'s `Init` constant pool. No Day4/Night4, though their speech banks exist.
- **35 story tasks** (`GUI_TASKLIST_*`), ≤4 per chapter — the mission vocabulary.
- **52 card slots** (10 sets; 50 narrated cards; the 2 extra are probably the rare F&G cards).
- **16 Folio Bruti**, **10 Lost & Found**, **9 statues**, **8 ingredients / 3 potions**, **6 bonus activities**,
  **a Marauder's Challenge in every chapter**, **4 Fred & George tracks**, **6 named secret passages**.
- **52-character `WizardCards` string** built by seven functions; card bits live behind pointers
  `0x3661C0`/`0x3661C4`, read by `sub_00071840`.

The two code seams that make this feasible:

1. **`sub_001AD3B0(name, value)`** — the APT set-variable function behind the pointer at `0x38A2F8` (265 call
   sites). One passive override sees every UI state write: cards, beans, house points, folio, L&F, tasks, chapter,
   minigame availability. This is the check detector.
2. **`sub_00115390(chapter, room, startpoint, flag)`** — the level loader; the debug menu already drives it through
   `0x376B44/48/4C/50`. This is the travel mechanism.

Still open (ranked): the **item grant / inventory add** (HP1 `sub_00041D20`, HP2 `sub_00081A10` analogue), the
**save/progress block** (16 KB save, layout unmapped), the **script VM dispatcher** (95,554 `.hpb` functions called
by name), and **pickup placements** (manifests name classes, not instances).

## Design in one paragraph

Start at **Day 1** with the earlier chapters seeded complete. Chapter passes open chapters (KH world visits);
mission passes open the 12–18 missions with real doors (FF12 story events); spells open traversal and are mission
prerequisites. Completing a mission is an **Event location** that yields a **Story Token**, and the finale
(the Dark Tower, Night 5) needs **9 of 12 tokens + the Night 5 pass + Expecto Patronum** — the OoT medallion
shape. `open_castle` on means all rooms of a held chapter are free; only mission doors and spell doors are gated.
Check slop is one preset (`minimal` / `standard` / `maximum`) over granular `include_*` groups; talk and pursuit
checks are **maximum only**.

## What I did not do

- **No game run.** You were asleep and the rules say tell you before a run opens on your screen. Every code claim
  is static (generated code + XBE strings + disc data); nothing here is confirmed live.
- No bridge, apworld, catalog or hook code — docs only, as agreed.
- No EUR work.

## Next session, in order

1. **Card count sanity**: confirm 52 (two extra slots) from the card table or a save with some cards; if it is 50,
   the catalog version changes once.
2. **Pickup census**: start with the script route — room `.hpb` files contain pickup model names; key
   `room + model + position` the way `hp1_recomp`/`hp2_ap` did with GEO tables. The `.pkg` object tables are the
   fallback.
3. **Find the item grant**: grep the generated code for string references to the variable names
   (`WiggenweldPotionFull`, `Gryffindor_Badge`, …) and follow the writers; the card module is a separate, small
   class and is a good warm-up.
4. **First live test** (with Austin): the debug menu already jumps chapters; warp Day1 → Day3 → Night5 and watch
   for script breakage. This answers the "seeded chapter start" question without any AP code.
5. Then Phase 1 of the roadmap: catalog generator + apworld skeleton.

## Useful commands

```powershell
# docs worktree
cd C:\Users\unowi\DecompProject\hp3_ap
git status

# rebuild the docs branch state (docs only, no build needed)

# census scripts used for the audit (not committed, in ../out/ap):
py -3 out\ap\scan_hpb.py        hp3_recomp\game\data\misc hp3_recomp\game\data\packages\plfs out\ap
py -3 out\ap\census2.py         hp3_recomp\game\data\misc hp3_recomp\game\data\packages\plfs out\ap
py -3 out\ap\apt_text.py        hp3_recomp\game\data\english\variables\Apt_Text.txt out\ap
py -3 out\ap\chapter_table.py   hp3_recomp\game\data\misc\system_chapterroom.hpb

# one PoA script's functions + constants
py -3 -I "Harry Potter Xbox Decomps\tools\hpbfn.py" hp3_recomp\game\data\misc\day1_entrancehall.hpb
```

## Open questions for Austin

1. Start chapter: Day 1 (recommended), Night0, or random?
2. Mission passes: ~12 major missions, or all 35 tasks?
3. Goal: Dark Tower with 9/12 tokens (recommended), House Cup, or cards?
4. Talk/pursuit only in Maximum — confirm.
5. Travel UI: pause-menu Archipelago page, debug menu, or hotkey?
6. Do vanilla rewards stay when the same thing is an AP check?
