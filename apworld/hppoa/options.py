from dataclasses import dataclass
from Options import Choice, DefaultOnToggle, OptionGroup, PerGameCommonOptions, Range


class CheckSlop(Choice):
    """How many checks the world has. Every Include option below can override its group.

    **Minimal:** story tasks, chapter ends and spellbooks only (about 40), for races.

    **Standard:** adds cards, card sets, Folio Bruti, Lost and Found, statues, potions, bonus activities, Marauder's
    Challenges, Fred and George, the shop, secrets and house point and bean milestones (about 220).

    **Maximum:** adds the groups still waiting for their census (pickups, enemies, talks, pursuits, intro memories);
    today they are empty, so Maximum equals Standard until they land."""
    display_name = "Check Slop"
    option_minimal = 0
    option_standard = 1
    option_maximum = 2
    default = 1


class IncludeGroup(Choice):
    """Preset follows Check Slop; On or Off overrides it."""
    option_preset = 0
    option_on = 1
    option_off = 2
    default = 0


class IncludeCards(IncludeGroup):
    """A check for each of the 50 collector's cards; the cards themselves become items."""
    display_name = "Cards"


class IncludeCardSets(IncludeGroup):
    """A check for each of the 10 completed card sets; the six Endurance rewards become items."""
    display_name = "Card Sets"


class IncludeFolio(IncludeGroup):
    """A check for each of the 16 Folio Bruti pages; the pages become items."""
    display_name = "Folio Bruti"


class IncludeLostAndFound(IncludeGroup):
    """A check for each of the 10 Lost and Found items; the items become items."""
    display_name = "Lost and Found"


class IncludeStatues(IncludeGroup):
    """A check for each of the 9 statues, and for every 3 statues of Hermione's homework."""
    display_name = "Statues"


class IncludePotions(IncludeGroup):
    """A check for each of the 8 potion ingredients and each of the 3 potions mixed."""
    display_name = "Potions"


class IncludeMinigames(IncludeGroup):
    """A check for each of the 8 bonus activities."""
    display_name = "Bonus Activities"


class IncludeMaraudersChallenge(IncludeGroup):
    """A check for the Marauder's Challenge in each of the 10 chapters that have one."""
    display_name = "Marauder's Challenges"


class IncludeFredAndGeorge(IncludeGroup):
    """Checks along Fred and George's four challenge tracks (statues, dungbombs, snot slinging, confiscated items)."""
    display_name = "Fred and George"


class IncludeShop(IncludeGroup):
    """A check for each one-time purchase in Fred and George's shop, including the Snufflifors and Mucus Projectum
    spellbooks."""
    display_name = "Shop"


class IncludeSecrets(IncludeGroup):
    """A check for each of the 7 named secret places."""
    display_name = "Secrets"


class IncludeChallengeShields(IncludeGroup):
    """Challenge Shields (census open: no checks yet)."""
    display_name = "Challenge Shields"


class IncludeHousePoints(IncludeGroup):
    """A check every 50 Gryffindor house points, up to 500."""
    display_name = "House Points"


class IncludeBeanMilestones(IncludeGroup):
    """A check at bean totals from 50 to 2000."""
    display_name = "Bean Milestones"


class IncludePickups(IncludeGroup):
    """World pickups (census open: no checks yet)."""
    display_name = "Pickups"


class IncludeLoosePickups(IncludeGroup):
    """Loose pickups found by play (census open: no checks yet)."""
    display_name = "Loose Pickups"


class IncludeEnemies(IncludeGroup):
    """Enemies (census open: no checks yet)."""
    display_name = "Enemies"


class IncludeTalks(IncludeGroup):
    """Conversations (Maximum only; census open: no checks yet)."""
    display_name = "Talks"


class IncludePursuits(IncludeGroup):
    """Pursuits (Maximum only; census open: no checks yet)."""
    display_name = "Pursuits"


class IncludeIntroMemories(IncludeGroup):
    """Checks inside the Intro flashback (census open: no checks yet). No seed's logic depends on them."""
    display_name = "Intro Memories"


class Goal(Choice):
    """**Dark Tower:** fly Buckbeak to the Dark Tower on Night 5; needs Tokens Required story tokens, the Night 5
    and Dark Tower passes and Expecto Patronum.

    **House Cup:** attend the House Cup ceremony on Day 6.

    **Cards:** own the number of collector's cards set in Cards Goal (turns the Cards group on)."""
    display_name = "Goal"
    option_dark_tower = 0
    option_house_cup = 1
    option_cards = 2
    default = 0


class TokensRequired(Range):
    """Story tokens (of 11; Hogwarts Express and Night 0 are already held) needed for the Dark Tower goal."""
    display_name = "Tokens Required"
    range_start = 2
    range_end = 11
    default = 8


class CardsGoal(Range):
    """Collector's cards to own for the Cards goal (of 50)."""
    display_name = "Cards Goal"
    range_start = 10
    range_end = 50
    default = 40


class ShuffleSpells(DefaultOnToggle):
    """The nine spells after Flipendo are items in the multiworld and their spellbooks are checks. Off: all ten are
    known from the start."""
    display_name = "Shuffle Spells"


@dataclass
class HppoaOptions(PerGameCommonOptions):
    check_slop: CheckSlop
    goal: Goal
    tokens_required: TokensRequired
    cards_goal: CardsGoal
    shuffle_spells: ShuffleSpells
    include_cards: IncludeCards
    include_card_sets: IncludeCardSets
    include_folio: IncludeFolio
    include_lost_and_found: IncludeLostAndFound
    include_statues: IncludeStatues
    include_potions: IncludePotions
    include_minigames: IncludeMinigames
    include_marauders_challenge: IncludeMaraudersChallenge
    include_fred_and_george: IncludeFredAndGeorge
    include_shop: IncludeShop
    include_secrets: IncludeSecrets
    include_challenge_shields: IncludeChallengeShields
    include_house_points: IncludeHousePoints
    include_bean_milestones: IncludeBeanMilestones
    include_pickups: IncludePickups
    include_loose_pickups: IncludeLoosePickups
    include_enemies: IncludeEnemies
    include_talks: IncludeTalks
    include_pursuits: IncludePursuits
    include_intro_memories: IncludeIntroMemories


option_groups = [
    OptionGroup("Goal", [Goal, TokensRequired, CardsGoal]),
    OptionGroup("Checks", [CheckSlop, IncludeCards, IncludeCardSets, IncludeFolio, IncludeLostAndFound,
                           IncludeStatues, IncludePotions, IncludeMinigames, IncludeMaraudersChallenge,
                           IncludeFredAndGeorge, IncludeShop, IncludeSecrets, IncludeChallengeShields,
                           IncludeHousePoints, IncludeBeanMilestones]),
    OptionGroup("Maximum Checks", [IncludePickups, IncludeLoosePickups, IncludeEnemies, IncludeTalks,
                                   IncludePursuits, IncludeIntroMemories]),
    OptionGroup("Items", [ShuffleSpells]),
]
