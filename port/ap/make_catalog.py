"""Build the Archipelago catalog (docs/ap/08-catalog-spec.md), the ID source shared by hppoa.exe's bridge and the
apworld. It writes port/ap/catalog.json and the apworld's verbatim copy apworld/hppoa/data/catalog.json. The catalog
keeps IDs, keys, logic inputs and the names the game prints; nothing else from the game.

    python make_catalog.py [--game-dir <hp3_recomp>/game] [--check]

Names are read from the game's own text database (data/usenglish/variables/Apt_Text.txt under --game-dir): the
collector's cards and their sets, the task list, Folio Bruti, Lost and Found, statues, ingredients, potions,
bonus activities, secret places, shop items and spells. The tables below only say which text keys make up each
group, their keys and their logic (the census, docs/ap/02-census.md); a missing or extra text key fails the build.

IDs: a key keeps its ID forever. ids.lock.json (committed) records every key ever issued; new keys get the next free
number and a removed key leaves a hole. --check fails if a committed file would change.
Version: sha256 of the canonical catalog (sorted keys, no whitespace, without "version") truncated to 12 hex chars.
"""
import argparse
import hashlib
import json
import os
import re

GAME = "Harry Potter and the Prisoner of Azkaban Recomp"
SCHEMA = 1
LOCATION_BASE = 74300000
ITEM_BASE = 74310000
HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
OUTPUTS = [os.path.join(HERE, "catalog.json"), os.path.join(REPO, "apworld", "hppoa", "data", "catalog.json")]
LOCK = os.path.join(HERE, "ids.lock.json")
DEFAULT_GAME_DIR = r"C:\Users\unowi\DecompProject\hp3_recomp\game"
TEXT_FILES = ["data/usenglish/variables/Apt_Text.txt", "data/english/variables/Apt_Text.txt"]

# region, display name; the game's chapter table order (01-audit §4.2)
CHAPTERS = [("Intro", "Intro"), ("HogwartsExpress", "Hogwarts Express"), ("Night0", "Night 0"), ("Day1", "Day 1"),
            ("Night1", "Night 1"), ("Day2", "Day 2"), ("Night2", "Night 2"), ("Day3", "Day 3"),
            ("Night3", "Night 3"), ("Day5", "Day 5"), ("Night5", "Night 5"), ("Day6", "Day 6")]
CHAPTER_NAME = dict(CHAPTERS)
TASKLIST = {"HogwartsExpress": "HOGEX", "Night0": "NIGHT0", "Day1": "DAY1", "Night1": "NIGHT1", "Day2": "DAY2",
            "Night2": "NIGHT2", "Day3": "DAY3", "Night3": "NIGHT3", "Day5": "DAY5", "Night5": "NIGHT5", "Day6": "DAY6"}
START = "Day1"
# seeded complete at the Day 1 start (design §3); their passes are pre-collected and their tasks are not checks
SEEDED = ["Intro", "HogwartsExpress", "Night0"]
# area passes (design §5): region, display name, the chapter it lives in (None = reachable from any chapter)
AREAS = [("GlaciusChallenge", "Glacius Challenge", "Day1"),
         ("MaraudersChallenge", "Marauder's Challenge", None),
         ("CarpeRetractumChallenge", "Carpe Retractum Challenge", "Day3"),
         ("ShriekingShack", "Shrieking Shack", "Night3"),
         ("PatronusChallenge", "Patronus Challenge", "Night3"),
         ("QuidditchMatch", "Quidditch Match", "Day5"),
         ("DementorBattle", "Dementor Battle", "Night5"),
         ("DarkTower", "Dark Tower", "Night5"),
         ("HouseCup", "House Cup", "Day6"),
         ("BonusActivities", "Bonus Activities", None)]
AREA_NAME = {region: name for region, name, _ in AREAS}
# GUI_SPLTXT_<KEY>_001, candidate spell id (08 §10, advisory until one live grant), source
SPELLS = [("FLIPENDO", 7, "start"), ("EXPELLIARMUS", 9, "Night0"), ("LUMOSDUO", 8, "Night1"),
          ("GLACIUS", 10, "Day1"), ("REPARO", 11, "Night2"), ("CARPERETRACTUM", 13, "Day3"),
          ("EXPECTOPATRONUM", 14, "Night3"), ("DRACONIFORS", 15, "Night3"), ("SNUFFLIFORS", 12, "shop"),
          ("MUCUSPROJECTUM", 16, "shop")]
