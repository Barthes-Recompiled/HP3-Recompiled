# 03 — Design: start state, open castle, story and gamestates

Status: **proposal**. Nothing here is built; the numbers it leans on are in [02-census.md](02-census.md).

## 1. Axioms

1. **Check slop is the point**, with options to trim — never a fixed list. Default = Standard; Maximum is for the
   completionist run; Minimal is for races.
2. **Open castle**: the world is open from the start *as far as the items allow*. A chapter you can reach is a
   chapter whose rooms you can explore; abilities (spells) and mission passes are what open individual things.
3. **Story missions are both checks and gates.** Completing one sends a check **and** yields a progression item
   that other content requires.
4. **The vanilla game must still work.** No server, no seed, no AP: the port behaves like the port.
5. **One source of truth for IDs**: `catalog.json`, version-fenced, refused by the exe if it differs.

## 2. The shape

```
            ┌───────────────────────────────┐
            │  Chapter passes (12)          │  KH world visits / ME planets
            │  "Day 1", "Night 3", …        │  → region + travel destination
            │  11 in the pool at start      │
            └──────────────┬────────────────┘
                           │
            ┌──────────────▼────────────────┐
            │  Mission passes (~12–18)      │  FF12 story events
            │  "DADA Class", "Marauder's    │  → one door / trigger
            │   Map", "Shrieking Shack"     │
            └──────────────┬────────────────┘
                           │
            ┌──────────────▼────────────────┐
            │  Spells (10)                  │  Zelda dungeon items
            │  Lumos, Glacius, Expelliarmus │  → traversal + mission prereq
            │  Carpe Retractum, Draconifors │
            │  Snufflifors, Reparo, Patronum│
            │  Mucus Projectum (shop)       │
            └──────────────┬────────────────┘
                           │
                  ┌────────▼────────┐
                  │ Mission complete│  Event location = check
                  │  + Story Token  │  item = gate for later content
                  └────────┬────────┘
                           │
                  ┌────────▼────────┐
                  │  Dark Tower     │  goal: N tokens + Night5 pass
                  │  (Night 5 end)  │  + Expecto Patronum  (OoT medallions)
                  └─────────────────┘
```

Three item classes, three jobs: **access** (chapter passes), **permission** (mission passes), **ability**
(spells). One reward class (story tokens) that the goal consumes.

## 3. Start state

**Default: Day 1, Gryffindor common room** — the first fully-open castle day. The Intro, Hogwarts Express and
Night0 are *seeded as complete* (their flags are written, their passes pre-collected) so the game starts in a
coherent Day 1 state, exactly as if the player had played them.

Options:

| Option | Values | Default | Effect |
|---|---|---|---|
| `start_chapter` | `intro`, `night0`, `day1`, `random` | `day1` | Where a new seed starts; the chapter's pass is pre-collected. |
| `skip_intro` | on/off | on | Skips the Privet Drive/Express opening when starting later (the port already has menu-level skipping). |
| `starting_spells` | `flipendo`, `lumen`… | `flipendo` | The one spell that must be known to not soft-lock the movement tutorial. |

Why not the Intro? It is a cutscene-heavy prologue with no checks of its own beyond the story; it makes a poor
randomizer start and a worse "open castle" statement. Why not Night0? It is the tutorial night; Day1 is the first
day where all four task types are available.

**Seed state carried into the start:** the Day 1 `InitialiseChapter` script runs normally (it is the game's own
state), then AP applies:
- the slot's spells and upgrades (the capability set),
- the slot's counters (beans, house points, cards, folio, L&F) — displayed, not replayed,
- the completion flags of missions the player has already done in the slot (so cutscenes do not re-fire).

## 4. The open castle

`open_castle` (default on) means:

- Holding a chapter pass makes **all rooms of that chapter** reachable through travel.
- A chapter's story flags are **seeded** to the vanilla start-of-chapter state on entry (see §6); the chapter's
  own scripts then run as if the player had arrived there legitimately.
- Within the chapter, only **specific doors/triggers** are item-gated: mission doors by mission passes, spell
  doors by spells. Everything else is free movement.
