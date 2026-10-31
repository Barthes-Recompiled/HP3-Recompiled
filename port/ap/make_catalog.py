"""Build port/ap/catalog.json, the Archipelago ID source shared by hppoa.exe's bridge and the apworld
(docs/ap/08-catalog-spec.md). The catalog keeps IDs, display names, keys and logic inputs; nothing from the game
beyond names the game prints.

    python make_catalog.py [--out catalog.json] [--lock ids.lock.json] [--apt-keys apt-keys.tsv] [--check]

The tables below are the census (docs/ap/02-census.md); every named row carries the Apt_Text key it was read from.
--apt-keys points at a key<TAB>value dump of Apt_Text.txt (out/ap/apt-keys.tsv, not committed) and checks that each
cited key exists and contains the name, so a typo or a wrong key fails the build instead of shipping.

IDs: a key keeps its ID forever. ids.lock.json (committed) records every key ever issued; new keys get the next free
number and a removed key leaves a hole. --check fails if the lock would change (CI / review).
Version: sha256 of the canonical catalog (sorted keys, no whitespace, without "version") truncated to 12 hex chars.
"""
import argparse
import hashlib
import json
import os
import re
import sys

GAME = "Harry Potter and the Prisoner of Azkaban Recomp"
SCHEMA = 1
LOCATION_BASE = 74300000
ITEM_BASE = 74310000
HERE = os.path.dirname(os.path.abspath(__file__))

# region, display name; the game's chapter table order (01-audit §4.2)
CHAPTERS = [("Intro", "Intro"), ("HogwartsExpress", "Hogwarts Express"), ("Night0", "Night 0"), ("Day1", "Day 1"),
            ("Night1", "Night 1"), ("Day2", "Day 2"), ("Night2", "Night 2"), ("Day3", "Day 3"),
            ("Night3", "Night 3"), ("Day5", "Day 5"), ("Night5", "Night 5"), ("Day6", "Day 6")]
CHAPTER_NAME = dict(CHAPTERS)
START = "Day1"
# seeded complete at the Day 1 start (design §3); their passes are pre-collected
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
# name, candidate spell id (08 §10, advisory until one live grant), source; GUI_SPLTXT_* names them
SPELLS = [("Flipendo", 7, "start"), ("Expelliarmus", 9, "Night0"), ("Lumos Duo", 8, "Night1"),
          ("Glacius", 10, "Day1"), ("Reparo", 11, "Night2"), ("Carpe Retractum", 13, "Day3"),
          ("Expecto Patronum", 14, "Night3"), ("Draconifors", 15, "Night3"), ("Snufflifors", 12, "shop"),
          ("Mucus Projectum", 16, "shop")]
SPELL_ID = {name: sid for name, sid, _ in SPELLS}
# the 11 task-list chapters yield a story token (design §5); HOGEX and Night0 are pre-collected
TOKEN_CHAPTERS = ["HogwartsExpress", "Night0", "Day1", "Night1", "Day2", "Night2", "Day3", "Night3", "Day5",
                  "Night5", "Day6"]

