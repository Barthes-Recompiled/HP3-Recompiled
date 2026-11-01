// Host test of the Archipelago bridge without the game (src/ap/bridge.cpp + client.cpp against a local AP server),
// ported from HP2's. Build and run with port/ap/test_bridge.sh.
//   test_bridge <phase>   with RECOMP_AP_SERVER / RECOMP_AP_SLOT set, run from a folder with saves\ beside the exe
// phase 1: a check before connecting (kept pending), connect, two checks by key, take every received item ("give"
//          it), report the counts; the seeded chapter ends are checked on binding
// phase 2: a new run of the same slot: the session must come back (checks made, items given) and give nothing twice
// fence:  built with a catalog whose version differs from the seed's: the slot must be refused (exit 0 if it was)
#include "bridge.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

static int pump(int seconds, int* given) {
  auto end = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
  while (std::chrono::steady_clock::now() < end) {
    hppoa_ap_frame();
    const char *name, *kind;
    int value;
    while (hppoa_ap_next_item(&name, &kind, &value)) {
      std::printf("give %s (%s %d)\n", name, kind, value);
      hppoa_ap_item_given();
      ++*given;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }
  return hppoa_ap_active();
}

int main(int argc, char** argv) {
  std::string phase = argc > 1 ? argv[1] : "1";
  int given = 0;
  if (!hppoa_ap_enabled()) {
    std::printf("RECOMP_AP_SERVER / RECOMP_AP_SLOT not set\n");
    return 2;
  }
  if (phase == "fence") {
    int bound = pump(5, &given);
    std::printf("fence: bound %d (0 = refused, as it must be)\n", bound);
    return bound ? 1 : 0;
  }
  if (phase == "1") hppoa_ap_location("task:day1:dada");  // before connecting: kept as pending
  int bound = pump(4, &given);
  std::printf("phase %s: bound %d, items given this run %d\n", phase.c_str(), bound, given);
  if (phase == "1") {
    hppoa_ap_location("task:day1:potions");
    hppoa_ap_location("card:1");  // in a Minimal slot this group is off: ignored
    pump(3, &given);
    std::printf("phase 1: after checks, items given this run %d\n", given);
  }
  std::printf("known card:1 %d, has Day 1 Pass %d, has Flipendo %d, has Glacius %d\n", hppoa_ap_known("card:1"),
              hppoa_ap_has("Day 1 Pass"), hppoa_ap_has("Flipendo"), hppoa_ap_has("Glacius"));
  for (int i = 0;; i++) {
    const char *name, *region;
    int chapter, unlocked;
    if (!hppoa_ap_travel_entry(i, &name, &region, &chapter, &unlocked)) break;
    if (unlocked) std::printf("travel: %s (%s, chapter %d)\n", name, region, chapter);
  }
  return bound ? 0 : 1;
}
