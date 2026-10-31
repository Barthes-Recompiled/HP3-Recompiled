# 08 — Catalog spec (check slop, IDs, groups, swap table)

The catalog is the single source of truth for IDs, names, groups and reward behaviour. It is generated from the
census data, copied verbatim into the apworld's `data/`, and embedded in the port. A seed whose catalog version
does not match is refused.

This spec is the contract for the implementation phase. Everything marked **open** is a Phase-0 input that does
not change the schema.

## 1. File and version

- Path: `port/ap/catalog.json` (generated), copied to `games/hppoa/world/hppoa/data/catalog.json`.
- Generator: `port/ap/make_catalog.py`.
- Version: `sha256` of the canonical JSON (sorted keys, no whitespace) truncated to 12 hex chars.
- The port embeds the version at build time; the apworld sends it in slot data; a mismatch refuses the slot.
- Every content change bumps the version and invalidates old seeds.

## 2. Schema

```jsonc
{
  "version": "a1b2c3d4e5f6",
  "schema": 1,
  "game": "Harry Potter and the Prisoner of Azkaban Recomp",
  "ids": { "location_base": 74300000, "item_base": 74310000 },
  "regions": {
    "start": "Day1",
    "chapters": ["Intro", "HogwartsExpress", "Night0", "Day1", "Night1", "Day2", "Night2",
                 "Day3", "Night3", "Day5", "Night5", "Day6"],
    "areas":    ["GlaciusChallenge", "MaraudersChallenge", "CarpeRetractumChallenge", "ShriekingShack",
                 "PatronusChallenge", "QuidditchMatch", "DementorBattle", "DarkTower", "HouseCup",
                 "BonusActivities"]
  },
  "items": {
    "Day 1 Pass": {"id": 74310001, "kind": "chapter", "class": "progression"},
    "Glacius":    {"id": 74310201, "kind": "spell",   "class": "progression", "spell_id": 10, "source": "Day1"},
    "Story Token (Day 1)": {"id": 74310301, "kind": "token", "class": "progression"}
  },
  "locations": [
    {
      "name": "Day1: DADA Class",
      "id": 74300001,
      "key": "task:day1:dada",
      "region": "Day1",
      "group": "tasks",
      "reward_class": "progression-story",
      "swap": null,
      "access": {"chapter": "Day1", "spells": [], "areas": []}
    }
  ],
  "groups": {
    "tasks": {"preset": ["minimal", "standard", "maximum"]},
    "cards": {"preset": ["standard", "maximum"]}
  }
}
```

- `items`: `kind` ∈ `chapter | area | spell | token | upgrade | filler`; `class` ∈ `progression | useful | filler`.
- `locations`: one row per check; `reward_class` ∈ `reward | progression-spell | progression-story`.
- `swap`: null for non-suppressed; otherwise `{"grant": "card_bit", "arg": 17, "item": "Card 17"}`.
- `access`: the logic inputs; `chapter`/`areas`/`spells` are item names, `prereq` is a list of location keys
  (for tasks that require another task in the same chapter).
- Region names are the AP region; `region` may be a chapter or an area.

## 3. IDs

| Range | Use |
|---|---|
| `74300000` + n | locations (n = 1..) |
| `74310000` + n | items |
| `74320000` + n | reserved (events/goals, not sent) |

HP1 uses 74100000, HP2 74200000; PoA takes 74300000. Locations and items are separate counters, stable across
catalog versions for the same key (a key never changes ID; a removed key leaves a hole).

## 4. Keys