# Story tasks (GUI_TASKLIST_<CH>_00n). HOGEX and Night0 tasks are pre-completed at the Day 1 start and not listed.
# (chapter, slug, name, text key, areas, spells, prereq slugs, spell taught)
TASKS = [
    ("Day1", "dada", "DADA Class", "GUI_TASKLIST_DAY1_001", [], [], [], None),
    ("Day1", "glacius", "Glacius Spellbook", "GUI_TASKLIST_DAY1_002", ["GlaciusChallenge"], [], ["dada"], "Glacius"),
    ("Day1", "potions", "Potions Class (Wiggenweld)", "GUI_TASKLIST_DAY1_003", [], [], [], None),
    ("Day1", "broom", "Broom Flying Class", "GUI_TASKLIST_DAY1_004", [], [], [], None),
    ("Night1", "neville", "Rescue Neville", "GUI_TASKLIST_NIGHT1_001", [], [], [], None),
    ("Night1", "common_room", "Return to the Common Room", "GUI_TASKLIST_NIGHT1_002", [], [], ["neville"], None),
    ("Day2", "comc", "Care of Magical Creatures", "GUI_TASKLIST_DAY2_001", [], [], [], None),
    ("Night2", "follow_ron", "Follow Ron into the Dungeons", "GUI_TASKLIST_NIGHT2_001", [], [], [], None),
    ("Night2", "escape", "Escape the Dungeons", "GUI_TASKLIST_NIGHT2_002", [], [], ["follow_ron"], None),
    ("Day3", "antidote_ingredients", "Antidote Ingredients", "GUI_TASKLIST_DAY3_001", [], [], [], None),
    ("Day3", "mix_antidote", "Mix the Antidote", "GUI_TASKLIST_DAY3_002", [], [], ["antidote_ingredients"], None),
    ("Day3", "charms", "Charms Class", "GUI_TASKLIST_DAY3_003", [], [], [], None),
    ("Day3", "carpe", "Carpe Retractum Spellbook", "GUI_TASKLIST_DAY3_004", ["CarpeRetractumChallenge"], [],
     ["charms"], "Carpe Retractum"),
    ("Night3", "lupin", "Meet Lupin (History of Magic)", "GUI_TASKLIST_NIGHT3_001", [], [], [], None),
    ("Night3", "patronus", "Expecto Patronum Spellbook", "GUI_TASKLIST_NIGHT3_002", ["PatronusChallenge"], [],
     ["lupin"], "Expecto Patronum"),
    ("Night3", "gunhilda", "Enter the Gunhilda Statue", "GUI_TASKLIST_NIGHT3_003", [], [], [], None),
    ("Night3", "common_room", "Return to the Common Room", "GUI_TASKLIST_NIGHT3_004", [], [], ["gunhilda"], None),
    # the flying seahorses are caught with Carpe Retractum (Flying Seahorse Fishing)
    ("Day5", "girding_ingredients", "Girding Ingredients", "GUI_TASKLIST_DAY5_001", [], ["Carpe Retractum"], [],
     None),
    ("Day5", "mix_girding", "Mix the Girding Potion", "GUI_TASKLIST_DAY5_002", [], [], ["girding_ingredients"], None),
    ("Day5", "crate", "Quidditch Ball Crate", "GUI_TASKLIST_DAY5_003", [], [], [], None),
    ("Day5", "stadium", "Crate to the Stadium", "GUI_TASKLIST_DAY5_004", ["QuidditchMatch"], [], ["crate"], None),
    ("Night5", "hagrid", "Visit Hagrid", "GUI_TASKLIST_NIGHT5_001", [], [], [], None),
    ("Night5", "protect_sirius", "Protect Sirius", "GUI_TASKLIST_NIGHT5_002", ["DementorBattle"],
     ["Expecto Patronum"], ["hagrid"], None),
    ("Night5", "repel", "Repel the Dementors", "GUI_TASKLIST_NIGHT5_003", ["DementorBattle"], ["Expecto Patronum"],
     ["protect_sirius"], None),
    ("Night5", "buckbeak", "Fly Buckbeak to the Dark Tower", "GUI_TASKLIST_NIGHT5_004", ["DarkTower"], [],
     ["repel"], None),
    ("Day6", "cards", "Collector's Cards", "GUI_TASKLIST_DAY6_001", [], [], [], None),
    ("Day6", "house_points", "House Points", "GUI_TASKLIST_DAY6_002", [], [], [], None),
    ("Day6", "house_cup", "House Cup Ceremony", "GUI_TASKLIST_DAY6_003", ["HouseCup"], [], [], None),
]
# chapter-end checks: every chapter playable after the Day 1 start except Night5, whose end is the goal
CHAPTER_ENDS = ["Day1", "Night1", "Day2", "Night2", "Day3", "Night3", "Day5", "Day6"]
# extra areas a chapter end needs (design §5 places the Shrieking Shack in Night3; a logic guess, not verified)
CHAPTER_END_AREAS = {"Night3": ["ShriekingShack"]}
# spellbooks that are not tasks (02 §15): slug, chapter, name, spell
SPELLBOOKS = [("expelliarmus", "Night0", "Peeves' Spellbook", "Expelliarmus"),
              ("lumos_duo", "Night1", "Muggle Studies Spellbook", "Lumos Duo"),
              ("reparo", "Night2", "Lower Dungeon Spellbook", "Reparo"),
              ("draconifors", "Night3", "Hermione's Second Spellbook", "Draconifors")]

