# 03 — Design: start state, open castle, story and gamestates

Status: **decisions taken 2026-10-31**; nothing built yet. Numbers are in [02-census.md](02-census.md).

## 0. Decisions (2026-10-31)

| # | Question | Decision |
|---|---|---|
| 1 | Start | **Day 1**, Gryffindor common room (§3). |
| 2 | Mission passes | **Area passes, not task passes**: a pass exists only for a destination the chapter's rooms cannot reach (~10). Classroom tasks are free once the chapter is open (§5). |
| 3 | Goal | **Dark Tower**: 8 of 11 chapter tokens (HOGEX and Night0 pre-collected; three chapters can still be skipped) + Night 5 pass + Expecto Patronum (§5). |
| 4 | Talk / pursuit | **Maximum only** (§7). |
| 5 | Travel | **Pause "Archipelago" page**, themed as the Marauder's Map / Floo network; fireplace-only is an option, not the default (§4). |
| 6 | Vanilla rewards | **Suppressed** when the same thing is an AP item; progression grants are exempt (§12). |
| 7 | Intro | **Playable flashback in every seed**, skipped at the start, revisitable in-game (§3.1). |
| 8 | Tracker / messages | **In-game themed tracker + check/item notifications** (§11). |

## 1. Axioms

1. **Check slop is the point**, with options to trim — never a fixed list. Default = Standard; Maximum is for the
   completionist run; Minimal is for races.
2. **Open castle**: the world is open from the start *as far as the items allow*. A chapter you can reach is a
   chapter whose rooms you can explore; abilities (spells) and area passes are what open individual things.
3. **Story missions are both checks and gates.** Completing one sends a check **and** yields a progression item
   that other content requires.
4. **The vanilla game must still work.** No server, no seed, no AP: the port behaves like the port.
5. **One source of truth for IDs**: `catalog.json`, version-fenced, refused by the exe if it differs.

## 2. The shape

