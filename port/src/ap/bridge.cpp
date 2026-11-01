// The in-exe Archipelago bridge, ported from HP2's (hp2_recomp src/ap, itself the Crackdown recomp's design).
// client.cpp's network thread only queues packets; this file runs on the title's thread (hle_video.c calls
// hppoa_ap_frame once a frame) and owns all session state:
//   - catalog.json (port/ap, made by make_catalog.py, embedded at build time) maps location keys and item names to
//     IDs; a slot whose slot_data catalog differs is refused (disconnected, logged) - the version fence of
//     docs/ap/08-catalog-spec.md §1;
//   - the slot's groups (slot_data "groups") say which checks exist: a check in a group the slot left out is ignored;
//   - the session sidecar <exe dir>\saves\archipelago\session_<seed>_<slot>.json keeps every item given (in the
//     server's order: its length is the received index) and every check made, so an item is given once and checks
//     are sent again on each connection (the server ignores repeats);
//   - checks made before a slot is connected this run go to pending_<slot>.json and join the session on connecting;
//   - received items are handed out one at a time (hppoa_ap_next_item / hppoa_ap_item_given), the session saved after
//     each, so a crash between giving and saving gives an item at most twice, never loses one;
//   - the seeded chapters' ends (Intro, Hogwarts Express, Night 0: complete at the Day 1 start) are checked as soon as
//     a slot is bound.
#include "bridge.h"
#include "client.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace hppoa::ap {
namespace {
using json = nlohmann::json;
namespace fs = std::filesystem;

const char kCatalogText[] =
#include "catalog_data.inc"
    ;

const char* const kSeededChapterEnds[] = {"chapter:intro", "chapter:hogwartsexpress", "chapter:night0"};

struct Catalog {
  std::string version, game;
  std::map<std::string, int64_t> location_by_key;
  std::map<int64_t, std::string> location_group;
  std::map<int64_t, std::string> item_name;
  std::map<std::string, json> item;  // by name
  std::set<std::string> start_items;
  json regions;
};

const Catalog& Cat() {
  static Catalog c = [] {
    Catalog c;
    json j = json::parse(kCatalogText);
    c.version = j["version"];
    c.game = j["game"];
    c.regions = j["regions"];
    for (auto& l : j["locations"]) {
      int64_t id = l["id"].get<int64_t>();
      c.location_by_key[l["key"].get<std::string>()] = id;
      c.location_group[id] = l["group"].get<std::string>();
    }
    for (auto& [name, it] : j["items"].items()) {
      c.item_name[it["id"].get<int64_t>()] = name;
      c.item[name] = it;
      if (it.value("start", false)) c.start_items.insert(name);
    }
    return c;
  }();
  return c;
}

fs::path SavesDir() {
  wchar_t exe[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
  return fs::path(std::wstring(exe, n)).parent_path() / L"saves" / L"archipelago";
}

std::string ReadPassword() {
  std::ifstream f(SavesDir() / L"connection.toml");
  std::string line;
  while (std::getline(f, line)) {
    auto eq = line.find('=');
    if (eq == std::string::npos || line.compare(0, 8, "password") != 0) continue;
    std::string v = line.substr(eq + 1);
    auto a = v.find_first_of("\"'"), b = v.find_last_of("\"'");
    if (a != std::string::npos && b > a) return v.substr(a + 1, b - a - 1);
  }
  return {};
}

struct Session {
  std::string seed, slot;
  std::vector<int64_t> given;  // every item given in the game, in the server's order
  std::set<int64_t> checked;
  fs::path path;
  void Load() {
    std::ifstream f(path);
    if (!f) return;
    try {
      json j = json::parse(f);
      for (auto& id : j.value("given", json::array())) given.push_back(id.get<int64_t>());
      for (auto& id : j.value("checked", json::array())) checked.insert(id.get<int64_t>());
    } catch (const std::exception& e) {
      std::fprintf(stderr, "[AP] session %s unreadable (%s): starting it over\n", path.string().c_str(), e.what());
    }
  }
  bool Save() const {
    fs::create_directories(path.parent_path());
    fs::path tmp = path;
    tmp += L".tmp";
    {
      std::ofstream f(tmp, std::ios::trunc);
      json j{{"format", 1}, {"seed", seed}, {"slot", slot}, {"received_index", given.size()}, {"given", given},
             {"checked", std::vector<int64_t>(checked.begin(), checked.end())}};
      f << j.dump(1);
      if (!f) return false;
    }
    return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
  }
};

struct Bridge {
  bool configured = false, started = false, connected = false;
  std::string server, slot, password;
  Client client;
  uint64_t generation = 0;
  std::string seed;
  json slot_data;
  Session session;
  bool bound = false;                 // a session is loaded (a slot connected this run)
  std::vector<int64_t> received;      // the server's whole item list this connection
  std::deque<int64_t> queued_checks;  // made before a slot connected
  bool goal_done = false, goal_sent = false;

  void Init() {
    const char* s = std::getenv("RECOMP_AP_SERVER");
    const char* n = std::getenv("RECOMP_AP_SLOT");
    configured = s && *s && n && *n;
    if (!configured) return;
    server = s;
    slot = n;
    password = ReadPassword();
    LoadPending();
    std::fprintf(stderr, "[AP] %s, slot %s, catalog %s (%zu locations), %zu checks pending\n", server.c_str(),
                 slot.c_str(), Cat().version.c_str(), Cat().location_by_key.size(), queued_checks.size());
    client.Start(server);
    started = true;
  }

  // the slot's groups decide which checks exist; before a slot is bound every catalog check is kept
  bool InSlot(int64_t id) const {
    if (!bound) return true;
    const json& groups = slot_data.value("groups", json::object());
    auto g = Cat().location_group.find(id);
    return g != Cat().location_group.end() && groups.value(g->second, false);
  }

  fs::path PendingPath() const { return SavesDir() / fs::path("pending_" + slot + ".json"); }
  void LoadPending() {
    std::ifstream f(PendingPath());
    if (!f) return;
    try {
      for (auto& id : json::parse(f)) queued_checks.push_back(id.get<int64_t>());
    } catch (...) {
    }
  }
  void SavePending() {
    fs::create_directories(PendingPath().parent_path());
    std::ofstream f(PendingPath(), std::ios::trunc);
    f << json(std::vector<int64_t>(queued_checks.begin(), queued_checks.end())).dump();
  }

  void Send(json cmds) {
    if (connected) client.Send(generation, std::move(cmds));
  }

  void SendChecks() {
    if (!connected || !bound || session.checked.empty()) return;
    Send(json::array({{{"cmd", "LocationChecks"},
                       {"locations", std::vector<int64_t>(session.checked.begin(), session.checked.end())}}}));
  }

  void SendGoal() {
    Send(json::array({{{"cmd", "StatusUpdate"}, {"status", 30}}}));
    goal_sent = connected;
  }

  void OnPacket(const NetEvent& e) {
    const json& p = e.body;
    if (e.cmd == "RoomInfo") {
      seed = p.value("seed_name", "");
      json connect{{"cmd", "Connect"}, {"game", Cat().game}, {"name", slot}, {"password", password},
                   {"uuid", "hppoa-" + slot}, {"items_handling", 7}, {"tags", json::array()}, {"slot_data", true},
                   {"version", {{"major", 0}, {"minor", 6}, {"build", 8}, {"class", "Version"}}}};
      client.Send(generation, json::array({connect}));
    } else if (e.cmd == "Connected") {
      json data = p.value("slot_data", json::object());
      std::string cat = data.value("catalog", "");
      if (cat != Cat().version) {
        std::fprintf(stderr, "[AP] slot built from catalog %s, this game has %s: not playing it\n", cat.c_str(),
                     Cat().version.c_str());
        client.RequestStop();
        return;
      }
      slot_data = std::move(data);
      connected = true;
      if (!bound || session.seed != seed) {
        session = Session{};
        session.seed = seed;
        session.slot = slot;
        session.path = SavesDir() / fs::path("session_" + seed + "_" + slot + ".json");
        session.Load();
        bound = true;
      }
      for (int64_t id : queued_checks)
        if (InSlot(id)) session.checked.insert(id);
      queued_checks.clear();
      for (const char* key : kSeededChapterEnds) {
        auto it = Cat().location_by_key.find(key);
        if (it != Cat().location_by_key.end() && InSlot(it->second)) session.checked.insert(it->second);
      }
      session.Save();
      SavePending();
      received.clear();
      std::fprintf(stderr, "[AP] connected: seed %s, %zu checks made, %zu items given\n", seed.c_str(),
                   session.checked.size(), session.given.size());
      SendChecks();
      if (goal_done && !goal_sent) SendGoal();
    } else if (e.cmd == "ConnectionRefused") {
      std::fprintf(stderr, "[AP] connection refused: %s\n", p.value("errors", json::array()).dump().c_str());
      client.RequestStop();
    } else if (e.cmd == "ReceivedItems") {
      size_t index = p.value("index", size_t(0));
      if (index == 0) received.clear();
      if (index != received.size()) {  // out of step: ask for the whole list again
        Send(json::array({{{"cmd", "Sync"}}}));
        return;
      }
      for (auto& it : p.value("items", json::array())) received.push_back(it.value("item", int64_t(0)));
    }
  }

  void EnsureInit() {
    static bool once = false;
    if (!once) {
      once = true;
      Init();
    }
  }

  void Frame() {
    EnsureInit();
    if (!started) return;
    for (NetEvent& e : client.Drain()) {
      if (e.kind == NetEvent::Kind::connected_socket) {
        generation = e.generation;
      } else if (e.kind == NetEvent::Kind::disconnected) {
        if (connected)
          std::fprintf(stderr, "[AP] disconnected: %s (checks are kept and sent later)\n", e.detail.c_str());
        connected = false;
      } else if (e.generation == generation) {
        OnPacket(e);
      }
    }
  }

  void Location(const std::string& key) {
    EnsureInit();
    if (!configured) return;
    auto it = Cat().location_by_key.find(key);
    if (it == Cat().location_by_key.end()) return;
    int64_t id = it->second;
    if (!InSlot(id)) return;
    if (!bound) {
      for (int64_t q : queued_checks)
        if (q == id) return;
      queued_checks.push_back(id);
      SavePending();
      std::fprintf(stderr, "[AP] check %s (%lld), kept until a slot is connected\n", key.c_str(), (long long)id);
      return;
    }
    if (!session.checked.insert(id).second) return;
    session.Save();
    std::fprintf(stderr, "[AP] check %s (%lld)\n", key.c_str(), (long long)id);
    if (connected) Send(json::array({{{"cmd", "LocationChecks"}, {"locations", {id}}}}));
  }
};

Bridge& B() {
  static Bridge b;
  return b;
}
std::string g_name, g_kind;  // hppoa_ap_next_item's strings, valid until the next call

struct TravelEntry {
  std::string name, region, pass, chapter_pass;
  int chapter = -1;
};
// chapters in the game's chapter-table order (the travel target's chapter index), then the areas
const std::vector<TravelEntry>& Travel() {
  static std::vector<TravelEntry> list = [] {
    std::vector<TravelEntry> out;
    std::map<std::string, std::string> pass_of;  // region -> its pass item
    for (auto& [name, it] : Cat().item)
      if (it.contains("region")) pass_of[it["region"].get<std::string>()] = name;
    auto display = [](const std::string& pass) { return pass.substr(0, pass.size() - 5); };  // drop " Pass"
    const json& chapters = Cat().regions["chapters"];
    std::map<std::string, int> index;
    for (size_t i = 0; i < chapters.size(); i++) {
      std::string region = chapters[i];
      index[region] = int(i);
      out.push_back({display(pass_of[region]), region, pass_of[region], "", int(i)});
    }
    for (auto& a : Cat().regions["areas"]) {
      std::string region = a;
      const json& ch = Cat().regions["area_chapters"][region];
      TravelEntry e{display(pass_of[region]), region, pass_of[region], "", -1};
      if (!ch.is_null()) {
        e.chapter = index[ch.get<std::string>()];
        e.chapter_pass = pass_of[ch.get<std::string>()];
      }
      out.push_back(e);
    }
    return out;
  }();
  return list;
}
}  // namespace
}  // namespace hppoa::ap