# out/ap/card_table.tsv: id, set, name (GUI_NAR_CARDS<set>_00<2*slot-1>); card N is bit N-1
CARD_SETS = [
    ("witches", "Witches", ["Gunhilda of Gorsemoor", "Morgan le Fay", "Queen Maeve", "Ignatia Wildsmith",
                            "Beatrix Bloxam"]),
    ("wizards", "Wizards", ["Albus Dumbledore", "Merlin", "Glanmore Peakes", "Herpo the Foul", "Edgar Stroulger"]),
    ("vampires", "Vampires", ["Count Vlad Drakul", "Lady Carmilla Sanguina", "Amarillo Lestoat",
                              "Sir Herbert Varney", "Blodwyn Bludd"]),
    ("goblins", "Goblins", ["Gringott", "Eargit the Ugly", "Alguff the Awful", "Ug the Unreliable",
                            "Urg the Unclean"]),
    ("hags", "Hags", ["Cordelia Misericordia", "Babayaga", "Malodora Grymm", "Leticia Somnolens",
                      "Old Mother Hubbard"]),
    ("giants", "Giants", ["Hengist of Upper Barnton", "Bran the Bloodthirsty", "Cyclops", "Goliath", "Morholt"]),
    ("dragons", "Dragons", ["Norwegian Ridgeback Dragon", "Common Welsh Green Dragon", "Hebridean Black Dragon",
                            "Hungarian Horntail Dragon", "Romanian Longhorn Dragon"]),
    ("beasts", "Beasts", ["Gnome", "Imp", "Doxy", "Bowtruckle", "Billywig"]),
    ("classical_beasts", "Classical Beasts", ["Phoenix", "Giant Squid", "Manticore", "Unicorn", "Mountain Troll"]),
    ("quidditch", "Quidditch", ["Barberus Bragge", "Cyprian Youdle", "Joscelind Wadcock", "Leopoldina Smethwyck",
                                "Roderick Plumpton"]),
]
# GUI_COLLECT_001..034: these six sets raise a character's Endurance; the other four give house points
ENDURANCE_SETS = {"witches", "wizards", "hags", "dragons", "classical_beasts", "quidditch"}
FOLIO = ["Dementor", "Billywig", "Chizpurfle", "Doxy", "Dragonfly", "Fairy", "Fire Seed Bush", "Flying Seahorse",
         "Ghoul", "Hinkypunk", "Hippogriff", "Pixie", "Red Cap", "Salamander", "Troll", "Venemous Tentacula"]
LOST_AND_FOUND = ["Prefect Badge", "Remembrall", "Potion Vial Case", "Dragon-hide Gloves", "Owl Post Parcel", "Key",
                  "Telescope", "Daily Prophet", "Brass Scales", "Potion Jar"]
STATUES = ["Bridget Wenlock", "Glanmore Peakes", "Hengist of Woodcroft", "Gondoline Oliphant",
           "Beaumont Marjoribanks", "Gunhilda of Gorsemoor", "Gregory the Smarmy", "Jocunda Sykes",
           "Hooky the House Elf"]
# GUI_QST_POT*: (slug, name, text key, chapter, spells)
INGREDIENTS = [("fire_seeds", "Fire Seeds", "GUI_QST_POTFIRESEED_001", "Day3", []),
               ("chizpurfle", "Chizpurfle Carapaces", "GUI_QST_POTCHIZPURFLE_001", "Day3", []),
               ("graphorn_horn", "Graphorn Horn", "GUI_QST_POTGRAPHORNHORN_001", "Day3", []),
               ("billywig_stings", "Billywig Stings", "GUI_QST_POTBILLYWIGSTINGS_001", "Day3", []),
               ("flying_seahorses", "Flying Seahorses", "GUI_QST_POTSEAHORSES_001", "Day5", ["Carpe Retractum"]),
               ("dragonfly_thoraxes", "Dragonfly Thoraxes", "GUI_QST_POTDRAGONFLIES_001", "Day5", []),
               ("doxy_eggs", "Doxy Eggs", "GUI_QST_POTDOXYEGGS_001", "Day5", []),
               ("fairy_wings", "Fairy Wings", "GUI_QST_POTFAIRIES_001", "Day5", [])]