# the 11 task-list chapters yield a story token (design §5); HOGEX and Night0 are pre-collected
TOKEN_CHAPTERS = list(TASKLIST)

# Story tasks: GUI_TASKLIST_<CH>_<n> -> slug, areas, spells (spell keys), prereq slugs, spell taught.
# The build fails unless these are exactly the task-list keys of the non-seeded chapters.
TASKS = {
    "DAY1_001": ("dada", [], [], [], None),
    "DAY1_002": ("glacius", ["GlaciusChallenge"], [], ["dada"], "GLACIUS"),
    "DAY1_003": ("potions", [], [], [], None),
    "DAY1_004": ("broom", [], [], [], None),
    "NIGHT1_001": ("neville", [], [], [], None),
    "NIGHT1_002": ("common_room", [], [], ["neville"], None),
    "DAY2_001": ("comc", [], [], [], None),
    "NIGHT2_001": ("follow_ron", [], [], [], None),
    "NIGHT2_002": ("escape", [], [], ["follow_ron"], None),
    "DAY3_001": ("antidote_ingredients", [], [], [], None),
    "DAY3_002": ("mix_antidote", [], [], ["antidote_ingredients"], None),
    "DAY3_003": ("charms", [], [], [], None),
    "DAY3_004": ("carpe", ["CarpeRetractumChallenge"], [], ["charms"], "CARPERETRACTUM"),
    "NIGHT3_001": ("lupin", [], [], [], None),
    "NIGHT3_002": ("patronus", ["PatronusChallenge"], [], ["lupin"], "EXPECTOPATRONUM"),
    "NIGHT3_003": ("gunhilda", [], [], [], None),
    "NIGHT3_004": ("common_room", [], [], ["gunhilda"], None),
    # the flying seahorses are caught with Carpe Retractum (Flying Seahorse Fishing)
    "DAY5_001": ("girding_ingredients", [], ["CARPERETRACTUM"], [], None),
    "DAY5_002": ("mix_girding", [], [], ["girding_ingredients"], None),
    "DAY5_003": ("crate", [], [], [], None),
    "DAY5_004": ("stadium", ["QuidditchMatch"], [], ["crate"], None),
    "NIGHT5_001": ("hagrid", [], [], [], None),
    "NIGHT5_002": ("protect_sirius", ["DementorBattle"], ["EXPECTOPATRONUM"], ["hagrid"], None),
    "NIGHT5_003": ("repel", ["DementorBattle"], ["EXPECTOPATRONUM"], ["protect_sirius"], None),
    "NIGHT5_004": ("buckbeak", ["DarkTower"], [], ["repel"], None),
    "DAY6_001": ("cards", [], [], [], None),
    "DAY6_002": ("house_points", [], [], [], None),
    "DAY6_003": ("house_cup", ["HouseCup"], [], [], None),
}
# chapter-end checks: all 12 chapters. The seeded three are sent when the seed starts; Night5's end is the
# Dark Tower, after which the goal fires.
CHAPTER_END_AREAS = {"Night3": ["ShriekingShack"], "Night5": ["DarkTower"]}  # Night3: a logic guess (design §5)
# spellbooks that are not tasks (02 §15): slug, chapter, name, spell key
SPELLBOOKS = [("expelliarmus", "Night0", "Peeves' Spellbook", "EXPELLIARMUS"),
              ("lumos_duo", "Night1", "Muggle Studies Spellbook", "LUMOSDUO"),
              ("reparo", "Night2", "Lower Dungeon Spellbook", "REPARO"),
              ("draconifors", "Night3", "Hermione's Second Spellbook", "DRACONIFORS")]