- Travel is available from the pause menu (the port's settings page already has a row-table UI to copy) and from
  the AP session; the debug menu stays as the developer path.

`open_castle` off = classic linear chapter order: only the next chapter's pass is in the pool, the rest are
pre-collected in order. Same code, different item pool.

**What is not open:** cutscenes and story beats that assume a prior event. Those are handled by seeding + mission
passes, not by making the engine ignore its flags. If a mission's script cannot run without an earlier flag, the
mission pass that opens it also sets that flag.

## 5. Story missions: checks *and* gates

Each **major mission** (one with a distinct door/trigger — DADA class, Potions class, Marauder's Map, the
Shrieking Shack, the Dementor battle, …) is:

- a **Location** in the AP world, in its chapter's region;
- an **Event** that, when completed, places a **progression item** (a Story Token) into the pool.

Two pool shapes, one option:

| `mission_passes` | Shape | Feel |
|---|---|---|
| `shuffle` (default) | mission passes are items in the pool; you may do the missions of a chapter in any order; each completion yields a token | KH/FF12 "you may now go here" |
| `chain` | mission N's completion yields mission N+1's pass; one order only | vanilla, Zelda-dungeon linear |

The 35 tasks are the source list. Not all 35 need a pass: the pass-eligible ones are those the census marks as
entering a distinct room/scene (e.g. DADA classroom, Potions classroom, Broom class, Charms, History of Magic,
Shrieking Shack, Dementor battle) — roughly **12–18**. In-room tasks (talk to X, pick up Y) are checks without a
pass.

**Chapter tokens and the goal.** Completing a chapter's story missions yields the chapter's token. The goal gate
("the Dark Tower") requires **9 of the 12 chapter tokens** + the Night 5 pass + Expecto Patronum. That is the OoT
medallion shape: the player can skip up to three chapters' stories, but not the abilities the finale needs.

## 6. Story and gamestate handling

### 6.1 The chapter lifecycle (what the game does)

```
travel/select chapter ──► sub_00115390(chapter, room, startpoint, flag)
                              │
                              ├─ CRoomManager-StartChapter
                              ├─ InitialiseVars / InitialiseChapter   (flags, spells, start room)
                              ├─ room load (plfs/<chapter>-<room>.pkg, .hpb)
                              └─ gameplay …
     end of chapter ──► EndChapter_StartNextChapter (next chapter id)
```

`InitialiseChapter` is per chapter and already contains the "right" state for that chapter (it is what the
vanilla game runs). `day3_chaptersavestate` and `night5_chaptersavestate` show the engine has an explicit
start-state form (`_Support_InitCapabilitySets` — the 952-byte capability/spell setup).

### 6.2 What AP does at each boundary

| Boundary | AP action |
|---|---|
| before travel | check the pass is held; pick the chapter/room; write 0x376B44/48/4C and the start-point name |
| on chapter load | let `InitialiseChapter` run untouched |
| after the chapter is ready (first frame / `ChapterId` write) | apply AP state: spells, upgrades, counters; set completion flags for missions already done |
| on task completion | detect `Task_Done_n` (or the underlying flag), send the check once, place the token |
| on chapter end | send the chapter-end check; grant the token |
| on item received | apply immediately if in a safe state, else queue and apply at the next chapter-ready |

The "chapter ready" point is the single most important unknown: it must be after `InitialiseChapter` (or AP state
is overwritten) and before the player can act. Candidate: the write of `ChapterId` to the UI (it happens once per
load), or the first frame after the loading screen hides.

### 6.3 Seeded story state

For chapter *N*, the seed is the vanilla state at the start of *N*:

- the chapter's own `InitialiseChapter` supplies the flags it sets;
- the chapters before *N* are marked complete in the progress block (their `EndChapter`/flag writes) so chapter *N*
  scripts that test "did the player do X" see the right answer;
- AP-completed missions inside *N* are marked done so their cutscenes do not re-fire.

The prototype of this is the game's own debug chapter select: it jumps to any chapter without replaying the story,
which proves the engine tolerates a seeded start. What it does **not** prove is that every mission script is happy
with a partial chapter; that is the live test matrix (§9).

### 6.4 Session, save and lineage

- Sidecar `saves/archipelago/session.toml` next to the native saves: seed, slot, received index, pending checks,
  catalog version.
- **Save lineage**: hash of the native save, so a profile stays bound to its seed (HP1/HP2's model).
- Offline play: checks queue; items queue; both flush on reconnect.
- A new game on the same slot does not re-grant: the session is per seed+slot, not per save (HP2's open bug; fix
  it here with lineage).

### 6.5 Failure modes and mitigations

| Failure | Mitigation |
|---|---|
| travel during a cutscene (HP2 black screen) | only allow travel from pause/menu; refuse while `IsMoviePlaying`/`IsScreenFadeActive` (both exist as variables) |
| `InitialiseChapter` resets AP counters | apply after "chapter ready", and re-check on the first UI write |
| mission script assumes a prior flag | the mission pass sets that flag when granted; keep a per-mission flag table |
| task completed out of order breaks later script | seed completion flags; if a script cannot tolerate it, demote that mission to `chain` |
| save from an AP run loaded without AP | detect lineage mismatch, warn, keep playing vanilla |
| chapter 5 / floor numbering gaps (no Floor5) | region list is data-driven from the chapter table, not hard-coded |

### 6.6 The gamestates the player can be in

"Various gamestates" is the real risk surface: the AP layer must know where the player is before it sends, gives
or travels. The states below are the ones the game demonstrably has.

| State | How the port knows | AP behaviour |
|---|---|---|
| **Front end** (title, save select, Bonus Activities menu) | `ChapterId` unset / FE scripts running | connect, flush queues, allow "start run" |
| **Loading / chapter transition** | loader entered (`sub_00115390`), loading screen up | queue everything; apply at the first "ready" |
| **Cutscene** | `IsMoviePlaying`, `CutsceneSkipped`, fade state | no travel, no item application; story checks fire from the cutscene's own end |
| **Free roam** | a room is up, no mission lock | checks live; travel allowed **from pause only** |
| **Mission active** | task script running / mission pass consumed | mission checks at completion; travel discouraged |
| **Minigame / challenge** | mode flag, minigame script, `poa_mini_*` UI | score/grade checks at result; no travel |
| **Duelling Club** (single/multiplayer) | duel-club chapter scripts | result checks; no travel |
| **Shop (Fred & George)** | shop UI open | purchases are checks/items; beans are the currency |
| **Pause menu** | pause UI (`Game Paused`) | **safe point**: apply received items, open travel page, save |
| **End of day** | `GotoBed` / `End Day` task | chapter-end check, token, next-chapter pass |
| **Death / instant restart** | `GenericPlayerDeath_*`, `CombatUtils_RestartLevel` | no rollback of sent checks; re-apply state on respawn |
| **Save / load** | save slot flow (`SaveGame-0..2`) | lineage check; re-apply state after load |
| **Disconnected / offline** | no bridge session | queue checks; queue items; never block play |

Two rules fall out of the table:

- **Items are applied only in a safe state** (pause, front end, or the first frame after a chapter is ready).
  Everything else queues. This is what prevents a delivery from being overwritten by `InitialiseChapter`.
- **Travel is only offered from pause**, never during a cutscene, loading screen, minigame or mission. HP2 learned
  this the hard way (warping during a level's opening cutscene black-screens it).

## 7. Options — "-sanity" and check slop

One preset plus granular toggles. The preset sets the defaults; the toggles override.

| Preset | Contents |
|---|---|
| `minimal` | story tasks + chapter ends + goal |
| `standard` (default) | + cards, card sets, folio, L&F, statues, potions, minigames, Marauder's, secrets, house-point and bean milestones |
| `maximum` | + world pickups, loose pickups, lessons, spell pickups, enemies, **talks**, **pursuits** |

Granular groups (each `include_*`, default per preset):

`cards`, `card_sets`, `folio`, `lost_and_found`, `statues`, `potions`, `minigames`, `marauders_challenge`,
`fred_and_george`, `shop`, `secrets`, `house_points`, `bean_milestones`, `pickups`, `loose_pickups`, `lessons`,
`spells_as_checks`, `enemies`, `talks`, `pursuits`.

Rules:
- **Talk and pursuit checks exist only in `maximum`** (and only if the group is on) — Austin's rule.
- Milestone checks (beans/points) are milestone-sized, not per-unit: `bean_milestone=25/50/100`,
  `house_point_milestone=25/50`.
- Pickup categories are off until the placement census lands, then on in Standard.

Other options:

| Option | Default | Notes |
|---|---|---|
| `goal` | `dark_tower` | `dark_tower` (9/12 tokens), `house_cup` (Day6), `cards` (N cards) |
| `tokens_required` | 9 | only for `dark_tower` |
| `cards_goal` | 40 | only for `cards` |
| `start_chapter` | `day1` | see §3 |
| `mission_passes` | `shuffle` | `shuffle` / `chain` |
| `open_castle` | on | see §4 |
| `shuffle_spells` | on | spells in the pool |
| `shuffle_upgrades` | on | endurance/broom/owl upgrades |
| `death_link` | off | out of scope for v1 |

## 8. Regions and items (concrete)

**Regions**: `Menu` → `Hogwarts` (always reachable) → one region per chapter (`Intro`, `HogwartsExpress`,
`Night0`, `Day1`, `Night1`, `Day2`, `Night2`, `Day3`, `Night3`, `Day5`, `Night5`, `Day6`) plus `Minigames`,
`Challenges`, `ShriekingShack`, `DarkTower`. Chapter edges require the chapter pass; mission edges require the
mission pass.

**Items (progression)**: 12 chapter passes (one per chapter; the start's is pre-collected, so 11 in the pool),
12–18 mission passes, 10 spells (Flipendo, Lumos, Glacius, Expelliarmus, Carpe Retractum, Draconifors,
Snufflifors, Reparo, Expecto Patronum, Mucus Projectum), 12 story tokens, 6 upgrades (5 endurance from card sets +
one broom set), 3 Dark Forces chapters, bean-bag and ammo holders.

**Items (useful/filler)**: beans, house points, Wiggenweld/Antidote/Girding vials, dungbombs, stink pellets,
chocolate frogs, cauldron cakes.

**Checks**: the census list, each with a stable key (`task:day1:dada`, `card:17`, `card_set:witches`,
`statue:gunhilda`, `folio:14`, `laf:key`, `minigame:owl:gold`, `secret:floor4_b`, `bean:250`, `point:100`, …).

**Slot data**: catalog version, `groups` map, goal, tokens required, start chapter, mission-pass mode, seed.

## 9. Live test matrix (what decides whether the design survives)

1. **Seeded chapter start**: debug-warp to Day 3, then Night 5, and play 10 minutes; do scripts, doors and
   companions behave? (The debug menu already jumps chapters; this is the cheapest first test.)
2. **Out-of-order mission**: in Day 1, do Potions before DADA; does the task list and the next chapter still work?
3. **AP state after init**: give a spell/bean/card, force a chapter reload, confirm the values survive and the
   UI shows them.
4. **Travel from pause**: write 0x376B44/48/4C from a paused state and load; confirm no black screen.
5. **Task detection**: complete one task and confirm `Task_Done_n`/the underlying flag fires exactly once.
6. **End Day**: finish a chapter and confirm the token/next-chapter transition.

## 10. Open design questions (for Austin)

1. **Start**: Day 1 (recommended), Night0, or random?
2. **Mission pass granularity**: ~12 major missions (rooms/scenes) or all 35 tasks?
3. **Goal**: Dark Tower with 9/12 tokens (recommended), House Cup, or cards?
4. **Talk/pursuit**: Maximum only — confirm.
5. **Travel UI**: pause-menu "Archipelago" page, the debug menu, or a hotkey overlay?
6. **Vanilla rewards**: when a card/statue/etc. is an AP check, does the vanilla reward stay (double reward) or
   get suppressed (cleaner, more work)?
7. **Intro/Express**: playable checks in Maximum, or always skipped?
