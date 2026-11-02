"""Run the world's tests against the pinned AP checkout (zipped, as the installed generator imports it) and optionally
generate a two-player seed (one Maximum, one Minimal slot). All mutable files go to --runtime.

    python apworld/hppoa/test_world.py --ap <out/ap-0.6.8> --runtime <dir> [--generate SEED]

It first checks that data/catalog.json is a verbatim copy of port/ap/catalog.json. test_world.py itself is left out
of the package."""
import argparse
import importlib
import os
from pathlib import Path
import sys
import unittest
import zipfile

GAME = "Harry Potter and the Prisoner of Azkaban Recomp"
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--ap", type=Path, required=True)
parser.add_argument("--runtime", type=Path, required=True)
parser.add_argument("--generate", type=int, help="Generate a two-player seed after the tests")
parser.add_argument("--catalog", type=Path, help="the port's catalog (default: port/ap/catalog.json in this repo)")
args = parser.parse_args()
world = Path(__file__).resolve().parent
args.catalog = args.catalog or world.parent.parent / "port" / "ap" / "catalog.json"
if args.catalog.read_bytes() != (world / "data" / "catalog.json").read_bytes():
    raise SystemExit(f"data/catalog.json differs from {args.catalog}; copy it over")
runtime = args.runtime.resolve()
runtime.mkdir(parents=True, exist_ok=True)
worlds_dir = runtime / "worlds"
worlds_dir.mkdir(exist_ok=True)
if (worlds_dir / "hppoa").exists():
    raise RuntimeError("a loose hppoa world is in the runtime's worlds folder; choose a new runtime directory")
with zipfile.ZipFile(worlds_dir / "hppoa.apworld", "w", zipfile.ZIP_DEFLATED) as package:
    for source in sorted(world.rglob("*")):
        if source.is_file() and "__pycache__" not in source.parts and source.name != "test_world.py":
            package.write(source, Path("hppoa") / source.relative_to(world))
sys.path.insert(0, str(args.ap.resolve()))
os.environ["AP_TEST_WORLDS"] = "hppoa"
import Utils
Utils.user_path.cached_path = str(runtime)
import test  # disables dependency auto-updates and config autosaves
module = importlib.import_module("worlds.hppoa.test")
cases = [obj for obj in vars(module).values()
         if isinstance(obj, type) and issubclass(obj, unittest.TestCase) and obj.__module__ == module.__name__
         and obj.__name__ != "HppoaTestBase"]
suite = unittest.TestSuite(unittest.defaultTestLoader.loadTestsFromTestCase(cls) for cls in cases)
result = unittest.TextTestRunner(verbosity=2).run(suite)
if not result.wasSuccessful():
    raise SystemExit(1)
if args.generate is not None:
    players = runtime / "players"
    players.mkdir(exist_ok=True)
    (players / "Harry.yaml").write_text(f"name: Harry\ngame: {GAME}\n{GAME}:\n  check_slop: maximum\n",
                                        encoding="utf-8")
    (players / "Hermione.yaml").write_text(
        f"name: Hermione\ngame: {GAME}\n{GAME}:\n  check_slop: minimal\n  tokens_required: 6\n", encoding="utf-8")
    import Generate
    options = Generate.mystery_argparse([
        "--player_files_path", str(players), "--outputpath", str(runtime / "seeds"),
        "--seed", str(args.generate), "--spoiler", "2"])
    rolled, seed = Generate.main(options)
    from Main import main
    multiworld = main(rolled, seed)
    print(f"Generated two-player seed {multiworld.seed_name}; "
          f"{len(multiworld.get_locations())} locations including goal events")
