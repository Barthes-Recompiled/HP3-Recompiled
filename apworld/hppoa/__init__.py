"""Harry Potter and the Prisoner of Azkaban (Xbox, recomp) - developer world. With warm thanks to MiddleDucky, whose
recompilation work the PC port this world plays on is built from.

IDs, names, groups and logic inputs come from data/catalog.json, a verbatim copy of the port's port/ap/catalog.json
(made by port/ap/make_catalog.py, contract in docs/ap/08-catalog-spec.md); the game refuses a slot whose catalog
version differs from the one it was built with.

Structure (docs/ap/03-design.md): the seed starts on Day 1 with the Intro, Hogwarts Express and Night 0 behind it.
Each chapter is a region opened by its pass; each area (challenge rooms, the Shrieking Shack, the Dementor battle,
the Dark Tower, ...) is a region opened by its area pass plus its chapter's pass. Checks without a place (cards,
folio, statues, shop, milestones) sit in Hogwarts and are open from the start. A location's own access entry adds
spells, prerequisite tasks and, for card sets, the set's cards.
"""
import hashlib
import json
import pkgutil

from BaseClasses import Item, ItemClassification, Location, Region, Tutorial
from Options import OptionError
from worlds.AutoWorld import WebWorld, World
from .options import HppoaOptions, option_groups

GAME = "Harry Potter and the Prisoner of Azkaban Recomp"