POTIONS = [("wiggenweld", "Wiggenweld Potion", "Day1", []),
           ("antidote", "Antidote to Uncommon Poisons", "Day3", ["antidote_ingredients"]),
           ("girding", "Girding Potion", "Day5", ["girding_ingredients"])]
# bonus activities (GUI_GENFRTXT_<KEY>_001); grade tiers are open, so one check per mode for now
MINIGAMES = [("broom_flight", "Broom Flight Challenge", "LESSONBROOMFLIGHT", []),
             ("advanced_broom_flight", "Advanced Broom Flight Challenge", "BROOMFLIGHT", []),
             ("dragon_statues", "Dragon Statues", "DRAGONS", []),
             ("hippogriff_flight", "Hippogriff Flight Challenge", "HIPPOGRIFFFLIGHT", []),
             ("owl_racing", "Owl Racing", "OWLRACING", []),
             ("seahorse_fishing", "Flying Seahorse Fishing", "CARPEFISHING", ["Carpe Retractum"]),
             ("duelling_club", "Duelling Club", "DUELLINGCLUB", []),
             ("snot_slinging", "Snot Slinging", "SNOTSLINGING", ["Mucus Projectum"])]
# *_marauderschallenge.hpb exists in these ten playable chapters
MARAUDERS = ["Night0", "Day1", "Night1", "Day2", "Night2", "Day3", "Night3", "Day5", "Night5", "Day6"]
# Fred & George's challenge tracks (02 §7): (track, display, milestones, spells)
FRED_AND_GEORGE = [("statues", "Statue Challenge: {n} Statues", [3, 6, 9], []),
                   ("dungbomb", "Dungbomb Challenge: {n} Targets", [5, 10, 15, 20], []),
                   ("snot", "Snot Slinging Busts", [1], ["Mucus Projectum"]),
                   ("confiscated", "Confiscated Items: {n} of 4", [1, 2, 3, 4], [])]
# Fred & George's shop (GUI_FANDG_SHOPTXT_*), one-time purchases only: (slug, name, text key, item, passes)
# Mucus Projectum is not in the shop text; SV3_RON_FANDGRON_015 says they sell it.
SHOP = [("rare_card_1", "Rare Collector's Card 1", "GUI_FANDG_SHOPTXT_004", None, 2),
        ("rare_card_2", "Rare Collector's Card 2", "GUI_FANDG_SHOPTXT_004", None, 4),
        ("owl_care_kit", "Owl Care Kit", "GUI_FANDG_SHOPTXT_005", "Owl Care Kit", 1),
        ("broom_handle_polish", "Broom Handle Polish", "GUI_FANDG_SHOPTXT_006", "Broom Handle Polish", 1),
        ("brass_compass", "Brass Compass", "GUI_FANDG_SHOPTXT_007", "Brass Compass", 2),
        ("tail_twig_clippers", "Silver Tail-Twig Clippers", "GUI_FANDG_SHOPTXT_008", "Silver Tail-Twig Clippers", 2),
        ("snufflifors", "Snufflifors Spellbook", "GUI_FANDG_SHOPTXT_010", "Snufflifors", 1),
        ("mucus_projectum", "Mucus Projectum Spellbook", None, "Mucus Projectum", 2),
        ("dark_forces_1", "Dark Forces: Chapter 1", "GUI_FANDG_SHOPTXT_011", "Dark Forces Chapter", 1),
        ("dark_forces_2", "Dark Forces: Chapter 2", "GUI_FANDG_SHOPTXT_012", "Dark Forces Chapter", 3),
        ("dark_forces_3", "Dark Forces: Chapter 3", "GUI_FANDG_SHOPTXT_013", "Dark Forces Chapter", 5),
        ("stink_pellet_bag", "Stink Pellet Bag", "GUI_FANDG_SHOPTXT_016", "Stink Pellet Bag", 1),
        ("dungbomb_case", "Dungbomb Case", "GUI_FANDG_SHOPTXT_017", "Dungbomb Case", 1)]
