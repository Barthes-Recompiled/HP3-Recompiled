# 04 — Roadmap

Each phase ends with something testable on the PC without a full play-through. The order is chosen so the two
unknown seams (item grant, script VM) are attacked only after the cheap wins are in.

## Phase 0 — finish the census (1–2 sessions, static + one trace)

| Task | Output | Done when |
|---|---|---|
| Card per-set sizes / the two extra slots | `card_table.tsv` (ids only) | 52 ids map to 10 sets with counts |
| Pickup placements | `rooms.tsv` (room, model, x, y, z, source) | ≥90% of placed pickups keyed from scripts or `.pkg` |
| Challenge Shields | count + award points | category size known |
| F&G milestones | per-track checkpoint list | dungbomb 5/10/15/20, snot targets, confiscation 4 |
| Enemy/creature list | per-room spawn table | Max-tier `enemies` can be sized |
| `InitialiseChapter` side effects | note per chapter: what it resets | AP state plan per chapter |
| Save layout first look | progress block offset map | save lineage possible |

Tools: the `.hpb` reader (`Harry Potter Xbox Decomps/tools/hpbfn.py`), the census scripts used for this audit
(kept out of git), `port/tools/` patterns from HP2.

## Phase 1 — catalog + apworld skeleton (no port changes)

- `port/ap/make_catalog.py` → `catalog.json` (locations, items, regions, groups, version hash), copied verbatim
  into the apworld's `data/`.
- `games/hppoa/` world: regions from the chapter table, items, rules in `rule_builder` terms, options dataclass,
  `fill_slot_data`, tests (`test_world.py --generate 7`).
- **Exit**: a seed generates offline, all locations reachable in logic, no port code touched.

## Phase 2 — bridge (port changes, no gameplay hooks)

- Port HP2's in-process bridge design: WinHTTP WebSocket on its own thread, frame-hook coordinator, sidecar
  session, offline queue, catalog fence.
- Slot data handshake; `RECOMP_AP_SERVER`/`RECOMP_AP_SLOT` env for tests.
- **Exit**: connect, receive items (logged only), send a synthetic check, survive a save/reload.

## Phase 3 — detection (the cheap, safe hooks)

| Hook | Address | Risk |
|---|---|---|
| APT variable observer | `sub_001AD3B0` | none (log-only) |
| Card state read | `sub_00071840` / 0x3661C0,C4 | none |
| Chapter/task state read | 0x376B44/48/4C, `ChapterId` | none |
| Travel | `sub_00115390` + globals | medium (only from safe states) |

- Whitelist variables → checks; debounce duplicate writes; persist sent-checks in the sidecar.
- Add the pause-menu "Archipelago" page: passes list + travel.
- **Exit**: a live run sends card/task/statue checks and travels between two chapters.

## Phase 4 — delivery (the expensive hooks)

- Find the **item grant / inventory add**; wire AP item → spell/upgrade/consumable.
- Find the **progress/save block**; wire lineage and "apply after chapter ready".
- Story tokens and mission-pass flags: per-mission flag table, seeded on grant.
- Vanilla reward suppression (optional; decide in Phase 3 review).
- **Exit**: a fresh seed can be started, one item of each class delivered, one mission completed and its token
  used to open another mission.

## Phase 5 — options, goal, polish

- `check_slop` presets and all groups; goal `dark_tower` (9/12), `house_cup`, `cards`.
- Start-chapter option; mission-pass shuffle/chain; open-castle off.
- Launcher Archipelago tab (shared Barthes launcher).
- EUR retarget through `tools/xdk_port` (addresses move, never by hand).
- **Exit**: two-slot test with different presets; a minimal seed and a maximum seed both complete.

## Risks (ranked)

1. **Chapter scripts refuse out-of-order state.** Mitigation: seed flags, demote fragile missions to `chain`,
   test matrix in [03-design §9](03-design.md#9-live-test-matrix-what-decides-whether-the-design-survives).
2. **InitialiseChapter resets AP state.** Mitigation: apply after the chapter-ready signal; if it also resets the
   *slot* counters, keep AP counters in the sidecar and re-apply at every chapter load.
3. **Item grant never found cheaply.** Fallback: script-variable writes (the UI names are known) may be enough to
   make spells/cards "owned" for gating, with the vanilla grant left in place (double-reward model).
4. **52-card ambiguity.** If the two extra slots are not cards, the album checks are 50; catalogue version bump.
5. **Pickup census stalls.** Ship Standard without `pickups`; add later as a group (the HP2 precedent).
6. **Save corruption.** Never write the native save from AP; the sidecar is separate, lineage is read-only.

## Branch and review model

- `ap/headwork` (this branch): docs only.
- Later: `ap/catalog`, `ap/bridge`, `ap/hooks`, each a PR against `main` on
  `Barthes-Recompiled/HP3-Recompiled`, reviewed before merge. `main` is never pushed to directly.
- Every hook commit cites the guest address, the disassembly it was based on, and the run that verified it.