def verify_catalog(catalog: dict) -> dict:
    """Refuse a catalog whose version is not the hash of its content (a hand edit or a bumped version)."""
    body = {k: v for k, v in catalog.items() if k != "version"}
    canonical = json.dumps(body, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
    if catalog.get("version") != hashlib.sha256(canonical.encode("utf-8")).hexdigest()[:12]:
        raise ValueError(f"catalog version {catalog.get('version')} does not match its content; "
                         "regenerate it with port/ap/make_catalog.py")
    if catalog.get("schema") != 1:
        raise ValueError(f"catalog schema {catalog.get('schema')} is not supported by this world")
    return catalog


CATALOG = verify_catalog(json.loads(pkgutil.get_data(__name__, "data/catalog.json")))
ITEMS = CATALOG["items"]
LOCATIONS = CATALOG["locations"]
BY_KEY = {loc["key"]: loc for loc in LOCATIONS}
GROUPS = CATALOG["groups"]
REGIONS = CATALOG["regions"]
PRESETS = ["minimal", "standard", "maximum"]


def _kind(kind):
    return [name for name, item in ITEMS.items() if item["kind"] == kind]


CHAPTER_PASSES = _kind("chapter")
POOL_PASSES = [name for name in CHAPTER_PASSES if not ITEMS[name].get("start")]
AREA_PASSES = _kind("area")
SPELLS = _kind("spell")
TOKENS = _kind("token")
CARDS = [name for name, item in ITEMS.items() if item.get("collect") == "card"]
REGION_PASS = {ITEMS[name]["region"]: name for name in CHAPTER_PASSES + AREA_PASSES}
CLASSES = {"progression": ItemClassification.progression, "useful": ItemClassification.useful,
           "filler": ItemClassification.filler}
FILLERS = _kind("filler")


class HppoaItem(Item):
    game = GAME


class HppoaLocation(Location):
    game = GAME


class HppoaWeb(WebWorld):
    theme = "stone"
    option_groups = option_groups
    tutorials = [Tutorial("Setup Guide", "Connecting the Prisoner of Azkaban PC port to a multiworld.",
                          "English", "setup_en.md", "setup/en", ["Jerrod"])]


def requirements(loc, seen=None):
    """Item names a location needs (chapter, areas, spells, cards), following prerequisite tasks."""
    seen = seen if seen is not None else set()
    acc = loc["access"]
    need = set(acc["areas"]) | set(acc["spells"]) | set(acc.get("cards", []))
    if acc["chapter"]:
        need.add(acc["chapter"])
    for key in acc["prereq"]:
        if key not in seen:
            seen.add(key)
            need |= requirements(BY_KEY[key], seen)
    return need


class HppoaWorld(World):
    """Harry Potter and the Prisoner of Azkaban on the original Xbox, as a native PC port: chapters, challenge rooms,
    spells, collector's cards and the rest of Hogwarts' check slop shuffled into the multiworld."""

    game = GAME
    web = HppoaWeb()
    options_dataclass = HppoaOptions
    options: HppoaOptions
    item_name_to_id = {name: item["id"] for name, item in ITEMS.items()}
    location_name_to_id = {loc["name"]: loc["id"] for loc in LOCATIONS}
    item_name_groups = {"Chapter Passes": set(CHAPTER_PASSES), "Area Passes": set(AREA_PASSES),
                        "Spells": set(SPELLS), "Story Tokens": set(TOKENS), "Collector's Cards": set(CARDS)}
    location_name_groups = {}
    for _loc in LOCATIONS:
        location_name_groups.setdefault(_loc["group"], set()).add(_loc["name"])
    del _loc

    def generate_early(self) -> None:

        preset = PRESETS[self.options.check_slop.value]
        self.enabled = {}
        for group, spec in GROUPS.items():
            value = getattr(self.options, spec["option"]).value if spec["option"] else 1
            self.enabled[group] = preset in spec["preset"] if value == 0 else value == 1
        if self.options.goal.value == self.options.goal.option_cards:
            self.enabled["cards"] = True
        self.start_items = [name for name, item in ITEMS.items() if item.get("start")]
        if not self.options.shuffle_spells.value:
            self.start_items += [s for s in SPELLS if s not in self.start_items]
        for name in self.start_items:
            self.multiworld.push_precollected(self.create_item(name))

    def _cards_progression(self) -> bool:
        return self.enabled["card_sets"] or self.options.goal.value == self.options.goal.option_cards

    def create_item(self, name: str) -> HppoaItem:
        item = ITEMS[name]
        classification = CLASSES[item["class"]]
        if item.get("collect") == "card" and self._cards_progression():
            # counted by the card sets and the cards goal: logic needs them, but no balancing sphere hunt
            classification = ItemClassification.progression_skip_balancing
        elif item["kind"] == "token" and self.options.goal.value != self.options.goal.option_dark_tower:
            classification = ItemClassification.useful
        return HppoaItem(name, classification, self.item_name_to_id[name], self.player)

    def get_filler_item_name(self) -> str:
        return self.random.choice(FILLERS)

    def create_regions(self) -> None:
        menu = Region("Menu", self.player, self.multiworld)
        hogwarts = Region("Hogwarts", self.player, self.multiworld)
        menu.connect(hogwarts)
        regions = {"Hogwarts": hogwarts}
        for chapter in REGIONS["chapters"]:
            regions[chapter] = Region(chapter, self.player, self.multiworld)
            hogwarts.connect(regions[chapter], None,
                             lambda state, p=REGION_PASS[chapter]: state.has(p, self.player))
        for area in REGIONS["areas"]:
            chapter = REGIONS["area_chapters"][area]
            need = [REGION_PASS[area]] + ([REGION_PASS[chapter]] if chapter else [])
            regions[area] = Region(area, self.player, self.multiworld)
            hogwarts.connect(regions[area], None, lambda state, n=need: state.has_all(n, self.player))
        self.checks = []
        for loc in LOCATIONS:
            if not self.enabled[loc["group"]]:
                continue
            region = regions[loc["region"]]
            location = HppoaLocation(self.player, loc["name"], loc["id"], region)
            region.locations.append(location)
            self.checks.append((loc, location))
        goal_region = {0: CATALOG["goal"]["region"], 1: "HouseCup", 2: "Hogwarts"}[self.options.goal.value]
        goal = HppoaLocation(self.player, "Victory", None, regions[goal_region])
        goal.place_locked_item(HppoaItem("Victory", ItemClassification.progression, None, self.player))
        regions[goal_region].locations.append(goal)
        self.goal_location = goal
        self.multiworld.regions += list(regions.values()) + [menu]

    def create_items(self) -> None:
        start = set(self.start_items)
        progression = [self.create_item(name) for name, item in ITEMS.items()
                       if item["class"] == "progression" and name not in start for _ in range(item.get("count", 1))]
        extras = [self.create_item(name) for name, item in ITEMS.items()
                  if item.get("pool") and any(self.enabled[g] for g in item["pool"])
                  for _ in range(item.get("count", 1))]
        room = len(self.checks)
        if len(progression) > room:
            raise OptionError(f"{GAME}: {room} checks cannot hold {len(progression)} progression items; "
                              "enable more check groups")
        # card items stay together when they are progression; other extras are dropped first
        extras.sort(key=lambda item: not item.advancement)
        pool = progression + extras[:room - len(progression)]
        while len(pool) < room:
            pool.append(self.create_item(self.get_filler_item_name()))
        self.multiworld.itempool += pool

    def set_rules(self) -> None:
        cards_in_pool = self.enabled["cards"]
        for loc, location in self.checks:
            need = requirements(loc)
            if not cards_in_pool:
                need -= set(CARDS)  # vanilla cards: the set completes by play
            passes = loc["access"].get("passes", 0)
            if need or passes:
                location.access_rule = lambda state, n=sorted(need), k=passes: \
                    state.has_all(n, self.player) and (k == 0 or state.has_from_list(POOL_PASSES, self.player, k))
        goal = self.options.goal.value
        if goal == self.options.goal.option_dark_tower:
            need, tokens = CATALOG["goal"]["requires"], self.options.tokens_required.value
            self.goal_location.access_rule = lambda state: \
                state.has_all(need, self.player) and state.has_from_list(TOKENS, self.player, tokens)
        elif goal == self.options.goal.option_house_cup:
            house_cup = requirements(BY_KEY["task:day6:house_cup"])
            self.goal_location.access_rule = lambda state: state.has_all(house_cup, self.player)
        else:
            n = self.options.cards_goal.value
            self.goal_location.access_rule = lambda state: state.has_from_list(CARDS, self.player, n)
        self.multiworld.completion_condition[self.player] = lambda state: state.has("Victory", self.player)

    def fill_slot_data(self) -> dict:
        return {"catalog": CATALOG["version"], "schema": CATALOG["schema"], "start": REGIONS["start"],
                "mode": "areas", "goal": self.options.goal.current_key,
                "tokens_required": self.options.tokens_required.value, "cards_goal": self.options.cards_goal.value,
                "shuffle_spells": bool(self.options.shuffle_spells.value),
                "groups": {group: on for group, on in self.enabled.items()}}
