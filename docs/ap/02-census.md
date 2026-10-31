# 02 — Check census

The goal is maximum checks with the ability to trim ("check slop"). Every row below is something the game itself
counts, names or rewards; none of it is invented. **Counts are from the disc** unless the Confidence column says
otherwise. Detection is the method that can fire the check in the port; the primary one is the APT set-variable
hook (`sub_001AD3B0`, [01-audit §4.1](01-audit.md#41-the-apt-variable-setter--sub_001ad3b0-pointer-at-0x38a2f8)).

## 1. Summary

| # | System | Checks | Source | Detection | Option group | Confidence |
|---|---|---|---|---|---|---|
| 1 | Story tasks | **35** (11 chapters, ≤4 each) | `GUI_TASKLIST_*` | task variable / task UI | always on | high |
| 2 | Chapter completions | **12** chapters (11 have `*_endchapter.hpb`) | chapter table + `EndChapter_StartNextChapter` | chapter/room state, `End Day` | always on | high |
| 3 | Collector's cards | **52** slots (50 named) | `WizardCards` 52-char string, card album | card bit arrays / `WizardCards` | `cards` | high (count 52), open (per-set) |
| 4 | Card sets completed | **10** | `GUI_COLLECTORS_FETXT_003…012`, `GUI_COLLECT_*` | set-completion message/state | `card_sets` | high |
| 5 | Folio Bruti pages | **16** | `GUI_BRUTI_TITLE_001…016` | `FolioBrutiPagesCollected`, `HasFolioBrutiPage` | `folio` | high |
| 6 | Lost & Found | **10** | `GUI_QST_LAF_001…010` | `Lost_And_Found`, item vars | `lost_and_found` | high |
| 7 | Statue list | **9** | `GUI_STATUES_FETXT_001…009` | `GEOnQueryStatuesInRemembrall` | `statues` | high |
| 8 | Fred & George challenges | **4 tracks** | F&G dialogue + quest pages | per-track counters | `fred_and_george` | high (4), open (milestones) |
| 9 | Potion ingredients | **8** types, **3** potions | `GUI_QST_POT*`, task list | ingredient/potion-full vars | `potions` | high |
| 10 | Minigames | **8 modes** | `GUI_GENFRTXT_*`, `poa_mini_*.big` | challenge scores, availability flags | `minigames` | high (modes), open (grade tiers) |
| 11 | Marauder's Challenge | **10** (one per chapter) | `*_marauderschallenge.hpb` in every chapter | challenge completion/score | `marauders_challenge` | medium |
| 12 | Challenge Shields | open | `GUI_GEN_HLPTXT_105` | shield state | `challenge_shields` | open |
| 13 | Secrets | **6** named | `GUI_LOCTXT_*`, `SecretDoor_InitForDay/Night` | secret-door state | `secrets` | medium |
| 14 | House points | milestones | `Housepoints`, grades | variable milestones | `house_points` | high |
| 15 | Beans | milestones | `Beans_Amount` | variable milestones | `bean_milestones` | high |
| 16 | World pickups | **open** (hundreds?) | manifests, scripts, `.pkg` | item grant / object census | `pickups` | low until census |
| 17 | Loose pickups | open | grant log (HP2 method) | play-through census | `loose_pickups` | low |
| 18 | Lessons | ~6 | task list, class scripts | chapter task / spell grant | `lessons` | high |
| 19 | Spells | ~7 | `AddSpells`, capability sets | spell state | `spells` | medium |
| 20 | Talk | 4,356 dialogue keys | `SV3_*` | dialogue played | `talks` (Max only) | medium |
| 21 | Pursuits | per chapter | `*_Pursuit` scripts | pursuit complete/fail | `pursuits` (Max only) | medium |
| 22 | Enemies | open | `SetAI*` scripts | kill/spawn events | `enemies` (Max only) | low |

**Rough check budget.** Minimal (story only) ≈ **47**. Standard (items 1–15) ≈ **190–220**. Maximum (all
categories incl. pickups/talk/pursuits) ≈ **800–1,500+**, dominated by world pickups and dialogue — the same shape
as HP1's census.

## 2. Story tasks — 35

The Tasks page is the game's own mission list (`Task_Done_1…4`, `Task1_Display…4`). They are the canonical
"story mission = check + gate" units.

| Chapter | N | Tasks |
|---|---|---|
| HOGEX | 3 | Scabbers · Malfoy/Crabbe/Goyle · Lupin |
| Night0 | 4 | Folio to Ron · Expelliarmus practice · Fred & George · Marauder's Map |
| Day1 | 4 | DADA class · Glacius spellbook · Potions (Wiggenweld) · Broom class |
| Night1 | 2 | Rescue Neville · Return to common room |
| Day2 | 1 | Care of Magical Creatures |
| Night2 | 2 | Follow Ron · Escape Dungeons |
| Day3 | 4 | Antidote ingredients · Mix Antidote · Charms · Carpe Retractum |
| Night3 | 4 | Lupin (History) · Expecto Patronum · Gunhilda statue · Return to common room |
| Day5 | 4 | Girding ingredients · Mix Girding · Quidditch crate · Stadium |
| Night5 | 4 | Hagrid · Protect Sirius · Repel Dementors · Buckbeak to Dark Tower |
| Day6 | 3 | Cards · House points · House Cup |

Detection: the UI writes `Task_Done_n` when a task completes; the underlying state is script-side. First hook is
the variable write; if that is too coarse (UI page open only), the script VM is needed. A task completed out of
order still writes its own flag, which is what the randomizer wants.

## 3. Collector's cards — 52 slots, 10 sets

- Ten sets, named in the album: **Witches, Wizards, Vampires, Goblins, Hags, Giants, Dragons, Beasts, Classical
  Beasts, Quidditch** (`GUI_COLLECTORS_FETXT_003…012`; one `pqca_*.big` each).
- The album receives a **52-character** `'0'/'1'` string as the `WizardCards` variable (seven builder functions,
  `sub_00020010…0034`).
- The narrator text names **50** cards (5 per set): e.g. Witches = Gunhilda of Gorsemoor, Morgan le Fay, Queen
  Maeve, Ignatia Wildsmith, Beatrix Bloxam; Wizards = Dumbledore, Merlin, Glanmore Peakes, Herpo the Foul, Edgar
  Stroulger; Quidditch = Barberus Bragge, Cyprian Youdle, Joscelind Wadcock, Leopoldina Smethwyck, Roderick
  Plumpton.
- 52 − 50 = the two "Rare Collector's Card" shop rewards is the obvious reading, **not verified**.
- Each **set completion** is its own reward event (`GUI_COLLECT_001…034`: a stat/Endurance increase or 10/20/30/40
  house points). Those are natural set-level checks.

Detection: read the two bit arrays behind `0x3661C0`/`0x3661C4` through `sub_00071840(id)`, or diff the
`WizardCards` string. Individual cards are the checks; set completion is a derived milestone.

## 4. Folio Bruti — 16

`GUI_BRUTI_TITLE_001…016`: Dementor, Billywig, Chizpurfle, Doxy, Dragonfly, Fairy, Fire Seed Bush, Flying Seahorse,
Ghoul, Hinkypunk, Hippogriff, Pixie, Red Cap, Salamander, Troll, Venomous Tentacula.

Counters exist as variables (`FolioBrutiPagesCollected`, `FolioBrutiPageCount`, `HasFolioBrutiPage`), and pages are
sold by Fred & George (`GUI_FANDG_SHOPTXT_009`) as well as found in rooms (`loose_page` model in manifests).

## 5. Lost & Found — 10

`GUI_QST_LAF_001…010` and the XBE variable names agree: Prefect Badge, Remembrall, Potion Vial Case, Dragon-hide
Gloves, Owl Post Parcel, Key, Telescope, Daily Prophet, Brass Scales, Potion Jar.
`GEOnQueryLostAndFound` / `Lost_And_Found` are the query and the count.

This is the Notice Board quest (Hermione's "someone's lost something again" line, `GEOnQueryLostAndFound`).

## 6. Statue list — 9, two reward tracks

The nine statues are Bridget Wenlock, Glanmore Peakes, Hengist of Woodcroft, Gondoline Oliphant, Beaumont
Marjoribanks, Gunhilda of Gorsemoor, Gregory the Smarmy, Jocunda Sykes, Hooky the House Elf.

Two quests consume the same list:
- **Hermione's homework** (`poa_quest_herm_homework.big`, `SV3_HER_HRHHERHOM_*`): every 3 statues → one chapter of
  *Dark Forces: A Guide to Self Protection* (Improved Duelling / Improved Defence / Faster Spell Cast).
- **Fred & George's statue challenge** (`SV3_GRG_FANDGHER_*`): every 3 statues → a spell upgrade.

So: 9 statue checks + 3 Dark Forces chapter checks + 3 spell-upgrade checks (the latter two can be modelled as the
same event if the rewards are identical — open).

`GEOnQueryStatuesInRemembrall` and `GUI_QST_STATUELIST_*` are the state.

## 7. Fred & George — 4 challenge tracks + shop

| Track | Target | Reward |
|---|---|---|
| Statue list | 9 statues | a Dark Forces chapter each 3 (spell upgrades) |
| Dungbomb challenge | 20 tipped objects | rare card; a card every 5 |
| Snot Slinging (Mucus Projectum) | Glover Hipworth busts around the school | rare card |
| Confiscated items | 4 items (Grow Your Own Warts Kit, fake wands, Belch Powder, fake rat) | rare card |
| Shop | beans → potions, cards, folio pages, bags/cases, spellbooks (Snufflifors, Mucus Projectum), Dark Forces chapters | purchases |

The milestone counts (20 dungbombs, every 5) are explicit in the dialogue; the exact per-target checkpoints are a
census item once the counters are located.

## 8. Potions and ingredients — 8 types, 3 potions

| Potion | Chapter | Ingredients (from `GUI_QST_POT*`) |
|---|---|---|
| Wiggenweld | Day1 | (class lesson; no collection task) |
| Antidote to Uncommon Poisons | Day3 | Fire Seeds, Chizpurfle Carapaces, Graphorn Horn, Billywig Stings |
| Girding Potion | Day5 | Flying Seahorses, Dragonfly Thoraxes, Doxy Eggs, Fairy Wings |

The ingredient names are the eight `GUI_QST_POT*` keys; the 4+4 split above is inferred from the two collection
tasks and should be confirmed against the chapter scripts. Checks: each ingredient gathered (8) + each potion
mixed (2) + the Wiggenweld lesson (1).

## 9. Minigames and challenges

**Bonus activities** (front-end "Bonus Activities" / `poa_mini_*.big`), each with its own score/rating page
(`poa_quest_chall_scores.big`, `GUI_GENFRTXT_RATING_001`):

| Activity | Text key | Notes |
|---|---|---|
| Broom Flight | `LESSONBROOMFLIGHT` | lesson version |
| Advanced Broom Flight | `BROOMFLIGHT` | challenge version |
| Dragon Statues | `DRAGONS` | broom target set |
| Hippogriff Flight | `HIPPOGRIFFFLIGHT` | |
| Owl Racing | `OWLRACING` | |
| Flying Seahorse Fishing | `CARPEFISHING` | Carpe Retractum |
| Duelling Club | `DUELLINGCLUB` | single + multiplayer |
| Snot Slinging | `SNOTSLINGING` | Fred & George |

**In-chapter challenges** (a room and script in the chapter itself):
- `day1_glaciuschallenge`, `day3_carperetractumchallenge`, `night3_patronuschallenge`.
- `*_marauderschallenge` in **every playable chapter** (10; a test-chapter copy makes 11 files) — a per-chapter
  challenge course, not just a story beat.

**Challenge Shields** (`GUI_GEN_HLPTXT_105`: "Collect Challenge Shields to earn extra house points") — the count and
where they are awarded is open; likely one per activity per grade.

## 10. Secrets

Named places in `GUI_LOCTXT_*`: Secret Passageway (Floor7), Secret Way to the Second Floor (Floor4), Secret
Passageway (Floor4), Secret Room (Floor4), Secret Way to the Fourth Floor (Floor2), Broom Cupboard (Entrance Hall).
Script hooks exist: `SecretDoor_InitForDay` / `SecretDoor_InitForNight`, `VIZCELL_FL4_SecretPassage`. The count is
open (the text names six; scripts may have more).

## 11. Pickups and loose items

The manifests name the pickup classes (`wizardcard`, `bean`, `cauldroncake`, `chocolate frog`, `loose_page`,
`wiggenweld`, `antidote`, `dungbomb`, `stinkpellet`, `chest`, `goblet`, `jar`) but not placements. Three routes, in
order of preference:

1. **Script spawns** — room scripts contain model names and coordinates; a static scan can key
   `room + model + position` the way HP2's `pickups.tsv` did.
2. **`.pkg` object tables** — decode the `PKGD`/`PKGCD` chunks and read placed objects (the HP1/HP2 GEO analogue).
3. **Play-through census** — HP2's `RECOMP_AP_RECORD` trick: log every grant and cluster by room/position. Works,
   but only covers what was played.

Until one of these lands, `pickups`/`loose_pickups` stay off by default and the budget above excludes them.

## 12. Milestones and derived checks

- **House points** (`Housepoints`/`House_Points`): milestone checks (e.g. every 25), plus the chapter-end grade
  (`HousePointGradeDistinction/A+/A/B/C`) and the Day6 House Cup.
- **Beans** (`Beans_Amount`): milestone checks, plus the bag upgrades (Dungbomb Case, Stink Pellet Bag, Large
  versions, `LargeBeanHolderStores`/`SmallBeanHolderStores`).
- **Card sets** (10) and **Dark Forces chapters** (3) are derived from their member checks.

## 13. Detection strategy (what to build first)

1. **`sub_001AD3B0` observer** — logs `(name, value)` for a whitelist of variables; zero gameplay risk. Covers
   cards (string), beans, points, folio count, L&F, tasks, chapter, minigame availability, potion-full.
2. **Card bit reader** — `sub_00071840(id)` for authoritative per-card state (the string is UI-formatted and may
   be page-filtered).
3. **Travel** — `sub_00115390` / the four globals; needed for passes.
4. **Item grant / VM** — the two open seams that turn "observe" into "give" and unlock script-level checks.

## 14. Open items for the census

| Item | Why it matters |
|---|---|
| per-set card counts / the 2 extra slots | exact check list |
| challenge-shield count/placement | a whole category |
| per-target F&G milestones | check granularity |
| pickup placements (script or `.pkg`) | the largest category |
| statue/homework reward overlap | avoid double-counting |
| Day4/Night4 | dead content or alias |
| whether Inventory/Quests pages are reachable | AP UI page host |
| enemy/creature list | Max-tier "enemies" |
| talk granularity (per line vs per conversation) | Max-tier size |