# GUI_LOCTXT_*: the named secret places
SECRETS = [("floor7_passage", "Secret Passageway (Seventh Floor)", "GUI_LOCTXT_FLOOR7_006"),
           ("floor4_way_down", "Secret Way to the Second Floor", "GUI_LOCTXT_FLOOR4_004"),
           ("floor4_passage", "Secret Passageway (Fourth Floor)", "GUI_LOCTXT_FLOOR4_006"),
           ("floor4_room", "Secret Room (Fourth Floor)", "GUI_LOCTXT_FLOOR4_007"),
           ("floor2_way_up", "Secret Way to the Fourth Floor", "GUI_LOCTXT_FLOOR2_002"),
           ("floor2_passage", "Secret Passageway (Second Floor)", "GUI_LOCTXT_FLOOR2_003"),
           ("broom_cupboard", "Broom Cupboard", "GUI_LOCTXT_ENTHALL_003")]
# milestones (Housepoints / Beans_Amount); bigger values need more chapters open: one pool chapter pass per step
HOUSE_POINTS = list(range(50, 501, 50))
HOUSE_POINTS_PER_PASS = 100
BEANS = [50, 100, 200, 300, 400, 500, 750, 1000, 1250, 1500, 2000]
BEANS_PER_PASS = 250

PRESETS = ["minimal", "standard", "maximum"]
# group -> (presets it is on in, include_* option or None when always on); empty groups wait for the census
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
    # standard once the placement census lands (08 §5)
    "pickups": (PRESETS[2:], "include_pickups"), "loose_pickups": (PRESETS[2:], "include_loose_pickups"),
    "enemies": (PRESETS[2:], "include_enemies"), "talks": (PRESETS[2:], "include_talks"),
    "pursuits": (PRESETS[2:], "include_pursuits"), "intro_memories": (PRESETS[2:], "include_intro_memories"),
}
FILLERS = ["25 Beans", "50 Beans", "10 House Points", "Wiggenweld Potion", "Antidote to Uncommon Poisons",
           "Girding Potion", "Dungbombs", "Stink Pellets", "Chocolate Frog", "Cauldron Cake"]


def slug(text):
    return re.sub(r"[^a-z0-9]+", "_", text.lower()).strip("_")


def card_item(name):
    return f"{name} Card"


def access(chapter=None, areas=(), spells=(), prereq=(), cards=(), passes=0):
    out = {"chapter": f"{CHAPTER_NAME[chapter]} Pass" if chapter else None,
           "areas": [f"{AREA_NAME[a]} Pass" for a in areas], "spells": list(spells), "prereq": list(prereq)}
    if cards:
        out["cards"] = list(cards)
    if passes:
        out["passes"] = passes
    return out


def build_items():
    items = {}

    def add(name, kind, cls, **extra):
        assert name not in items, name
        items[name] = {"kind": kind, "class": cls, **extra}

    for region, name in CHAPTERS:
        add(f"{name} Pass", "chapter", "progression", region=region,
            **({"start": True} if region in SEEDED + [START] else {}))
    for region, name, chapter in AREAS:
        add(f"{name} Pass", "area", "progression", region=region, chapter=chapter)
    for name, sid, source in SPELLS:
        add(name, "spell", "progression", spell_id=sid, source=source, **({"start": True} if source == "start" else {}))
    for chapter in TOKEN_CHAPTERS:
        add(f"Story Token ({CHAPTER_NAME[chapter]})", "token", "progression",
            **({"start": True} if chapter in SEEDED else {}))
    # collectibles enter the pool only with the group whose vanilla grant they replace ("pool")
    for _, _, names in CARD_SETS:
        for name in names:
            add(card_item(name), "card", "useful", pool=["cards"])
    for name in FOLIO:
        add(f"Folio Bruti Page: {name}", "collectible", "useful", pool=["folio"])
    for name in LOST_AND_FOUND:
        add(name, "collectible", "useful", pool=["lost_and_found"])
    add("Endurance Upgrade", "upgrade", "useful", count=len(ENDURANCE_SETS), pool=["card_sets"])
    add("Dark Forces Chapter", "upgrade", "useful", count=3, pool=["statues", "shop"])
    for _, _, _, item, _ in SHOP:
        if item and item not in items:
            add(item, "upgrade", "useful", pool=["shop"])
    for name in FILLERS:
        add(name, "filler", "filler")
    return items