| Prefix | Shape | Example |
|---|---|---|
| `task:` | `task:<chapter>:<slug>` | `task:day1:dada` |
| `chapter:` | `chapter:<name>` | `chapter:day1` |
| `card:` | `card:<1-52>` | `card:17` |
| `card_set:` | `card_set:<set>` | `card_set:witches` |
| `folio:` | `folio:<1-16>` | `folio:9` |
| `laf:` | `laf:<item>` | `laf:key` |
| `statue:` | `statue:<name>` | `statue:gunhilda` |
| `potion:` | `potion:<name>` / `ingredient:<name>` | `potion:girding`, `ingredient:doxy` |
| `minigame:` | `minigame:<mode>:<grade>` | `minigame:owl:gold` |
| `marauder:` | `marauder:<chapter>` | `marauder:night3` |
| `fg:` | `fg:<track>:<n>` | `fg:dungbomb:15` |
| `shop:` | `shop:<item>` | `shop:snufflifors` |
| `secret:` | `secret:<place>` | `secret:floor4_b` |
| `bean:` / `point:` | milestone value | `bean:250`, `point:100` |
| `pickup:` | `pickup:<room>:<item>:<cluster>` | `pickup:day1-entrancehall:card:3` |
| `talk:` | `talk:<id>` | `talk:SV3_...` |
| `pursuit:` | `pursuit:<chapter>:<n>` | `pursuit:day1:2` |
| `enemy:` | `enemy:<room>:<type>:<n>` | `enemy:day2-exterior:doxy:1` |
| `intro:` | `intro:<event>` | `intro:...` |

Slugs are lowercase, ASCII, `[a-z0-9_]+`. A key is immutable once shipped.

## 5. Groups and presets

| Group | Minimal | Standard | Maximum | Notes |
|---|---|---|---|---|
| `tasks` | on | on | on | 28 reachable; 7 pre-completed |
| `chapters` | on | on | on | 12 chapter-end checks |
| `cards` | off | on | on | 50 set cards (+2 rare open) |
| `card_sets` | off | on | on | 10 |
| `folio` | off | on | on | 16 |
| `lost_and_found` | off | on | on | 10 |
| `statues` | off | on | on | 9 |
| `potions` | off | on | on | 8 ingredients + 3 potions |
| `minigames` | off | on | on | 8 modes × grades |
| `marauders_challenge` | off | on | on | 10 |
| `fred_and_george` | off | on | on | 4 tracks + milestones |
| `shop` | off | on | on | purchases |
| `secrets` | off | on | on | 7 named (`GUI_LOCTXT_*`; 02 missed `FLOOR2_003`) |
| `challenge_shields` | off | on | on | count open |
| `house_points` | off | on | on | milestones |
| `bean_milestones` | off | on | on | milestones |
| `pickups` | off | on* | on | *after the placement census |
| `loose_pickups` | off | off | on | play-through log |
| `enemies` | off | off | on | |
| `talks` | off | off | on | **maximum only** |
| `pursuits` | off | off | on | **maximum only** |
| `intro_memories` | off | off | on | optional flashback checks |

Presets set defaults; every `include_<group>` option overrides. The slot's `groups` map is authoritative for the
port: it only sends enabled checks.

## 6. Reward classes and swap table

| Class | Swap | Examples |
|---|---|---|
| `reward` | `{"grant": "<site>", "arg": <id>, "item": "<name>"}` | cards, beans, points, endurance, folio, L&F, statues, minigames, F&G |
| `progression-spell` | `{"grant": "spell_grant", "arg": <spell_id>, "item": "<Spell>"}` | the 9 spellbook/purchase checks |
| `progression-story` | `null` (vanilla grant stays) | tasks, chapter ends, area passes |

Grant sites: `card_bit` (`0x3661C0/C4`), `points_add`, `bean_add`, `item_add`, `spell_grant`. Each site is a hook
the port implements once; the catalog only names it and the argument.

Rule: a suppressed check fires its AP location on the **pickup event**, then suppresses the vanilla grant; the AP
item applies the grant when it arrives. The card entity still spawns (it is the check); set completion is
re-evaluated on card add.

## 7. Item list

Progression:

- 12 chapter passes (Day1, Intro, HOGEX, Night0 pre-collected; 8 in the pool).
- 10 area passes.
- 10 spells: Flipendo pre-collected; Expelliarmus, Glacius, Lumos Duo, Reparo, Carpe Retractum, Expecto Patronum,
  Draconifors, Snufflifors, Mucus Projectum in the pool.
- 11 story tokens (2 pre-collected).
- 6 Endurance upgrades: six card sets raise Endurance (Witches, Wizards, Hags, Dragons, Classical Beasts,
  Quidditch; `GUI_COLLECT_001…034`), the other four give house points. Shop upgrades are listed per item (§13).
