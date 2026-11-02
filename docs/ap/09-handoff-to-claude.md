# 09 — Handoff to Claude: implementation

The audit and the catalog spec are done (docs 01–08). This is the handoff to the implementation writer. DeepSeek did
the static research; Claude is the only writer for port code. Nothing in this document has been built or run.

## 1. State

- Branch `ap/headwork`, worktree `C:\Users\unowi\DecompProject\hp3_ap`, draft PR #5, based on `main` 753e456.
- Docs: 01 audit, 02 census, 03 design, 04 roadmap, 05/06 handoffs, 07 prompt, 08 catalog spec.
- Research artifacts in `C:\Users\unowi\DecompProject\out\ap\` (not committed): `card_table.tsv` (50 cards),
  census scripts, the `.big` extracts. Game files stay in `hp3_recomp\game`.
- No game run has been done for any of this; every code claim is static.

## 2. What is solid

| Finding | Evidence |
|---|---|
| 50 cards, 10 sets × 5, 52 slots; 5th card per set is special; 2 extra slots (likely the rare F&G cards) | `GUI_NAR_CARDS1..10` (5 each = 50); `pqca_*.big` `Card1..Card4` + `fifth_card`; 52-char `WizardCards` |
| 10 world spells and their sources; Melofors is a duelling move; light spell is Lumos Duo | `GUI_SPLTXT_*`; `SV3_*` spellbook lines; 02 §15 |
| Card state: two 7-byte bit arrays behind `0x3661C0/C4`; `sub_00071840(id)`; save/load vtables `sub_00088520/540` | 01 §4.3; `recomp_0002.c`, `recomp_0003.c` |
| Chapter/room table; loader `sub_00115390` + `0x376B44/48/4C/50`; debug-menu travel | 01 §4.2 |
| APT set-variable `sub_001AD3B0` behind `0x38A2F8`; 265 sites | 01 §4.1 |
| Pickup scripts name callbacks and models, not placements | 02 §17.3 |
| Save serialization is a vtable-based module system; card module save/load mapped | 02 §17.4 |

## 3. What is open (and blocks the next phase)

| Item | Why it blocks | Next |
|---|---|---|
| **Item / spell grant** | Delivery, suppression, spells | Search the module vtables / the shop and pickup callers; one function unlocks it |
| **Save/progress block layout** | Lineage, snapshot, chapter-ready state | Find the module registry (vtables near `0x885xx`); map offsets |
| **Pickup placements** | The largest check group | Decode `.cpb`/`.pkg` object data or use a play-through grant log |
| **Spell ids** | The nine spell items | One live confirmation (grant id N, see the spell) |
| Card 5th/rare split, shields, F&G milestones, enemies | Counts, not schema | Census follow-up |

## 4. Implementation order

### Phase 1 — catalog + apworld (no port changes)

1. `port/ap/make_catalog.py`: inputs are `Apt_Text.txt`, the chapter table, the 50-card table, and the census
   tables; output `catalog.json` per [08-catalog-spec.md](08-catalog-spec.md). Include the version hash.
2. `games/hppoa/`: world package with regions, items, rules, options (`check_slop` presets + `include_*`), slot
   data, `test_world.py`.
3. Tests: generation, IDs, groups, reachability, no-self-lock, counts.
- **Exit:** a minimal and a maximum seed generate offline; every enabled location is reachable; the counts match
  the catalog.

### Phase 2 — port foundation (no gameplay hooks)

1. Bridge: port HP2's in-process design (WebSocket thread, frame coordinator, sidecar, offline queue, catalog
   fence). Slot data handshake.
2. Pause → "Archipelago" page: Travel and Status tabs first. The travel action writes `0x376B44/48/4C/50` or calls
   `sub_00115390(chapter, room, start, flag)`.
3. **Return to the common room (Floo)**: same loader, `GryffindorCommonRoom` or the chapter start; pause and one
   shortcut; disabled in cutscene/loading/minigame.
- **Exit:** connect, receive an item (logged), travel between two chapters, retreat from a mission, no black
  screen. Live test with Austin.

### Phase 3 — detection (cheap, safe)

1. Override `sub_001AD3B0`: log `(name, value)` for the whitelist; map to catalog checks; debounce; persist sent
   checks.
2. Card reader via `sub_00071840` / `0x3661C0/C4` for authoritative cards.
3. Task/chapter state from `Task_Done_n` / `ChapterId`.
4. Tracker: Checks/Items tabs + parchment toasts; Remembrall HUD optional.
- **Exit:** a live run sends card/task/statue checks, the tracker matches the sidecar, a toast fires.

### Phase 4 — delivery and spells

1. Find and hook the **item grant** and the **spell grant**; wire AP items → spells/upgrades/consumables.
2. **Suppression**: `ap_suppress(check)` at the grant site using the catalog's `reward_class`/`swap`; cards keep
   spawning; set completion re-evaluates on card add; spellbook grants suppressed.
3. **Mission snapshot**: copy the progress block + AP state on mission entry; restore on retreat.
4. **Intro flashback**: snapshot, load Intro, no `EndChapter_StartNextChapter`, restore.
- **Exit:** one item of each class delivered, one suppressed card/statue/spellbook exactly once, a spell castable
  from AP, the retreat recovers a stuck mission, the Intro leaves Day 1 untouched.

### Phase 5 — polish

Options and goal, Floo theming, game-text notifications if found, launcher tab, EUR retarget through
`tools/xdk_port`.

## 5. Rules for the writer

- Build with MSYS2 UCRT64; the full command is in `hp3_recomp/CLAUDE.md`. Never edit `port/src/recomp/gen/`.
- Every address must be a function start (`check_no_foreign_addrs.py`); new override files go in `regen.sh`
  `--exclude-manual`.
- No game data in git: no extracted files, `gen/`, captures; ids, addresses, counts only.
- Evidence per commit: guest address, the generated code it was based on, and the run that verified it.
- Ask Austin before a game run; one instance; don't drive his pad.
- Small commits on `ap/*` branches, PRs against `main`, never push `main`.
- If a static finding here is wrong, fix the doc in the same PR; don't silently diverge.

## 6. First three tasks

1. **Catalog generator + apworld skeleton** (Phase 1). Definition of done: `catalog.json` with a version, a world
   that generates minimal and maximum seeds, tests passing.
2. **Retreat + travel page** (Phase 2). Definition of done: pause page lists held passes; travel loads; the retreat
   loads the common room/start from a mission; live test with Austin.
3. **Set-variable observer** (Phase 3). Definition of done: a live run logs the whitelisted variables and sends
   one card, one task and one statue check exactly once.

## 7. Copy-paste prompt for Claude

```text
You are the implementation writer for the HP: Prisoner of Azkaban Archipelago port. DeepSeek has finished the
audit and the catalog spec; your job is to build, in the repo, starting with Phase 1.

Read, in order:
1. docs/ap/09-handoff-to-claude.md   (this file)
2. docs/ap/08-catalog-spec.md        (the catalog contract)
3. docs/ap/03-design.md §0, §5.1, §6.7, §12
4. docs/ap/01-audit.md               (seams and rules)
5. docs/ap/02-census.md              (counts, evidence)
6. hp3_recomp/CLAUDE.md and port/DEVELOPING.md section 8

Start with Phase 1: port/ap/make_catalog.py, catalog.json, and games/hppoa. No port hooks yet. Use the counts and
keys in 08; keep the version hash canonical; add the tests listed in 08 §11.

Rules: never edit port/src/recomp/gen/; every hook address is a function start; no game data in git; small commits
on ap/impl off ap/headwork; PR to main, never push main; ask Austin before any game run.

Report: what generates, the catalog version, the test results, and the next three tasks.
```