# card set n: name GUI_COLLECTORS_FETXT_<n+2>, cards GUI_NAR_CARDS<n>_001/003/005/007/009 (card N is bit N-1).
# The 52-slot WizardCards string has two more slots (likely the rare cards): ids 51-52 are reserved, not emitted.
CARD_SETS = 10
CARDS_PER_SET = 5
# GUI_COLLECT_001..034: these six sets raise a character's Endurance; the other four give house points
ENDURANCE_SETS = {"witches", "wizards", "hags", "dragons", "classical_beasts", "quidditch"}
# GUI_QST_POT<KEY>_001 -> slug, chapter, spell keys
INGREDIENTS = [("FIRESEED", "fire_seeds", "Day3", []), ("CHIZPURFLE", "chizpurfle", "Day3", []),
               ("GRAPHORNHORN", "graphorn_horn", "Day3", []), ("BILLYWIGSTINGS", "billywig_stings", "Day3", []),
               ("SEAHORSES", "flying_seahorses", "Day5", ["CARPERETRACTUM"]),
               ("DRAGONFLIES", "dragonfly_thoraxes", "Day5", []), ("DOXYEGGS", "doxy_eggs", "Day5", []),
               ("FAIRIES", "fairy_wings", "Day5", [])]
# GUI_SPLTXT_<KEY>POT_001 -> slug, chapter, prereq task slugs
POTIONS = [("WIGGENWELD", "wiggenweld", "Day1", []), ("ANTIDOTE", "antidote", "Day3", ["antidote_ingredients"]),
           ("GIRDING", "girding", "Day5", ["girding_ingredients"])]
# bonus activities GUI_GENFRTXT_<KEY>_001 -> slug, spell keys; grade tiers are open, so one check per mode
MINIGAMES = [("LESSONBROOMFLIGHT", "broom_flight", []), ("BROOMFLIGHT", "advanced_broom_flight", []),
             ("DRAGONS", "dragon_statues", []), ("HIPPOGRIFFFLIGHT", "hippogriff_flight", []),
             ("OWLRACING", "owl_racing", []), ("CARPEFISHING", "seahorse_fishing", ["CARPERETRACTUM"]),
             ("DUELLINGCLUB", "duelling_club", []), ("SNOTSLINGING", "snot_slinging", ["MUCUSPROJECTUM"])]
# *_marauderschallenge.hpb exists in these ten playable chapters
MARAUDERS = ["Night0", "Day1", "Night1", "Day2", "Night2", "Day3", "Night3", "Day5", "Night5", "Day6"]
# Fred & George's challenge tracks (02 §7): track, display, milestones, spell keys
FRED_AND_GEORGE = [("statues", "Statue Challenge: {n} Statues", [3, 6, 9], []),
                   ("dungbomb", "Dungbomb Challenge: {n} Targets", [5, 10, 15, 20], []),
                   ("snot", "Snot Slinging Busts", [1], ["MUCUSPROJECTUM"]),
                   ("confiscated", "Confiscated Items: {n} of 4", [1, 2, 3, 4], [])]
# Fred & George's shop, one-time purchases: slug, GUI_FANDG_SHOPTXT_<n> (None = not in the shop text), AP item
# ("spell:<KEY>", "text" = the shop name, None = no item yet), pool chapter passes needed.
# Mucus Projectum is not in the shop text; SV3_RON_FANDGRON_015 says they sell it.
SHOP = [("rare_card_1", 4, None, 2), ("rare_card_2", 4, None, 4), ("owl_care_kit", 5, "text", 1),
        ("broom_handle_polish", 6, "text", 1), ("brass_compass", 7, "text", 2), ("tail_twig_clippers", 8, "text", 2),
        ("snufflifors", 10, "spell:SNUFFLIFORS", 1), ("mucus_projectum", None, "spell:MUCUSPROJECTUM", 2),
        ("dark_forces_1", 11, "Dark Forces Chapter", 1), ("dark_forces_2", 12, "Dark Forces Chapter", 3),
        ("dark_forces_3", 13, "Dark Forces Chapter", 5), ("stink_pellet_bag", 16, "text", 1),
        ("dungbomb_case", 17, "text", 1)]