- 3 Dark Forces chapters.

Useful/filler: beans, house points, Wiggenweld/Antidote/Girding, dungbombs, stink pellets, chocolate frogs,
cauldron cakes.

## 8. Regions and access

```
Menu
└─ Hogwarts (start: Day1)
   ├─ Intro, HogwartsExpress, Night0, Day1, Night1, Day2, Night2, Day3, Night3, Day5, Night5, Day6
   │     (edge: chapter pass; start/Intro/HOGEX/Night0 pre-collected)
   └─ areas (edge: area pass + chapter if the area lives there)
         GlaciusChallenge (Day1), MaraudersChallenge (any), CarpeRetractumChallenge (Day3),
         ShriekingShack (Night3), PatronusChallenge (Night3), QuidditchMatch (Day5),
         DementorBattle (Night5), DarkTower (Night5 goal), HouseCup (Day6), BonusActivities
```

- Rooms are free inside a chapter; only area/mission edges take a pass.
- Tasks: `access.chapter` + `access.spells`/`areas` + `prereq` task keys.
- Goal: `DarkTower` requires `tokens >= 8`, `Night 5 Pass`, `Expecto Patronum`.

## 9. Generator inputs

| Input | Source | Status |
|---|---|---|
| task list | `Apt_Text.txt` `GUI_TASKLIST_*` | done |
| card table | `GUI_NAR_CARDS*` (50 names) + `pqca_*.big` (5 slots/set) | done |
| folio, L&F, statues, ingredients | `Apt_Text.txt` | done |
| minigame modes | `GUI_GENFRTXT_*` | done |
| secrets | `GUI_LOCTXT_*` + scripts | partial |
| shields, F&G milestones, pickup placements, enemies | census | **open** |
| spell ids | script/UI evidence | partial (§10) |

## 10. Spell ids (current evidence)

The game's spell table has 11 entries; 10 are world spells. The inventory UI lists them in this order
(`poa_ig_inventory.big`, both `name` and `name_inv`):
`LumosDuo, ExpectoPatronum, CarpeRetractum, MucusProjectum, Draconifors, Snufflifors, Reparo, Rictusempra,
Glacius, Expelliarmus`.

Script evidence: `AddSpells` in Intro/Night0 passes `7`; Night1 passes `7, 8, 9`; the Day1 Glacius spellbook
function carries `10`; the Day3 Carpe one carries `13`. The two orders disagree, so the script numbers are
probably **item ids**, not the UI enum. Current candidate (needs one live confirmation):

| Spell | Candidate id | Evidence |
|---|---|---|
| Flipendo | 7 | `AddSpells` Intro/Night0 (start spell); UI alias `Rictusempra` |
| Expelliarmus | 9 | third of `7,8,9` at Night1; learned in Night0 |
| Lumos Duo | 8 | second of `7,8,9` at Night1 |
| Glacius | 10 | Day1 Glacius spellbook constant |
| Reparo | 11 | between 10 and 13 (inferred) |
| Carpe Retractum | 13 | Day3 Carpe spellbook constant |
| Expecto Patronum | 14 | after Carpe (inferred) |
| Draconifors | 15 | after Patronum (inferred) |
| Snufflifors / Mucus Projectum | 12 / 16 | shop (inferred) |

Verification: grant id *N* through the same path the script uses and check which spell icon/state appears;
or read the spell bitfield after each spellbook. Until then the catalog uses the **name** as the item key and
`spell_id` is advisory.

## 11. Tests

- IDs unique and in range; keys unique and immutable; every location has a group and reward class.
- Every group appears in the options dataclass; presets are monotone (minimal ⊆ standard ⊆ maximum).
- Count assertions per group against the census (cards 50, folio 16, L&F 10, statues 9, …).
- Reachability: a maximal seed reaches every enabled location; a minimal seed reaches tasks + chapters + goal.
- No-self-lock: for every spell, reach its check in logic without owning that spell.
- Slot data round-trip: catalog version, groups, goal, tokens, start, mode.
- The port refuses a catalog version it was not built with (test with a bumped version).