using namespace hppoa::ap;

extern "C" void hppoa_ap_frame(void) { B().Frame(); }
extern "C" int hppoa_ap_enabled(void) {
  static int on = -1;
  if (on < 0) {
    const char* s = std::getenv("RECOMP_AP_SERVER");
    const char* n = std::getenv("RECOMP_AP_SLOT");
    on = s && *s && n && *n;
  }
  return on;
}
extern "C" int hppoa_ap_known(const char* key) {
  if (!key || !hppoa_ap_enabled()) return 0;
  auto it = Cat().location_by_key.find(key);
  return it != Cat().location_by_key.end() && B().InSlot(it->second);
}
extern "C" void hppoa_ap_location(const char* key) {
  if (key && hppoa_ap_enabled()) B().Location(key);
}
extern "C" void hppoa_ap_goal(void) {
  Bridge& b = B();
  if (!hppoa_ap_enabled() || b.goal_done) return;
  b.goal_done = true;
  std::fprintf(stderr, "[AP] goal done\n");
  b.SendGoal();
}
extern "C" int hppoa_ap_next_item(const char** name, const char** kind, int* value) {
  Bridge& b = B();
  while (b.bound && b.connected && b.session.given.size() < b.received.size()) {
    int64_t id = b.received[b.session.given.size()];
    auto it = Cat().item_name.find(id);
    if (it == Cat().item_name.end()) {  // not in this catalog: counted as given, nothing to do
      b.session.given.push_back(id);
      b.session.Save();
      continue;
    }
    const json& item = Cat().item.at(it->second);
    g_name = it->second;
    g_kind = item.value("kind", "");
    *name = g_name.c_str();
    *kind = g_kind.c_str();
    *value = item.value("spell_id", 0);
    return 1;
  }
  return 0;
}
extern "C" void hppoa_ap_item_given(void) {
  Bridge& b = B();
  if (b.session.given.size() < b.received.size()) {
    int64_t id = b.received[b.session.given.size()];
    b.session.given.push_back(id);
    b.session.Save();
    auto it = Cat().item_name.find(id);
    std::fprintf(stderr, "[AP] item %s given\n", it != Cat().item_name.end() ? it->second.c_str() : "?");
  }
}
extern "C" int hppoa_ap_active(void) { return B().bound; }
extern "C" int hppoa_ap_option(const char* key, int def) {
  auto& b = B();
  if (!b.bound || !key || !b.slot_data.contains(key)) return def;
  const json& v = b.slot_data[key];
  return v.is_boolean() ? int(v.get<bool>()) : v.is_number_integer() ? v.get<int>() : def;
}
extern "C" int hppoa_ap_has(const char* item_name) {
  Bridge& b = B();
  auto it = Cat().item.find(item_name ? item_name : "");
  if (!b.bound || it == Cat().item.end()) return 0;
  if (Cat().start_items.count(it->first)) return 1;
  int64_t id = it->second.value("id", int64_t(0));
  for (int64_t g : b.session.given)
    if (g == id) return 1;
  return 0;
}
extern "C" int hppoa_ap_travel_entry(int i, const char** name, const char** region, int* chapter, int* unlocked) {
  const auto& list = Travel();
  if (i < 0 || size_t(i) >= list.size()) return 0;
  const TravelEntry& e = list[size_t(i)];
  *name = e.name.c_str();
  *region = e.region.c_str();
  *chapter = e.chapter;
  *unlocked = hppoa_ap_has(e.pass.c_str()) && (e.chapter_pass.empty() || hppoa_ap_has(e.chapter_pass.c_str()));
  return 1;
}