# secret places: every GUI_LOCTXT_<PLACE>_<n> whose name says Secret, plus the Entrance Hall broom cupboard
PLACES = {"FLOOR2": "Second Floor", "FLOOR4": "Fourth Floor", "FLOOR7": "Seventh Floor", "ENTHALL": "Entrance Hall"}
EXTRA_SECRETS = ["GUI_LOCTXT_ENTHALL_003"]
# milestones (Housepoints / Beans_Amount); bigger values need more chapters open: one pool chapter pass per step
HOUSE_POINTS = list(range(50, 501, 50))
HOUSE_POINTS_PER_PASS = 100
BEANS = [50, 100, 200, 300, 400, 500, 750, 1000, 1250, 1500, 2000]
BEANS_PER_PASS = 250

PRESETS = ["minimal", "standard", "maximum"]
# group -> (presets it is on in, include_* option or None when always on); 08 §5 plus the always-on spellbooks.
# Empty groups wait for their census; pickups is Maximum-only until the placement census lands.
GROUPS = {
    "tasks": (PRESETS, None), "chapters": (PRESETS, None), "spellbooks": (PRESETS, None),
    "cards": (PRESETS[1:], "include_cards"), "card_sets": (PRESETS[1:], "include_card_sets"),
    "folio": (PRESETS[1:], "include_folio"), "lost_and_found": (PRESETS[1:], "include_lost_and_found"),
    "statues": (PRESETS[1:], "include_statues"), "potions": (PRESETS[1:], "include_potions"),
    "minigames": (PRESETS[1:], "include_minigames"),
    "marauders_challenge": (PRESETS[1:], "include_marauders_challenge"),
    "fred_and_george": (PRESETS[1:], "include_fred_and_george"), "shop": (PRESETS[1:], "include_shop"),
    "secrets": (PRESETS[1:], "include_secrets"), "challenge_shields": (PRESETS[1:], "include_challenge_shields"),
    "house_points": (PRESETS[1:], "include_house_points"),
    "bean_milestones": (PRESETS[1:], "include_bean_milestones"),
    "pickups": (PRESETS[2:], "include_pickups"), "loose_pickups": (PRESETS[2:], "include_loose_pickups"),
    "enemies": (PRESETS[2:], "include_enemies"), "talks": (PRESETS[2:], "include_talks"),
    "pursuits": (PRESETS[2:], "include_pursuits"), "intro_memories": (PRESETS[2:], "include_intro_memories"),
}
FILLERS = ["25 Beans", "50 Beans", "10 House Points", "Wiggenweld Potion", "Antidote to Uncommon Poisons",
           "Girding Potion", "Dungbombs", "Stink Pellets", "Chocolate Frog", "Cauldron Cake"]


class Text:
    """The game's APT text database: '&KEY=value' records, Latin-1, %XX escapes."""

    def __init__(self, game_dir):
        for rel in TEXT_FILES:
            path = os.path.join(game_dir, rel)
            if os.path.exists(path):
                break
        else:
            raise SystemExit(f"no Apt_Text.txt under {game_dir} (tried {', '.join(TEXT_FILES)})")
        with open(path, "rb") as f:
            raw = f.read().decode("latin-1")
        self.values = {}
        for record in raw.split("&"):
            if "=" in record:
                key, value = record.split("=", 1)
                value = re.sub(r"%([0-9A-Fa-f]{2})", lambda m: chr(int(m.group(1), 16)), value)
                self.values[key.strip()] = " ".join(value.split())

    def __call__(self, key):
        if key not in self.values:
            raise SystemExit(f"text key {key} is missing from Apt_Text.txt")
        return self.values[key]

    def keys(self, pattern):
        return sorted(k for k in self.values if re.fullmatch(pattern, k))


def slug(text):
    return re.sub(r"[^a-z0-9]+", "_", text.lower()).strip("_")


def numbered(text, prefix):
    """Names of PREFIX_001, PREFIX_002, ... up to the first missing one."""
    out, n = [], 1
    while f"{prefix}_{n:03d}" in text.values:
        out.append(text(f"{prefix}_{n:03d}"))
        n += 1
    return out