```
            ┌───────────────────────────────┐
            │  Chapter passes (12)          │  KH world visits / ME planets
            │  "Day 1", "Night 3", …        │  → region + travel destination
            │  8 in the pool at start       │
            └──────────────┬────────────────┘
                           │
            ┌──────────────▼────────────────┐
            │  Area passes (~10)            │  FF12 story events
            │  "Shrieking Shack",           │  → one door / area
            │   "Dementor Battle", …        │
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

Three item classes, three jobs: **access** (chapter passes), **permission** (area passes), **ability**
(spells). One reward class (story tokens) that the goal consumes.

## 3. Start state

**Default: Day 1, Gryffindor common room** — the first fully-open castle day. The Intro, Hogwarts Express and
Night0 are *seeded as complete* (their flags written, their passes pre-collected) so the game starts in a coherent
Day 1 state, exactly as if the player had played them.

Options:

| Option | Values | Default | Effect |
|---|---|---|---|
| `start_chapter` | `intro`, `night0`, `day1`, `random` | `day1` | Where a new seed starts; the chapter's pass is pre-collected. |
| `skip_intro` | on/off | on | Skips the Privet Drive/Express opening at the start; the Intro stays a playable flashback either way (§3.1). |
| `starting_spells` | `flipendo`, `lumos`… | `flipendo` | The one spell that must be known to not soft-lock the movement tutorial. |

Why not the Intro? It is a cutscene-heavy prologue; it makes a poor randomizer start and a worse "open castle"
statement. Why not Night0? It is the tutorial night; Day1 is the first day where all four task types are available.

**Seed state carried into the start:** the Day 1 `InitialiseChapter` script runs normally (it is the game's own
state), then AP applies:
- the slot's spells and upgrades (the capability set),
- the slot's counters (beans, house points, cards, folio, L&F) — displayed, not replayed,
- the completion flags of missions the player has already done in the slot (so cutscenes do not re-fire).

### 3.1 The Intro as a revisitable flashback

Decision 7: the Intro is **playable in every seed but never forced**.

- Its chapter pass is pre-collected, so the Travel page always lists it.
- It is entered as a **flashback**: snapshot the main progress block, load the Intro chapter, and restore the
  snapshot on exit. Its `EndChapter_StartNextChapter` must not run — that is the vanilla transition that would
  advance the main story.
- **In-game access, not just a menu**: a Floo fireplace in the Gryffindor common room (and the Great Hall) opens
  the travel page; the Marauder's Map item does the same from anywhere once found in Night0.
- **Checks**: none are required. If the Intro has discrete events worth checking, they are `maximum`-only
  "memories", so no seed's logic ever depends on a flashback (open item: whether any such events exist).

The risk is that the Intro's scripts assume a fresh game. The debug chapter select proves the engine can load a
chapter out of order; the snapshot/restore is what keeps the main run's flags intact.

## 4. The open castle

`open_castle` (default on) means:

- Holding a chapter pass makes **all rooms of that chapter** reachable through travel.
- A chapter's story flags are **seeded** to the vanilla start-of-chapter state on entry (see §6); the chapter's
  own scripts then run as if the player had arrived there legitimately.
- Within the chapter, only **specific doors/triggers** are item-gated: mission doors by area passes, spell
  doors by spells. Everything else is free movement.
- Travel lives on a pause page ("Archipelago"), themed as the **Marauder's Map** with the **Floo network** as the
  fast-travel conceit: parchment, ink, footprints, a green-flame transition. The Map item (pre-collected with
  Night0) is the in-world justification; the page is usable as soon as any pass is held.
- **Fireplace-only Floo is an option, not the default.** Restricting travel to fireplaces would mean walking back to
  a common room or the Great Hall between every trip, which fights the open castle. `travel_requires_floo` (default
  off) gives the flavour run; fireplaces sit in the common rooms, the Great Hall and the staff offices. The
  fireplace interaction is also the in-game way into the page when the player does not want to pause (§3.1, §11).
- The debug menu stays as the developer path, and the AP session can request a travel from the page.

`open_castle` off = classic linear chapter order: only the next chapter's pass is in the pool, the rest are
pre-collected in order. Same code, different item pool.

**What is not open:** cutscenes and story beats that assume a prior event. Those are handled by seeding + mission
passes, not by making the engine ignore its flags. If a mission's script cannot run without an earlier flag, the
area pass that opens it also sets that flag.

## 5. Story missions: checks *and* gates

Each **story mission** is a Location in its chapter's region. Missions that start at a distinct threshold are also
gated by an **area pass**; completing a mission is an Event that yields a **Story Token**.

**The pass rule (decision 2): a pass exists only for a destination the chapter's rooms cannot reach on their own.**
Classroom lessons, "talk to X" and "return to the common room" are free once the chapter is open. The Shrieking
Shack, the challenge rooms, the Dementor battle, the Quidditch match and the Dark Tower are not. That is about
**ten** passes:

| Pass | Chapter | Destination |
|---|---|---|
| Glacius Challenge | Day1 | `day1-glaciuschallenge` |
| Marauder's Challenge | any (one pass, per-chapter rooms) | `*_marauderschallenge` |
| Carpe Retractum Challenge | Day3 | `day3_carperetractumchallenge` (+ The Return) |
| Shrieking Shack | Night3 | `ShriekingShack` / Interior / Tunnel |
| Patronus Challenge | Night3 | `night3_patronuschallenge` |
| Quidditch Match | Day5 | `day5-quidditchstadium` |
| Dementor Battle | Night5 | `DementorsBattle` |
| Dark Tower / Buckbeak Flight | Night5 | `Exterior_RescueBlack` and the final |
| House Cup | Day6 | `GreatHallForHouseCup` |
| Bonus Activities | any | the minigames menu (one access, not one per game) |

The exact list is a census item; the shape is the decision: **~10, not 35.**

**Why area passes and not task passes.** Thirty-five single-task passes would make a third of the pool keys to one
task each, and a player would spend more time finding a key than doing the thing. The classroom tasks are the
*content* of a chapter; they should not each need a key. The passes are for the places the castle cannot walk you
to — which is exactly the Mass Effect / KH "world visit" feel.

Options:

| `mission_passes` | Shape | For |
|---|---|---|
| `areas` (default) | the ten above; tasks inside a chapter are free | smooth run |
| `tasks` | all 35 tasks get a pass | maximum fragmentation / check-slop runs |
| `chain` | no passes; a mission's completion yields the next mission's pass | vanilla-like, one order |

**Seeded chapters.** The Intro, Hogwarts Express and Night0 are past at the Day 1 start. The Intro has no tasks.
HOGEX (3) and Night0 (4) do, and those seven checks are **pre-completed, not in the pool**: replaying them would
fight the seeded flags (the Marauder's Map is already owned). Total task checks in a seed: **28 of 35**. Open
question: whether to add HOGEX/Night0 flashbacks later (same mechanism as the Intro) to recover the seven.

**Chapter tokens and the goal.** The 11 task-list chapters yield a token each; HOGEX and Night0 are pre-collected,
so nine are obtainable. The Dark Tower gate needs **8 of 11** — six of the nine — plus the Night 5 pass and Expecto
Patronum. That is the OoT medallion shape: three chapters can be skipped, but not the abilities the finale needs.

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
| mission script assumes a prior flag | the area pass sets that flag when granted; keep a per-mission flag table |
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
| **Mission active** | task script running / area pass consumed | mission checks at completion; travel discouraged |
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
| `tokens_required` | 8 | only for `dark_tower` (of 11; 2 pre-collected) |
| `cards_goal` | 40 | only for `cards` |
| `start_chapter` | `day1` | see §3 |
| `mission_passes` | `areas` | `areas` / `tasks` / `chain` (§5) |
| `open_castle` | on | see §4 |
| `travel_requires_floo` | off | fireplace-only travel, for flavour runs (§4) |
| `tracker` | on | Marauder's Map page + Remembrall HUD (§11) |
| `notifications` | `overlay` | `overlay` / `game` / `off` (§11) |
| `suppress_rewards` | on | vanilla rewards suppressed when AP owns the item (§12) |
| `shuffle_spells` | on | spells in the pool |
| `shuffle_upgrades` | on | endurance/broom/owl upgrades |
| `death_link` | off | out of scope for v1 |

## 8. Regions and items (concrete)

**Regions**: `Menu` → `Hogwarts` (always reachable) → one region per chapter (`Intro`, `HogwartsExpress`,
`Night0`, `Day1`, `Night1`, `Day2`, `Night2`, `Day3`, `Night3`, `Day5`, `Night5`, `Day6`) plus `Minigames`,
`Challenges`, `ShriekingShack`, `DarkTower`. Chapter edges require the chapter pass; mission edges require the
area pass.

**Items (progression)**: 12 chapter passes (Day1, Intro, HOGEX and Night0 pre-collected, so 8 in the pool),
10 area passes (§5), 10 spells (Flipendo, Lumos, Glacius, Expelliarmus, Carpe Retractum, Draconifors,
Snufflifors, Reparo, Expecto Patronum, Mucus Projectum), 11 story tokens (2 pre-collected), 6 upgrades (5 endurance
from card sets + one broom set), 3 Dark Forces chapters, bean-bag and ammo holders.

**Items (useful/filler)**: beans, house points, Wiggenweld/Antidote/Girding vials, dungbombs, stink pellets,
chocolate frogs, cauldron cakes.

**Checks**: the census list, each with a stable key (`task:day1:dada`, `card:17`, `card_set:witches`,
`statue:gunhilda`, `folio:14`, `laf:key`, `minigame:owl:gold`, `secret:floor4_b`, `bean:250`, `point:100`, …).

**Slot data**: catalog version, `groups` map, goal, tokens required, start chapter, area-pass mode, seed.

## 9. Live test matrix (what decides whether the design survives)

1. **Seeded chapter start**: debug-warp to Day 3, then Night 5, and play 10 minutes; do scripts, doors and
   companions behave? (The debug menu already jumps chapters; this is the cheapest first test.)
2. **Out-of-order mission**: in Day 1, do Potions before DADA; does the task list and the next chapter still work?
3. **AP state after init**: give a spell/bean/card, force a chapter reload, confirm the values survive and the
   UI shows them.
4. **Travel from pause**: write 0x376B44/48/4C from a paused state and load; confirm no black screen.
5. **Task detection**: complete one task and confirm `Task_Done_n`/the underlying flag fires exactly once.
6. **End Day**: finish a chapter and confirm the token/next-chapter transition.
7. **Intro flashback**: travel to the Intro, play two minutes, return; confirm the Day 1 state is untouched.
8. **Suppression**: pick up a card/statue with AP owning the check; confirm the vanilla grant does not fire, the AP
   item does, exactly once, and the album/set-completion state is consistent.
9. **Tracker**: the pause page opens in every gamestate and shows the right counts; a toast fires for one check and
   one item; the Remembrall glows only where unchecked locations exist.

## 10. Remaining questions

1. **Area-pass list**: confirm the ten in §5 — in particular whether Marauder's Challenge is one pass for all ten
   rooms or one per chapter.
2. **Tracker shortcut**: Back is taken by the PC Settings page (settings-parity, merged). Which input opens the
   tracker from gameplay — a held combo, the Map button, or a keyboard key?
3. **Intro memories**: should the flashback carry optional checks at all, or be pure replay?
4. **Floo-only default**: recommended off (fireplace-only as an option); confirm.
5. **Text channel**: pursue the game's own text display for messages, or stop at the native overlay?
6. **Suppression scope**: confirm that progression grants (spellbooks, story, chapter ends) stay vanilla (§12).

## 11. The tracker and in-game messages

Decision 8. Two layers, both themed.

**The page (pause → Archipelago).** The Marauder's Map: parchment, ink, footprints. Tabs:

| Tab | Shows |
|---|---|
| Travel | held chapter and area passes, current chapter, the Floo/fireplace list |
| Checks | sent/total, per-group counts (`cards 17/52`, `folio 3/16`, `statues 4/9`, `tasks 6/35`, …), unchecked in this room |
| Items | received list, recent items, pending queue |
| Status | seed/slot, catalog version, connection, goal progress (tokens 5/9, cards 17/52) |

Drawn by the port (the settings row-table UI is the precedent), not by the game's APT UI; an APT-native version is
later polish.

**The HUD.** A **Remembrall** indicator (`tracker_remembrall`, default on): it glows when the current chapter has
unchecked locations — the game's own "something you have forgotten" object, used as a check radar.

**Access.** The pause page plus one controller shortcut, so it is reachable without a menu dive. Back is taken, so
the shortcut has to be something else (question 2). The Floo fireplace is the diegetic access (§3.1, §4).

**Messages.** Two channels:

1. **Native toast (v1)** — a parchment strip drawn by the port: "Check sent: Card 17", "Received: Glacius from
   Harry". Works in every gamestate, themed, no RE. This is the primary feedback.
2. **Game-text hook (stretch)** — the user's idea of using the game's own text. PoA has subtitles
   (`Subtitles` option), the `GUI_*_HLPTXT_*` help lines and the loading-screen tips; if the display/queue function
   can be found, an override can push AP messages as in-world text. That is a Phase-3/4 RE item, not a v1 blocker.

Rule: the tracker is the log, the toast is the feedback — a check must never be learnable only by opening a menu.

## 12. Reward suppression

Decision 6: when a check's vanilla reward is the same thing as an AP item, the vanilla grant is suppressed and the
AP item is what grants it. Two classes:

| Class | Examples | Suppress? | Why |
|---|---|---|---|
| Reward | cards, beans, house points, endurance, folio pages, L&F items, statue rewards, minigame grades, F&G rewards | **yes** | the AP item is the same reward; leaving both doubles it |
| Progression | spellbooks/lessons, story tasks, chapter ends, area passes | **no** (keep vanilla) | suppressing can soft-lock the mission that expects the spell; the check is the event, the AP item is the token/key |

Implementation:

- The test lives at the grant site (item add / card bit / points add) and needs a `check → suppress` flag from the
  catalog plus an "AP owns this check" test. It is one more reason to find the item-grant function.
- **Cards are the awkward one.** The world card entity is the check; suppressing its ownership grant means the
  album stays empty until the AP item arrives, and the card must still spawn (it *is* the check). HP2 hit the same
  problem ("world cards AP-only") and deferred it; here it is in scope.
- **Set completion** (endurance/points) must be re-evaluated when a card arrives from AP, not only when the vanilla
  pickup fires.
- The **reward-swap table** (check key → suppressed grant → AP item) is part of the catalog.