## 12. Open inputs that do not change the schema

Card 5th/rare split (50 vs 52), challenge shields, F&G milestone counts, pickup placements, enemy list, talk
granularity, intro memories, spell-id confirmation.

## 13. As built (Phase 1, 2026-10-31)

`port/ap/make_catalog.py --game-dir <hp3_recomp>/game` → `catalog.json` version **6f022b02f338**: 221 locations,
137 item names. Where the build differs from the sections above, this section wins.

**Generator.** Every name comes from the game's text database (`data/usenglish/variables/Apt_Text.txt`, falling
back to `english`): card sets and cards, task list, Folio Bruti, Lost and Found, statues, ingredients, potions, bonus
activities, secret places, shop items and spells. The generator's tables only say which text keys form each group,
the keys and the logic; the build fails if the game's task list differs from the census or a key is missing. Output
is deterministic: `port/ap/catalog.json` plus the apworld's verbatim copy `apworld/hppoa/data/catalog.json`. IDs
live in `port/ap/ids.lock.json` (committed): a key keeps its ID, new keys take the next number, a removed key leaves
a hole. `--check` fails when a committed file is stale. `port/ap/*.json` is LF in every checkout.

**Schema additions** (schema stays 1; all additive):
- `regions.area_chapters`: area → the chapter it lives in (null = any chapter).
- `goal`: `{region, tokens_required, requires}` for the Dark Tower goal.
- items: `start` (pre-collected), `count` (copies), `pool` (groups whose vanilla grant the item replaces; the item is
  in the pool only when one of them is on), `collect` (`card` / `folio` / `lost_and_found` on collectibles, which
  are kind `upgrade`), `region`/`chapter` on passes. Kinds are exactly §2's six.
- `access` names are **item names** (`"Day 1 Pass"`, `"Glacius Challenge Pass"`), plus `cards` (item names a card
  set needs when cards are shuffled) and `passes` (pool chapter passes a milestone or shop purchase needs).
- `groups.<g>`: `{preset, option, count}`; `option` is null for always-on groups.

**Counts per group:** tasks 28 (the seven HOGEX/Night0 tasks are seeded and not in the catalog) · chapters 12 (the
seeded Intro/HOGEX/Night0 ends are reachable at the start, so the port sends them when the seed begins; Night5's end
sits in the Dark Tower) · **spellbooks 4** (an always-on group §5 lacks: the non-task spellbooks Expelliarmus/Night0,
Lumos Duo/Night1, Reparo/Night2, Draconifors/Night3; the Glacius, Carpe and Patronum spellbooks are tasks with
`reward_class` progression-spell, and Snufflifors/Mucus are shop checks) · cards 50 (ids 51–52 reserved for the two
extra `WizardCards` slots, not emitted) · card_sets 10 · folio 16 · lost_and_found 10 · statues 9 · potions 11 ·
minigames 8 (one per mode, `minigame:<mode>:complete`, until grade tiers are known) · marauders_challenge 10 ·
fred_and_george 12 (statues 3/6/9, dungbombs 5/10/15/20, snot 1, confiscated 1–4) · shop 13 one-time purchases ·
secrets **7** · house_points 10 (50–500) · bean_milestones 11 (50–2000). Empty until their census:
challenge_shields, pickups, loose_pickups, enemies, talks, pursuits, intro_memories (pickups is Maximum-only until
then). Minimal = 44 checks, Standard = Maximum = 221 today.

**Swap args.** `card_bit` args are card ids 1–50 (bit = id − 1); `spell_grant` args are the §10 candidates;
`item_add` args are null until the grant is found (Phase 4). Points card sets, statues, secrets, milestones,
ingredients, minigames, F&G tracks and the rare cards have `swap: null` (vanilla reward stays) until their grants are
known.

**Logic guesses to confirm in play:** the Night3 chapter end needs the Shrieking Shack pass; the Girding ingredients
(flying seahorses) and Seahorse Fishing need Carpe Retractum; Snot Slinging and the F&G snot track need Mucus
Projectum; milestones need one pool chapter pass per 100 house points / 250 beans; shop purchases need 1–5.