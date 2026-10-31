"""Catalog contract tests (docs/ap/08-catalog-spec.md §11), standard library only.

    python test_catalog.py          (HPPOA_GAME_DIR=<hp3_recomp>/game for the two tests that rebuild)
"""
import copy
import json
import os
import re
import unittest

import make_catalog as mc

HERE = os.path.dirname(os.path.abspath(__file__))
GAME_DIR = os.environ.get("HPPOA_GAME_DIR", mc.DEFAULT_GAME_DIR)
CENSUS = mc.read_census(mc.Text(GAME_DIR)) if os.path.isdir(GAME_DIR) else None
KEY = re.compile(r"^[a-z_]+:[a-z0-9_]+(:[a-z0-9_]+)?$")
# census counts (02-census.md, docs/ap/08 §5)
COUNTS = {"tasks": 28, "chapters": 12, "spellbooks": 4, "cards": 50, "card_sets": 10, "folio": 16,
          "lost_and_found": 10, "statues": 9, "potions": 11, "minigames": 8, "marauders_challenge": 10,
          "fred_and_george": 12, "shop": 13, "secrets": 7, "house_points": 10, "bean_milestones": 11}


def load(name):
    with open(os.path.join(HERE, name), encoding="utf-8") as f:
        return json.load(f)


class TestCatalog(unittest.TestCase):
    def setUp(self):
        self.catalog = load("catalog.json")
        self.lock = load("ids.lock.json")

    @unittest.skipUnless(os.path.isdir(GAME_DIR), "needs --game-dir / HPPOA_GAME_DIR")
    def test_committed_catalog_is_current(self):
        catalog, lock = mc.build(mc.read_census(mc.Text(GAME_DIR)), self.lock)
        self.assertEqual(catalog, self.catalog, "catalog.json is stale: run make_catalog.py")
        self.assertEqual(lock, self.lock)

    def test_apworld_copy_is_verbatim(self):
        with open(mc.OUTPUTS[0], "rb") as a, open(mc.OUTPUTS[1], "rb") as b:
            self.assertEqual(a.read(), b.read())

    def test_version_is_canonical_hash(self):
        self.assertEqual(self.catalog["version"], mc.version_of(self.catalog))
        bumped = dict(self.catalog, version="000000000000")
        self.assertNotEqual(bumped["version"], mc.version_of(bumped))

    def test_ids_unique_and_in_range(self):
        loc_ids = [loc["id"] for loc in self.catalog["locations"]]
        item_ids = [item["id"] for item in self.catalog["items"].values()]
        self.assertEqual(len(loc_ids), len(set(loc_ids)))
        self.assertEqual(len(item_ids), len(set(item_ids)))
        self.assertTrue(all(mc.LOCATION_BASE < i < mc.ITEM_BASE for i in loc_ids))
        self.assertTrue(all(mc.ITEM_BASE < i < mc.ITEM_BASE + 10000 for i in item_ids))

    def test_keys_and_names(self):
        keys = [loc["key"] for loc in self.catalog["locations"]]
        names = [loc["name"] for loc in self.catalog["locations"]]
        self.assertEqual(len(keys), len(set(keys)))
        self.assertEqual(len(names), len(set(names)))
        for key in keys:
            self.assertRegex(key, KEY)

    def test_every_location_is_well_formed(self):
        regions = {"Hogwarts"} | set(self.catalog["regions"]["chapters"]) | set(self.catalog["regions"]["areas"])
        items = self.catalog["items"]
        keys = {loc["key"] for loc in self.catalog["locations"]}
        for loc in self.catalog["locations"]:
            self.assertIn(loc["group"], self.catalog["groups"])
            self.assertIn(loc["region"], regions)
            self.assertIn(loc["reward_class"], ("reward", "progression-spell", "progression-story"))
            if loc["reward_class"] == "progression-story":
                self.assertIsNone(loc["swap"])
            if loc["reward_class"] == "progression-spell":
                self.assertEqual(loc["swap"]["grant"], "spell_grant")
            if loc["swap"]:
                self.assertIn(loc["swap"]["item"], items)
            acc = loc["access"]
            for name in [acc["chapter"]] * bool(acc["chapter"]) + acc["areas"] + acc["spells"] + acc.get("cards", []):
                self.assertIn(name, items, loc["key"])
            for key in acc["prereq"]:
                self.assertIn(key, keys)

    @unittest.skipUnless(CENSUS, "needs HPPOA_GAME_DIR")
    def test_ids_are_stable(self):
        """A key keeps its ID when keys are added before it, and a removed key leaves a hole."""
        lock = copy.deepcopy(self.lock)
        removed = next(iter(lock["locations"]))
        lock["locations"]["zzz:new"] = max(lock["locations"].values()) + 7
        catalog, new_lock = mc.build(CENSUS, lock)
        for loc in catalog["locations"]:
            self.assertEqual(loc["id"], self.lock["locations"][loc["key"]])
        self.assertIn(removed, new_lock["locations"])

    def test_presets_monotone_and_options(self):
        order = mc.PRESETS
        for group, spec in self.catalog["groups"].items():
            first = min(order.index(p) for p in spec["preset"])
            self.assertEqual(spec["preset"], order[first:], group)
            self.assertTrue(spec["option"] is None or spec["option"] == f"include_{group}", group)

    def test_counts(self):
        for group, count in COUNTS.items():
            self.assertEqual(self.catalog["groups"][group]["count"], count, group)

    def test_spells(self):
        items = self.catalog["items"]
        spells = [name for name, item in items.items() if item["kind"] == "spell"]
        self.assertEqual(len(spells), 10)
        self.assertEqual([s for s in spells if items[s].get("start")], ["Flipendo"])
        sources = [loc["swap"]["item"] for loc in self.catalog["locations"] if loc["reward_class"] == "progression-spell"]
        self.assertEqual(sorted(sources), sorted(s for s in spells if s != "Flipendo"))


if __name__ == "__main__":
    unittest.main()