def read_census(text):
    spell = {key: text(f"GUI_SPLTXT_{key}_001") for key, _, _ in SPELLS}
    sets = []
    for n in range(1, CARD_SETS + 1):
        cards = [text(f"GUI_NAR_CARDS{n}_{2 * i + 1:03d}") for i in range(CARDS_PER_SET)]
        sets.append((text(f"GUI_COLLECTORS_FETXT_{n + 2:03d}"), cards))
    tasks = [k[len("GUI_TASKLIST_"):] for k in text.keys(r"GUI_TASKLIST_[A-Z0-9]+_\d{3}")]
    seeded = {TASKLIST[c] for c in SEEDED if c in TASKLIST}
    tasks = [k for k in tasks if k.split("_")[0] in set(TASKLIST.values()) - seeded]
    if sorted(tasks) != sorted(TASKS):
        raise SystemExit(f"task list differs from the census: game {sorted(tasks)}, census {sorted(TASKS)}")
    secret_keys = [k for k in text.keys(r"GUI_LOCTXT_[A-Z0-9]+_\d{3}") if "Secret" in text(k)] + EXTRA_SECRETS
    return {
        "spell": spell,
        "sets": sets,
        "tasks": {k: text(f"GUI_TASKLIST_{k}").rstrip(".") for k in tasks},
        "folio": numbered(text, "GUI_BRUTI_TITLE"),
        "laf": numbered(text, "GUI_QST_LAF"),
        "statues": numbered(text, "GUI_STATUES_FETXT"),
        "ingredients": {key: text(f"GUI_QST_POT{key}_001") for key, *_ in INGREDIENTS},
        "potions": {key: text(f"GUI_SPLTXT_{key}POT_001") for key, *_ in POTIONS},
        "minigames": {key: text(f"GUI_GENFRTXT_{key}_001") for key, *_ in MINIGAMES},
        "secrets": [(k, f"{text(k)} ({PLACES[k.split('_')[2]]})") for k in sorted(set(secret_keys))],
        "shop": {n: text(f"GUI_FANDG_SHOPTXT_{n:03d}").replace("' -", " -")
                 for _, n, _, _ in SHOP if n is not None},
    }


def card_item(name):
    return f"{name} Card"


def build_items(census):
    items = {}
    spell = census["spell"]

    def add(name, kind, cls, **extra):
        assert name not in items, name
        items[name] = {"kind": kind, "class": cls, **extra}

    for region, name in CHAPTERS:
        add(f"{name} Pass", "chapter", "progression", region=region,
            **({"start": True} if region in SEEDED + [START] else {}))
    for region, name, chapter in AREAS:
        add(f"{name} Pass", "area", "progression", region=region, chapter=chapter)
    for key, sid, source in SPELLS:
        add(spell[key], "spell", "progression", spell_id=sid, source=source,
            **({"start": True} if source == "start" else {}))
    for chapter in TOKEN_CHAPTERS:
        add(f"Story Token ({CHAPTER_NAME[chapter]})", "token", "progression",
            **({"start": True} if chapter in SEEDED else {}))
    # collectibles and upgrades enter the pool only with a group whose vanilla grant they replace ("pool");
    # "collect" says which collection a collectible belongs to
    for _, cards in census["sets"]:
        for name in cards:
            add(card_item(name), "upgrade", "useful", collect="card", pool=["cards"])
    for name in census["folio"]:
        add(f"Folio Bruti Page: {name}", "upgrade", "useful", collect="folio", pool=["folio"])
    for name in census["laf"]:
        add(name, "upgrade", "useful", collect="lost_and_found", pool=["lost_and_found"])
    add("Endurance Upgrade", "upgrade", "useful", count=len(ENDURANCE_SETS), pool=["card_sets"])
    add("Dark Forces Chapter", "upgrade", "useful", count=3, pool=["shop"])
    for _, n, item, _ in SHOP:
        if item == "text":
            add(census["shop"][n], "upgrade", "useful", pool=["shop"])
    for name in FILLERS:
        add(name, "filler", "filler")
    return items


