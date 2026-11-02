from test.bases import WorldTestBase
from .. import CATALOG, CARDS, GAME, GROUPS, LOCATIONS, SPELLS, verify_catalog
from ..options import HppoaOptions

COUNTS = {"tasks": 28, "chapters": 12, "spellbooks": 4, "cards": 50, "card_sets": 10, "folio": 16,
          "lost_and_found": 10, "statues": 9}


def enabled_count(world):
    return sum(GROUPS[g]["count"] for g, on in world.enabled.items() if on)


class HppoaTestBase(WorldTestBase):
    game = GAME

    def checks(self):
        return [loc for loc in self.multiworld.get_locations(self.player) if loc.address is not None]

    def assert_balanced(self):
        self.assertEqual(len(self.checks()), enabled_count(self.world))
        self.assertEqual(len(self.multiworld.itempool), len(self.checks()))


class TestStandard(HppoaTestBase):
    def test_counts_and_pool(self):
        self.assert_balanced()
        for group, count in COUNTS.items():
            self.assertEqual(GROUPS[group]["count"], count, group)
        names = {item.name for item in self.multiworld.itempool}
        self.assertTrue(set(CARDS) <= names)
        self.assertTrue(set(SPELLS) - {"Flipendo"} <= names)
        self.assertNotIn("Day 1 Pass", names)

    def test_ids_in_range(self):
        ids = CATALOG["ids"]
        self.assertTrue(all(ids["location_base"] < loc["id"] < ids["item_base"] for loc in LOCATIONS))
        self.assertEqual(len({loc["id"] for loc in LOCATIONS}), len(LOCATIONS))

    def test_every_group_has_an_option(self):
        fields = set(HppoaOptions.__dataclass_fields__)
        for group, spec in GROUPS.items():
            self.assertTrue(spec["option"] is None or spec["option"] in fields, group)

    def test_no_spell_locks_itself(self):
        """Every spell's check is reachable in logic without that spell."""
        for loc in LOCATIONS:
            swap = loc["swap"]
            if not swap or swap["grant"] != "spell_grant":
                continue
            with self.subTest(spell=swap["item"]):
                self.collect_all_but([swap["item"], "Victory"])
                self.assertTrue(self.can_reach_location(loc["name"]))
                self.multiworld.state = self.multiworld.state.__class__(self.multiworld)

    def test_bumped_catalog_is_refused(self):
        verify_catalog(CATALOG)
        with self.assertRaises(ValueError):
            verify_catalog(dict(CATALOG, version="000000000000"))
        with self.assertRaises(ValueError):
            verify_catalog(dict(CATALOG, ids={"location_base": 1, "item_base": 2}))

    def test_seeded_chapter_ends_open_at_start(self):
        for chapter in ("Intro", "HogwartsExpress", "Night0"):
            self.assertTrue(self.can_reach_location(f"{chapter}: Chapter Complete"))
        self.assertFalse(self.can_reach_location("Night5: Chapter Complete"))

    def test_slot_contract(self):
        slot = self.world.fill_slot_data()
        self.assertEqual(slot["catalog"], CATALOG["version"])
        self.assertEqual(slot["start"], "Day1")
        self.assertEqual(slot["goal"], "dark_tower")
        self.assertEqual(slot["tokens_required"], 8)
        self.assertTrue(slot["groups"]["cards"])
        self.assertFalse(slot["groups"]["talks"])
        self.assertEqual(set(slot["groups"]), set(GROUPS))


class TestMinimal(HppoaTestBase):
    options = {"check_slop": "minimal"}

    def test_story_only(self):
        self.assert_balanced()
        self.assertEqual(len(self.checks()), COUNTS["tasks"] + COUNTS["chapters"] + COUNTS["spellbooks"])
        self.assertFalse(any(item.name in CARDS for item in self.multiworld.itempool))


class TestMaximum(HppoaTestBase):
    options = {"check_slop": "maximum", "tokens_required": 11}

    def test_everything(self):
        self.assert_balanced()
        self.assertEqual(len(self.checks()), len(LOCATIONS))
        self.assertTrue(self.world.fill_slot_data()["groups"]["talks"])


class TestOverrides(HppoaTestBase):
    options = {"check_slop": "minimal", "include_secrets": "on", "include_cards": "on"}

    def test_overrides_win(self):
        self.assert_balanced()
        groups = self.world.fill_slot_data()["groups"]
        self.assertTrue(groups["secrets"] and groups["cards"])
        self.assertFalse(groups["shop"])


class TestTrimStandard(HppoaTestBase):
    options = {"include_cards": "off", "include_shop": "off"}

    def test_trimmed(self):
        self.assert_balanced()
        names = {item.name for item in self.multiworld.itempool}
        self.assertFalse(set(CARDS) & names)
        self.assertNotIn("Owl Care Kit", names)
        self.assertNotIn("Dark Forces Chapter", names)
        self.assertIn("Endurance Upgrade", names)


class TestCardsGoal(HppoaTestBase):
    options = {"check_slop": "minimal", "goal": "cards", "cards_goal": 30}

    def test_cards_forced_on(self):
        self.assert_balanced()
        self.assertTrue(self.world.fill_slot_data()["groups"]["cards"])


class TestHouseCup(HppoaTestBase):
    options = {"goal": "house_cup"}


class TestNoSpellShuffle(HppoaTestBase):
    options = {"shuffle_spells": False, "check_slop": "minimal"}

    def test_spells_known(self):
        self.assert_balanced()
        self.assertFalse(any(item.name in SPELLS for item in self.multiworld.itempool))
        self.assertTrue(all(self.multiworld.state.has(s, self.player) for s in SPELLS))
