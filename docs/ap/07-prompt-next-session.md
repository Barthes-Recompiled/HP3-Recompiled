# 07 — Prompt for the next session

Copy the block below into a fresh agent session. It is self-contained; the agent reads the docs for detail.

```text
You are continuing the Archipelago headwork for Harry Potter and the Prisoner of Azkaban (Xbox, USA disc) in the
static-recompilation port. This is a docs-and-analysis session: do not write port hooks, the bridge, or the apworld
this time.

## Read first, in order
1. docs/ap/06-handoff-2026-10-31.md        (the current handoff: decisions, workstreams, first tasks)
2. docs/ap/03-design.md  §0, §5, §6.7      (decisions, area passes, spells in the pool, the retreat)
3. docs/ap/01-audit.md                     (formats, code seams, what the recompiler permits)
4. docs/ap/02-census.md                    (counts, evidence, the spell sources)
5. docs/ap/04-roadmap.md                   (phases and risks)
Also read hp3_recomp/CLAUDE.md and port/DEVELOPING.md section 8 for the repo rules.

## Where you are
- Worktree: C:\Users\unowi\DecompProject\hp3_ap
- Branch: ap/headwork, draft PR #5 on Barthes-Recompiled/HP3-Recompiled, based on main 753e456.
- Never push to main. Commit in small logical commits and push to ap/headwork; the PR updates itself.
- A repo-local git credential helper is already configured in this worktree.
- Game data: hp3_recomp\game (USA default.xbe, SHA-1 51b09e01...). Generated code:
  hp3_recomp\port\src\recomp\gen (read-only reference; never edit).
- Census scratch scripts (not committed): C:\Users\unowi\DecompProject\out\ap\*.py

## Established facts (do not re-derive; correct them only with better evidence)
- 12 chapters; 11 have task lists (HOGEX + 10 day/night). 35 story tasks total; HOGEX (3) and Night0 (4) are
  pre-completed at the Day 1 start, so 28 are reachable in a seed.
- 52 card slots / 10 sets (50 cards named in the narrator text; 2 slots unexplained).
- 16 Folio Bruti, 10 Lost & Found, 9 statues, 8 ingredients / 3 potions, 8 minigame modes,
  10 Marauder's Challenges. Goal: Dark Tower, 8 of 11 chapter tokens.
- Spells: 10 world spells, 9 obtainable (Flipendo is the start). Sources: Expelliarmus Night0 (inferred),
  Glacius Day1 DADA, Lumos Duo Night1 (inferred), Reparo Night2, Carpe Retractum Day3 Charms,
  Expecto Patronum Night3, Draconifors Night3, Snufflifors/Mucus Projectum Fred & George. Melofors is a
  duelling-club move, not a world item. The light spell is Lumos Duo, not Lumos.
- Verified code seams:
  * sub_001AD3B0(name, value) behind pointer 0x38A2F8 - APT set-variable; 265 call sites; the cheap check detector.
  * sub_00115390(chapter, room, startPoint, flag), driven by 0x376B44/48/4C/50 - the level loader.
  * 0x3661C0/0x3661C4 are pointers to two card bit arrays; sub_00071840(id) tests a card.
  * The album gets a 52-char '0'/'1' WizardCards string from sub_00020010...0034.
- Open: the item-grant/inventory function, the spell grant, the 16 KB save/progress block, the .hpb VM dispatcher,
  pickup placements, per-set card counts, challenge shields, F&G milestone counters, enemy spawn list.

## Scope this session: Workstreams A, G and B from 06
A. Check and pickup audit, to real numbers:
   1. Card table: confirm 52 slots and per-set counts; identify the 2 extra slots.
   2. Pickup placements: start with .hpb constant pools (model names + numeric constants per function, clustered
      per room). Validate on day1-entrancehall, day1-greathall, day1-floor6. Fallback: .pkg object chunks or a
      play-through grant log. Deliverable: rooms.tsv (room, item, x, y, z, source).
   3. Challenge Shields: find the award path/counter and the count.
   4. Fred & George milestones: locate the counters (dungbomb 20/every 5, snot targets, 4 confiscated items).
   5. Enemy/creature list: per-room spawn names.
   6. Intro events: any optional "intro_memories" checks, or "none".
   7. Reward classes: tag every check reward / progression-spell / progression-story.
   8. Save layout first look: find serialise/deserialise, diff two saves, map the progress block.

G. Spells and the retreat:
   1. Map each spell name to its numeric id (AddSpells / _Support_InitCapabilitySets).
   2. Confirm which check holds which spell (the 9 sources above; Expelliarmus and Lumos Duo are inferred).
   3. Spellbook-task dependency: does the task complete on pickup or on ownership? Document it.
   4. Write the retreat spec: pause + shortcut -> load GryffindorCommonRoom or the chapter start; disabled only in
      cutscenes/loading/minigames; mission snapshot/restore.
   5. No-self-lock rule for the apworld (a spell's check may not sit behind a gate needing that spell).

B. Catalog and check-slop spec:
   - docs/ap/08-catalog-spec.md: keys, groups, preset defaults (minimal / standard / maximum), reward classes,
     reward-swap table, slot_data shape, catalog versioning. Spells are always in the pool; the 7 story spellbook
     checks are always on, the 2 shop spells fall back to other locations if the shop group is off.
   - A draft catalog generator (port/ap/make_catalog.py or tools/) and a catalog.json draft (numbers/ids only).
   - Talk/pursuit checks stay Maximum-only. Check slop is the goal.

## Rules
- No game data in git: no extracted files, no gen/, no captures, no artwork; ids, addresses and counts only.
- Evidence rule: every claim cites the file/offset/function or the live run that proved it. Fix the docs if a fact
  is wrong; do not silently diverge.
- Do not start a game run without telling Austin first; one instance at a time; don't drive his pad.
- Build/test on Windows only if you change code; this session should not need a build.
- Keep the doc style and link new docs from docs/ap/README.md and docs/INDEX.md.
- If you need bulk reading of generated code or logs, delegate read-only; you are the writer.

## Done when
- 02-census.md carries the real numbers for cards, pickups, shields, F&G and enemies (or "blocked, why").
- The spell sources and ids are confirmed; the spellbook-task dependency is answered.
- 08-catalog-spec.md exists with the full key/group/preset/swap spec; catalog.json draft + generator exist.
- The retreat spec is written (implementation is a later phase).
- A new handoff says what was found, what is still open, and the next three tasks.
- Branch pushed, PR #5 updated, main untouched.

## Report back
A short summary: the new numbers, what changed, what you could not find and why, the next three tasks. No
play-by-play.
```
