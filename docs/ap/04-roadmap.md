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
| Reward class per check | catalog field (`reward`/`progression`) | every check has a class and a swap target |
| Intro event scan | optional `intro:*` list | flashback content known (or "none") |
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
| Return to common room | same loader, hub/start room | medium (pause only) |

- Whitelist variables → checks; debounce duplicate writes; persist sent-checks in the sidecar.
- Add the pause "Archipelago" page (Marauder's Map): Travel / Checks / Items / Status; native parchment toasts for
  sent checks and received items; optional Remembrall HUD.
- Add the **Return to the common room** action (Floo) as the softlock escape hatch; wire it before any spell is
  shuffled in anger.
- **Exit**: a live run sends card/task/statue checks, travels between two chapters, the tracker shows the right
  counts, a toast fires for one check and one item, and the retreat recovers from a mission.

## Phase 4 — delivery (the expensive hooks)

- Find the **item grant / inventory add**; wire AP item → spell/upgrade/consumable.
- Find the **progress/save block**; wire lineage and "apply after chapter ready".
- Story tokens and area-pass flags: per-mission flag table, seeded on grant.
- **Spells** (decision 9): the nine spellbook/purchase checks, the spell grant, and the task-vs-ownership test;
  `shuffle_spells` on by default.
- **Reward suppression** (decision 6): grant-site test, the reward-swap table, and re-evaluating card-set
  completion when a card arrives from AP; spellbook grants suppressed with the AP item as the spell.
- **Mission snapshot/restore** for the retreat (progress block + AP state on mission entry).
- **Intro flashback**: snapshot/restore around the Intro load, no `EndChapter_StartNextChapter`.
- **Exit**: a fresh seed starts, one item of each class is delivered (spell included), one mission completes and its
  token opens another, one suppressed card/statue/spellbook arrives exactly once, the retreat recovers a stuck
  mission, and the Intro leaves the Day 1 state untouched.

## Phase 5 — options, goal, polish

- `check_slop` presets and all groups; goal `dark_tower` (8/11), `house_cup`, `cards`.
- Start-chapter option; area-pass mode `areas`/`tasks`/`chain`; open-castle off.
- Travel theming: `travel_requires_floo` fireplaces, the Map/Floo transition, the page's parchment pass.
- Notifications: native toasts ship; the game-text channel if the display function is found.
- Launcher Archipelago tab (shared Barthes launcher).
- EUR retarget through `tools/xdk_port` (addresses move, never by hand).
- **Exit**: two-slot test with different presets; a minimal seed and a maximum seed both complete.

## Risks (ranked)

1. **Chapter scripts refuse out-of-order state.** Mitigation: seed flags, demote fragile missions to `chain`,
   test matrix in [03-design §9](03-design.md#9-live-test-matrix-what-decides-whether-the-design-survives).
2. **InitialiseChapter resets AP state.** Mitigation: apply after the chapter-ready signal; if it also resets the
   *slot* counters, keep AP counters in the sidecar and re-apply at every chapter load.
3. **Item grant never found cheaply.** Fallback: script-variable writes (the UI names are known) may be enough to
   make spells/cards "owned" for gating; that forces the double-reward model and `suppress_rewards=off` for the
   affected groups, which is a visible regression, so the grant search is a real gate.
4. **52-card ambiguity.** If the two extra slots are not cards, the album checks are 50; catalogue version bump.
5. **Pickup census stalls.** Ship Standard without `pickups`; add later as a group (the HP2 precedent).
6. **Save corruption.** Never write the native save from AP; the sidecar is separate, lineage is read-only.
7. **Suppression edge cases** — a card that never reaches the album, a set that never completes, a grant that fires
   twice. Mitigation: exactly-once test in the live matrix, set re-evaluation on card add, the reward-swap table in
   the catalog, and a group-level `suppress_rewards` switch to fall back.
8. **Spell self-lock.** A spell's check behind a gate that needs that spell (or a cycle). Mitigation: the apworld
   no-self-lock test, plus the retreat as the in-game escape.
9. **Spellbook task depends on ownership.** If the story task only completes when the spell is owned, the mission
   chain waits for the AP item; the retreat + snapshot make that livable, but the logic must gate the next mission
   on the task *and* the spell. Test G3 decides.
10. **Retreat disabled where it is needed.** Some mission states may block the pause menu; the live matrix finds
    them, and the mission snapshot is the fallback (restore rather than reload).

## Branch and review model

- `ap/headwork` (this branch): docs only.
- Later: `ap/catalog`, `ap/bridge`, `ap/hooks`, each a PR against `main` on
  `Barthes-Recompiled/HP3-Recompiled`, reviewed before merge. `main` is never pushed to directly.
- Every hook commit cites the guest address, the disassembly it was based on, and the run that verified it.