def build_locations():
    locs = []

    def add(key, name, region, group, reward_class="reward", swap=None, acc=None):
        locs.append({"key": key, "name": name, "region": region, "group": group, "reward_class": reward_class,
                     "swap": swap, "access": acc or access()})

    def spell_swap(spell):
        return {"grant": "spell_grant", "arg": SPELL_ID[spell], "item": spell}

    for chapter, s, name, _, areas, spells, prereq, teaches in TASKS:
        region = areas[0] if areas else chapter
        key = f"task:{chapter.lower()}:{s}"
        add(key, f"{chapter}: {name}", region, "tasks",
            "progression-spell" if teaches else "progression-story",
            spell_swap(teaches) if teaches else None,
            access(chapter, areas, spells, [f"task:{chapter.lower()}:{p}" for p in prereq]))
    for chapter in CHAPTER_ENDS:
        tasks = [f"task:{c.lower()}:{s}" for c, s, *_ in TASKS if c == chapter]
        areas = CHAPTER_END_AREAS.get(chapter, [])
        add(f"chapter:{chapter.lower()}", f"{chapter}: Chapter Complete", areas[0] if areas else chapter,
            "chapters", "progression-story", None, access(chapter, areas, (), tasks))
    for s, chapter, name, spell in SPELLBOOKS:
        add(f"spellbook:{s}", f"{chapter}: {name}", chapter, "spellbooks", "progression-spell", spell_swap(spell),
            access(chapter))
    n = 0
    for set_slug, set_name, names in CARD_SETS:
        for name in names:
            n += 1
            add(f"card:{n}", f"Card {n}: {name}", "Hogwarts", "cards",
                swap={"grant": "card_bit", "arg": n, "item": card_item(name)})
    for set_slug, set_name, names in CARD_SETS:
        swap = {"grant": "item_add", "arg": None, "item": "Endurance Upgrade"} if set_slug in ENDURANCE_SETS else None
        add(f"card_set:{set_slug}", f"Card Set: {set_name}", "Hogwarts", "card_sets", swap=swap,
            acc=access(cards=[card_item(name) for name in names]))
    for i, name in enumerate(FOLIO, 1):
        add(f"folio:{i}", f"Folio Bruti {i}: {name}", "Hogwarts", "folio",
            swap={"grant": "item_add", "arg": None, "item": f"Folio Bruti Page: {name}"})
    for name in LOST_AND_FOUND:
        add(f"laf:{slug(name)}", f"Lost and Found: {name}", "Hogwarts", "lost_and_found",
            swap={"grant": "item_add", "arg": None, "item": name})
    for name in STATUES:
        add(f"statue:{slug(name)}", f"Statue: {name}", "Hogwarts", "statues")
    for k in (3, 6, 9):
        add(f"statue:homework_{k}", f"Hermione's Homework: {k} Statues", "Hogwarts", "statues",
            swap={"grant": "item_add", "arg": None, "item": "Dark Forces Chapter"})
    for s, name, _, chapter, spells in INGREDIENTS:
        add(f"ingredient:{s}", f"{chapter}: Ingredient {name}", chapter, "potions", acc=access(chapter, (), spells))
    for s, name, chapter, prereq in POTIONS:
        task_prereq = [f"task:{chapter.lower()}:{p}" for p in prereq]
        add(f"potion:{s}", f"{chapter}: Mix {name}", chapter, "potions", acc=access(chapter, (), (), task_prereq))
    for s, name, _, spells in MINIGAMES:
        add(f"minigame:{s}:complete", f"Bonus: {name}", "BonusActivities", "minigames",
            acc=access(None, ["BonusActivities"], spells))
    for chapter in MARAUDERS:
        add(f"marauder:{chapter.lower()}", f"{chapter}: Marauder's Challenge", "MaraudersChallenge",
            "marauders_challenge", acc=access(chapter, ["MaraudersChallenge"]))
    for track, display, milestones, spells in FRED_AND_GEORGE:
        for k in milestones:
            add(f"fg:{track}:{k}", f"Fred and George: {display.format(n=k)}", "Hogwarts", "fred_and_george",
                acc=access(None, (), spells))
    for s, name, _, item, passes in SHOP:
        spell = item if item in SPELL_ID else None
        swap = spell_swap(spell) if spell else ({"grant": "item_add", "arg": None, "item": item} if item else None)
        add(f"shop:{s}", f"Shop: {name}", "Hogwarts", "shop", "progression-spell" if spell else "reward", swap,
            access(passes=passes))
    for s, name, _ in SECRETS:
        add(f"secret:{s}", f"Secret: {name}", "Hogwarts", "secrets")
    for k in HOUSE_POINTS:
        add(f"point:{k}", f"House Points: {k}", "Hogwarts", "house_points", acc=access(passes=k // HOUSE_POINTS_PER_PASS))
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


def verify_text(path):
    """Every cited Apt_Text key exists and contains the name we print."""
    with open(path, encoding="utf-8") as f:
        text = dict(line.rstrip("\n").split("\t", 1) for line in f if "\t" in line)
    norm = lambda s: s.replace("%2D", "-").replace("'", "").lower()
    checks = [(key, None) for _, _, _, key, *_ in TASKS]
    checks += [(f"GUI_BRUTI_TITLE_{i:03d}", n) for i, n in enumerate(FOLIO, 1)]
    checks += [(f"GUI_QST_LAF_{i:03d}", n) for i, n in enumerate(LOST_AND_FOUND, 1)]
    checks += [(f"GUI_STATUES_FETXT_{i:03d}", n) for i, n in enumerate(STATUES, 1)]
    checks += [(key, name) for _, name, key, _, _ in INGREDIENTS]
    checks += [(f"GUI_GENFRTXT_{key}_001", None) for _, _, key, _ in MINIGAMES]
    checks += [(key, name.rstrip(" 12")) for _, name, key, _, _ in SHOP if key and "Dark Forces" not in name]
    checks += [(key, None) for _, _, key in SECRETS]
    checks += [(f"GUI_SPLTXT_{slug(n).replace('_', '').upper()}_001", None) for n, _, _ in SPELLS
               if n != "Flipendo"]
    bad = [(k, n, text.get(k)) for k, n in checks if k not in text or (n and norm(n) not in norm(text[k]))]
    for key, name, value in bad:
        print(f"text check failed: {key} (expected {name!r}) = {value!r}", file=sys.stderr)
    return not bad


def build(lock):
    items = build_items()
    locs = build_locations()
    new_lock = assign_ids(items, locs, lock)
    catalog = {
        "schema": SCHEMA,
        "game": GAME,
        "ids": {"location_base": LOCATION_BASE, "item_base": ITEM_BASE},
        "regions": {"start": START, "chapters": [r for r, _ in CHAPTERS], "areas": [r for r, _, _ in AREAS],
                    "area_chapters": {r: c for r, _, c in AREAS}},
        "goal": {"region": "DarkTower", "tokens_required": 8, "requires": ["Night 5 Pass", "Dark Tower Pass",
                                                                            "Expecto Patronum"]},
        "items": items,
        "locations": locs,
        "groups": {g: {"preset": presets, "option": option, "count": sum(loc["group"] == g for loc in locs)}
                   for g, (presets, option) in GROUPS.items()},
    }
    canonical = json.dumps(catalog, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
    catalog = {"version": hashlib.sha256(canonical.encode("utf-8")).hexdigest()[:12], **catalog}
    return catalog, new_lock


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", default=os.path.join(HERE, "catalog.json"))
    parser.add_argument("--lock", default=os.path.join(HERE, "ids.lock.json"))
    parser.add_argument("--apt-keys", help="key<TAB>value dump of Apt_Text.txt to check the cited text keys")
    parser.add_argument("--check", action="store_true", help="fail if the catalog or the ID lock would change")
    args = parser.parse_args()
    if args.apt_keys and not verify_text(args.apt_keys):
        raise SystemExit(1)
    lock = {}
    if os.path.exists(args.lock):
        with open(args.lock, encoding="utf-8") as f:
            lock = json.load(f)
    catalog, new_lock = build(lock)
    outputs = {args.out: catalog, args.lock: new_lock}
    texts = {path: json.dumps(data, indent=1, ensure_ascii=False) + "\n" for path, data in outputs.items()}
    if args.check:
        stale = [p for p, t in texts.items() if not os.path.exists(p) or open(p, encoding="utf-8").read() != t]
        if stale:
            raise SystemExit(f"out of date: {', '.join(stale)} (run make_catalog.py)")
        print(f"catalog {catalog['version']} up to date")
        return
    for path, text in texts.items():
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    print(f"catalog {catalog['version']}: {len(catalog['locations'])} locations, {len(catalog['items'])} items")


if __name__ == "__main__":
    main()
