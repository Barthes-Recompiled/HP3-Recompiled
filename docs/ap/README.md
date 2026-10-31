# Archipelago for HP: Prisoner of Azkaban (Xbox port) — audit and design

Work in progress, 2026-10-31. **Docs only**: nothing here is implemented yet. This folder exists so the check
census and the world design can be reviewed before a line of bridge or apworld code is written.

The game is the USA disc (`default.xbe` SHA-1 `51b09e01…`), played through `hp3_recomp`'s static recompilation
(`hppoa.exe`). EUR is out of scope until the USA world works.

## Read in this order

| Doc | What it answers |
|---|---|
| [01-audit.md](01-audit.md) | What the disc, the scripts, the engine and the recompilation actually give us: formats, counts, code seams, and what a hook can and cannot see. |
| [02-census.md](02-census.md) | Every plausible check, grouped by the game's own systems, with counts, evidence and a detection method. |
| [03-design.md](03-design.md) | The world: decisions, Day 1 start, chapter/area passes, the open castle, Intro flashback, story and gamestate handling, tracker, reward suppression, options and check slop, goal. |
| [04-roadmap.md](04-roadmap.md) | Phases from census to playable seed, the open questions that gate each phase, and the risks. |
| [05-handoff.md](05-handoff.md) | State at the end of the 2026-10-31 night session. |
| [06-handoff-2026-10-31.md](06-handoff-2026-10-31.md) | Post-decision handoff: the eight answers, the area-pass call, and the next workstreams (check/pickup audit, check slop, tracker, suppression, Intro). |
| [07-prompt-next-session.md](07-prompt-next-session.md) | Copy-paste prompt for the next agent session, scoped to the audit and the catalog spec. |
| [08-catalog-spec.md](08-catalog-spec.md) | The catalog contract: schema, IDs, keys, groups and presets, reward classes and swap table, regions, tests, spell-id evidence. |
| [09-handoff-to-claude.md](09-handoff-to-claude.md) | Implementation handoff: what is solid/open, the phase order, rules, first tasks, and a prompt for Claude. |
| [10-implementation-log.md](10-implementation-log.md) | Implementation log: Phase 1 (catalog generator, apworld, tests, deviations, next tasks). |

## Method

- **Disc data first.** The scripts (`.hpb`) and the UI text (`Apt_Text.txt`) name most of the game's systems; the
  chapter/room table and the task lists are in the game's own data, not guessed. Counts below are file-derived
  unless marked *inferred* or *open*.
- **Code second.** Each seam is cited by guest address and, where useful, by the generated function that shows the
  call. Nothing is called "found" until it is a real function start (`check_no_foreign_addrs.py`) and the call site
  is readable.
- **Live testing last.** Nothing in this folder was confirmed on a running game; the 2026-10-30 handoffs cover what
  is known to run.

## What is solid, what is not

Solid: chapter/room names and the load call, the task lists, the collection systems and their sizes, the
set-variable hook, the card bitfield accessor, the debug-menu travel path.

Not solid yet: per-set card counts (52 slots vs 50 narrated names), where the item-grant/inventory function is,
the in-memory progress block and save layout, whether chapter scripts tolerate out-of-order missions, and the
exact challenge-shield count.

## Rules this folder follows

- No game data in git: names, ids, addresses and counts only; no extracted files, no captures.
- USA addresses only. A hook that moves to EUR later goes through `tools/xdk_port`, never by hand.
- Anything randomised is reversible from the slot: the port must still boot and play without a server.