def build_locations(census):
    locs = []
    spell = census["spell"]

    def access(chapter=None, areas=(), spells=(), prereq=(), cards=(), passes=0):
        out = {"chapter": f"{CHAPTER_NAME[chapter]} Pass" if chapter else None,
               "areas": [f"{AREA_NAME[a]} Pass" for a in areas], "spells": [spell[s] for s in spells],
               "prereq": list(prereq)}
        if cards:
            out["cards"] = list(cards)
        if passes:
            out["passes"] = passes
        return out

    def add(key, name, region, group, reward_class="reward", swap=None, acc=None):
        locs.append({"key": key, "name": name, "region": region, "group": group, "reward_class": reward_class,
                     "swap": swap, "access": acc or access()})

    def spell_swap(key):
        return {"grant": "spell_grant", "arg": dict((k, i) for k, i, _ in SPELLS)[key], "item": spell[key]}

    chapter_of = {code: chapter for chapter, code in TASKLIST.items()}
    task_key = {}
    for k in TASKS:
        chapter = chapter_of[k.split("_")[0]]
        task_key[k] = f"task:{chapter.lower()}:{TASKS[k][0]}"
    for k, (s, areas, spells, prereq, teaches) in TASKS.items():
        chapter = chapter_of[k.split("_")[0]]
        add(task_key[k], f"{chapter}: {census['tasks'][k]}", areas[0] if areas else chapter, "tasks",
            "progression-spell" if teaches else "progression-story",
            spell_swap(teaches) if teaches else None,
            access(chapter, areas, spells, [f"task:{chapter.lower()}:{p}" for p in prereq]))
    for chapter, _ in CHAPTERS:
        tasks = [task_key[k] for k in TASKS if chapter_of[k.split("_")[0]] == chapter]
        areas = CHAPTER_END_AREAS.get(chapter, [])
        add(f"chapter:{chapter.lower()}", f"{chapter}: Chapter Complete", areas[0] if areas else chapter,
            "chapters", "progression-story", None, access(chapter, areas, (), tasks))
    for s, chapter, name, key in SPELLBOOKS:
        add(f"spellbook:{s}", f"{chapter}: {name}", chapter, "spellbooks", "progression-spell", spell_swap(key),
            access(chapter))
    n = 0
    for _, cards in census["sets"]:
        for name in cards:
            n += 1
            add(f"card:{n}", f"Card {n}: {name}", "Hogwarts", "cards",
                swap={"grant": "card_bit", "arg": n, "item": card_item(name)})
    for set_name, cards in census["sets"]:
        set_slug = slug(set_name)
        swap = {"grant": "item_add", "arg": None, "item": "Endurance Upgrade"} if set_slug in ENDURANCE_SETS else None
        add(f"card_set:{set_slug}", f"Card Set: {set_name}", "Hogwarts", "card_sets", swap=swap,
            acc=access(cards=[card_item(name) for name in cards]))
    for i, name in enumerate(census["folio"], 1):
        add(f"folio:{i}", f"Folio Bruti {i}: {name}", "Hogwarts", "folio",
            swap={"grant": "item_add", "arg": None, "item": f"Folio Bruti Page: {name}"})
    for name in census["laf"]:
        add(f"laf:{slug(name)}", f"Lost and Found: {name}", "Hogwarts", "lost_and_found",
            swap={"grant": "item_add", "arg": None, "item": name})
    for name in census["statues"]:
        add(f"statue:{slug(name)}", f"Statue: {name}", "Hogwarts", "statues")
    for key, s, chapter, spells in INGREDIENTS:
        add(f"ingredient:{s}", f"{chapter}: Ingredient {census['ingredients'][key]}", chapter, "potions",
            acc=access(chapter, (), spells))
    for key, s, chapter, prereq in POTIONS:
        add(f"potion:{s}", f"{chapter}: Mix {census['potions'][key]}", chapter, "potions",
            acc=access(chapter, (), (), [f"task:{chapter.lower()}:{p}" for p in prereq]))
    for key, s, spells in MINIGAMES:
        add(f"minigame:{s}:complete", f"Bonus: {census['minigames'][key]}", "BonusActivities", "minigames",
            acc=access(None, ["BonusActivities"], spells))
    for chapter in MARAUDERS:
        add(f"marauder:{chapter.lower()}", f"{chapter}: Marauder's Challenge", "MaraudersChallenge",
            "marauders_challenge", acc=access(chapter, ["MaraudersChallenge"]))
    for track, display, milestones, spells in FRED_AND_GEORGE:
        for k in milestones:
            add(f"fg:{track}:{k}", f"Fred and George: {display.format(n=k)}", "Hogwarts", "fred_and_george",
                acc=access(None, (), spells))
    for s, n, item, passes in SHOP:
        if item and item.startswith("spell:"):
            key = item.split(":")[1]
            name, cls, swap = f"{spell[key]} Spellbook", "progression-spell", spell_swap(key)
        else:
            name = census["shop"][n] + (f" ({s[-1]})" if s.startswith("rare_card") else "")
            target = census["shop"][n] if item == "text" else item
            cls, swap = "reward", ({"grant": "item_add", "arg": None, "item": target} if item else None)
        add(f"shop:{s}", f"Shop: {name}", "Hogwarts", "shop", cls, swap, access(passes=passes))
    for key, name in census["secrets"]:
        add(f"secret:{key[len('GUI_LOCTXT_'):].lower()}", f"Secret: {name}", "Hogwarts", "secrets")
    for k in HOUSE_POINTS:
        add(f"point:{k}", f"House Points: {k}", "Hogwarts", "house_points",
            acc=access(passes=k // HOUSE_POINTS_PER_PASS))
    for k in BEANS:
        add(f"bean:{k}", f"Beans: {k}", "Hogwarts", "bean_milestones", acc=access(passes=k // BEANS_PER_PASS))
    return locs


def assign_ids(items, locs, lock):
    """Reuse locked IDs, give new keys the next number; a key never changes ID and holes are never reused."""
    out = {"locations": dict(lock.get("locations", {})), "items": dict(lock.get("items", {}))}
    for table, base, keys in (("locations", LOCATION_BASE, [loc["key"] for loc in locs]),
                              ("items", ITEM_BASE, list(items))):
        ids = out[table]
        nxt = max(ids.values(), default=base) + 1
        for key in keys:
            if key not in ids:
                ids[key] = nxt
                nxt += 1
    for loc in locs:
        loc["id"] = out["locations"][loc["key"]]
    for name, item in items.items():
        item["id"] = out["items"][name]
    return out


def version_of(catalog):
    body = {k: v for k, v in catalog.items() if k != "version"}
    canonical = json.dumps(body, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()[:12]


def build(census, lock):
    items = build_items(census)
    locs = build_locations(census)
    new_lock = assign_ids(items, locs, lock)
    catalog = {
        "schema": SCHEMA,
        "game": GAME,
        "ids": {"location_base": LOCATION_BASE, "item_base": ITEM_BASE},
        "regions": {"start": START, "chapters": [r for r, _ in CHAPTERS], "areas": [r for r, _, _ in AREAS],
                    "area_chapters": {r: c for r, _, c in AREAS}},
        "goal": {"region": "DarkTower", "tokens_required": 8,
                 "requires": ["Night 5 Pass", "Dark Tower Pass", census["spell"]["EXPECTOPATRONUM"]]},
        "items": items,
        "locations": locs,
        "groups": {g: {"preset": presets, "option": option, "count": sum(loc["group"] == g for loc in locs)}
                   for g, (presets, option) in GROUPS.items()},
    }
    return {"version": version_of(catalog), **catalog}, new_lock


def dump(data):
    return json.dumps(data, indent=1, ensure_ascii=False) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--game-dir", default=os.environ.get("HPPOA_GAME_DIR", DEFAULT_GAME_DIR))
    parser.add_argument("--check", action="store_true", help="fail if a committed file would change")
    args = parser.parse_args()
    lock = {}
    if os.path.exists(LOCK):
        with open(LOCK, encoding="utf-8") as f:
            lock = json.load(f)
    catalog, new_lock = build(read_census(Text(args.game_dir)), lock)
    texts = {path: dump(catalog) for path in OUTPUTS}
    texts[LOCK] = dump(new_lock)
    if args.check:
        stale = [p for p, t in texts.items() if not os.path.exists(p) or open(p, encoding="utf-8").read() != t]
        if stale:
            raise SystemExit(f"out of date: {', '.join(stale)} (run make_catalog.py)")
        print(f"catalog {catalog['version']} up to date")
        return
    for path, text in texts.items():
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    print(f"catalog {catalog['version']}: {len(catalog['locations'])} locations, {len(catalog['items'])} items")


if __name__ == "__main__":
    main()
