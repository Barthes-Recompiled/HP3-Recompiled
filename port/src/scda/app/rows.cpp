// xboxrecomp: every setting as one table of rows, after SCDA-XBERecomp's app::rows (src/app/menu.cpp, MIT,
// LICENSE.SCDA). The F10 panel (menu.cpp) draws each page's rows as a label with its value stepped left / right, and the
// game's own Settings pages (hle_frontend.c, through app_PageOption*) list the rows marked for them, so both show the
// same names and values and step them the same way. See app.h.
#include "app/app.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace app {

namespace {

// a row's setting
enum Id {
    kDisplayMode, kResolution, kAspect, kFps, kVsync, kInternal, kRenderScale, kMaxHeight, kMsaa, kSmaa, kAniso,
    kBrightness, kContrast, kFpsCounter, kSpeakers, kKbm, kMouseSensitivity,
    kMouseInvertY, kMoveMode, kPaceSneak, kPaceWalk, kController, kBackgroundPad, kDeadzoneLeft, kSensitivityLeft,
    kDeadzoneRight, kCameraSpeed, kInvertX, kInvertY, kSneakPad, kVibration, kVibrationStrength, kCutsceneSkip, kIntroSkip,
    kCutsceneBars, kColorBlind, kColorBlindStrength, kDebugLogs, kDebugMenu, kIds
};

// how a row's value reads and steps: an on / off flag, a list of choices, or a range; the display mode, aspect ratio
// and resolution are worked out in Value / Step
struct Def {
    bool Settings::*flag = nullptr;
    uint32_t Settings::*value = nullptr;
    const uint32_t* choices = nullptr;
    const char* const* names = nullptr;
    int count = 0;
    uint32_t lo = 0, hi = 0, step = 0;
    const char* unit = "";
    const char* zero = nullptr;  // a range's 0 by name
};

constexpr uint32_t kMsaa_[] = {0, 2, 4, 8};
const char* const kMsaaNames[] = {"Off", "2x", "4x", "8x"};
constexpr uint32_t kAniso_[] = {0, 2, 4, 8, 16};
const char* const kAnisoNames[] = {"Game's", "2x", "4x", "8x", "16x"};
// internal resolution: render lines, 0 follows the window
constexpr uint32_t kInternal_[] = {0, 480, 720, 960, 1080, 1440, 1920, 2160};
const char* const kInternalNames[] = {"Follow window", "480 (Xbox, 1x)", "720 (1.5x)", "960 (2x)",
                                      "1080 (2.25x)",  "1440 (3x)",      "1920 (4x)",  "2160 (4.5x, 4K)"};
constexpr uint32_t kSpeakers_[] = {2, 6};
const char* const kSpeakerNames[] = {"Stereo", "5.1 surround"};
constexpr uint32_t kColorBlind_[] = {kCbOff, kCbProtanopia, kCbDeuteranopia, kCbTritanopia};
const char* const kColorBlindNames[] = {"Off", "Protanopia (red-weak)", "Deuteranopia (green-weak)",
                                        "Tritanopia (blue-weak)"};
constexpr uint32_t kMove_[] = {kMoveSneak, kMoveWalk, kMoveRun};
// the sneak toggle's pad input: none (kPadInputs) first
constexpr uint32_t kSneakPad_[] = {kPadInputs, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
const char* const kSneakPadNames[] = {"None", "A",     "B",    "X",    "Y",      "Black",  "White", "LT", "RT",
                                      "Up",   "Down", "Left", "Right", "Start", "Back", "LThumb", "RThumb"};
const char* const kDisplayNames[] = {"Windowed", "Borderless", "Fullscreen"};
const char* const kAspectNames[] = {"4:3", "16:9", "Stretch"};

template <size_t N>
constexpr Def List(uint32_t Settings::*value, const uint32_t (&choices)[N], const char* const* names) {
    return {.value = value, .choices = choices, .names = names, .count = int(N)};
}
constexpr Def Range(uint32_t Settings::*value, uint32_t lo, uint32_t hi, uint32_t step, const char* unit = "",
                    const char* zero = nullptr) {
    return {.value = value, .lo = lo, .hi = hi, .step = step, .unit = unit, .zero = zero};
}
constexpr Def Flag(bool Settings::*flag) { return {.flag = flag}; }

using S = Settings;
const Def kDefs[kIds] = {
    /* kDisplayMode */ {},
    /* kResolution */ {},
    /* kAspect */ {},
    /* kFps */ {.value = &S::fps_limit, .choices = kFpsChoices, .names = kFpsChoiceNames, .count = int(kFpsChoiceCount)},
    /* kVsync */ Flag(&S::vsync),
    /* kInternal */ List(&S::internal_height, kInternal_, kInternalNames),
    /* kRenderScale */ Range(&S::render_percent, 25, 200, 5, "%"),
    /* kMaxHeight */ Range(&S::render_max_height, 0, 4320, 120, " lines", "No limit"),
    /* kMsaa */ List(&S::msaa, kMsaa_, kMsaaNames),
    /* kSmaa */ Flag(&S::smaa),
    /* kAniso */ List(&S::aniso, kAniso_, kAnisoNames),
    /* kBrightness */ Range(&S::brightness, 0, 100, 5),
    /* kContrast */ Range(&S::contrast, 0, 100, 5),
    /* kFpsCounter */ Flag(&S::fps_counter),
    /* kSpeakers */ List(&S::speakers, kSpeakers_, kSpeakerNames),
    /* kKbm */ Flag(&S::kbm),
    /* kMouseSensitivity */ Range(&S::mouse_sensitivity, 1, 100, 5),
    /* kMouseInvertY */ Flag(&S::mouse_invert_y),
    /* kMoveMode */ List(&S::move_mode, kMove_, kMoveModeNames),
    /* kPaceSneak */ Range(&S::pace_sneak, 5, 100, 5, "%"),
    /* kPaceWalk */ Range(&S::pace_walk, 5, 100, 5, "%"),
    /* kController */ Flag(&S::controller),
    /* kBackgroundPad */ Flag(&S::background_pad),
    /* kDeadzoneLeft */ Range(&S::deadzone_left, 0, 50, 5, "%"),
    /* kSensitivityLeft */ Range(&S::sensitivity_left, 25, 200, 25, "%"),
    /* kDeadzoneRight */ Range(&S::deadzone_right, 0, 50, 5, "%"),
    /* kCameraSpeed */ Range(&S::sensitivity_right, 25, 200, 25, "%"),
    /* kInvertX */ Flag(&S::invert_x),
    /* kInvertY */ Flag(&S::invert_y),
    /* kSneakPad */ List(&S::sneak_pad, kSneakPad_, kSneakPadNames),
    /* kVibration */ Flag(&S::vibration),
    /* kVibrationStrength */ Range(&S::vibration_strength, 0, 100, 10, "%"),
    /* kCutsceneSkip */ Flag(&S::cutscene_skip),
    /* kIntroSkip */ Flag(&S::intro_skip),
    /* kCutsceneBars */ Flag(&S::cutscene_bars),
    /* kColorBlind */ List(&S::colorblind, kColorBlind_, kColorBlindNames),
    /* kColorBlindStrength */ Range(&S::colorblind_strength, 0, 100, 10, "%"),
    /* kDebugLogs */ Flag(&S::debug_logs),
    /* kDebugMenu */ Flag(&S::debug_menu),
};

using rows::Kind;
using rows::Row;
constexpr Row Head(const char* label) { return {Kind::Heading, -1, label}; }
constexpr Row Val(Id id, const char* label, bool game = false) { return {Kind::Value, id, label, game}; }

// the pages; a row marked true is on the game's own page too (hle_frontend.c), in this order
const Row kGraphicsRows[] = {
    Head("Display"),
    Val(kDisplayMode, "Display mode", true),
    Val(kResolution, "Resolution", true),
    Val(kAspect, "Aspect ratio", true),
    Val(kFps, "Frame rate", true),
    Val(kVsync, "Vsync", true),
    Head("Resolution"),
    Val(kInternal, "Internal resolution"),
    Val(kRenderScale, "Render scale", true),
    Val(kMaxHeight, "Max render height"),
    Head("Anti-aliasing and filtering"),
    Val(kMsaa, "MSAA", true),
    Val(kSmaa, "SMAA", true),
    Val(kAniso, "Anisotropic filtering", true),
    Head("Picture"),
    Val(kBrightness, "Brightness", true),
    Val(kContrast, "Contrast"),
    Val(kFpsCounter, "FPS counter", true),
};
const Row kAudioRows[] = {
    Head("Output"),
    Val(kSpeakers, "Speakers"),
};
const Row kControlsRows[] = {
    Head("Controller"),
    Val(kController, "Use controller", true),
    Val(kBackgroundPad, "Controller in background", true),
    Val(kVibration, "Vibration", true),
    Val(kVibrationStrength, "Vibration strength", true),
    Val(kInvertX, "Invert camera X", true),
    Val(kInvertY, "Invert camera Y", true),
    Val(kDeadzoneLeft, "Left stick deadzone", true),
    Val(kSensitivityLeft, "Left stick sensitivity"),
    Val(kDeadzoneRight, "Right stick deadzone", true),
    Val(kCameraSpeed, "Camera speed", true),
    Val(kSneakPad, "Sneak toggle button"),
    {Kind::PadMap, -1, "Button remapping"},
    Head("Keyboard and mouse"),
    Val(kKbm, "Keyboard and mouse", true),
    Val(kMouseSensitivity, "Mouse sensitivity", true),
    Val(kMouseInvertY, "Invert mouse Y", true),
    Val(kMoveMode, "Movement keys pace", true),
    Val(kPaceSneak, "Sneak speed"),
    Val(kPaceWalk, "Walk speed"),
    {Kind::Keys, -1, "Key bindings"},
};
const Row kGameplayRows[] = {
    Head("Cutscenes"),
    Val(kCutsceneSkip, "Cutscene fast-forward", true),
    Val(kIntroSkip, "Skip start-up logos", true),
    Val(kCutsceneBars, "Cutscene bars (16:9)", true),
};
const Row kAccessibilityRows[] = {
    Head("Colour blindness"),
    Val(kColorBlind, "Correction filter"),
    Val(kColorBlindStrength, "Strength"),
};
const Row kDebugRows[] = {
    Head("Logs"),
    Val(kDebugLogs, "Write debug logs"),
    Head("Developer menu"),
    Val(kDebugMenu, "Allow the debug menu"),
};

struct PageDef {
    const char* name;
    const Row* rows;
    size_t count;
};
template <size_t N>
constexpr PageDef P(const char* name, const Row (&r)[N]) { return {name, r, N}; }
const PageDef kPageDefs[rows::kPages] = {
    P("Graphics", kGraphicsRows), P("Audio", kAudioRows), P("Controls", kControlsRows),
    P("Gameplay", kGameplayRows), P("Accessibility", kAccessibilityRows),
    P("Debug", kDebugRows),
};

// xboxrecomp: the sizes for an aspect ratio, biggest first: the monitor's own modes of that shape and the common ones
// that fit the desktop; stretch takes every mode
bool Shaped(uint32_t w, uint32_t h, AspectMode a) {
    if (a == AspectMode::Stretch) return true;
    float r = float(w) / float(h), want = a == AspectMode::Wide ? 16.0f / 9.0f : 4.0f / 3.0f;
    return r > want - 0.02f && r < want + 0.02f;
}

std::vector<std::pair<uint32_t, uint32_t>> ModesFor(AspectMode a) {
    static const std::vector<std::pair<uint32_t, uint32_t>> monitor = [] {
        std::vector<std::pair<uint32_t, uint32_t>> m;
        DEVMODEW dm{};
        dm.dmSize = sizeof dm;
        for (DWORD i = 0; EnumDisplaySettingsW(nullptr, i, &dm); i++)
            if (dm.dmPelsWidth >= 640 && dm.dmPelsHeight >= 480) m.emplace_back(dm.dmPelsWidth, dm.dmPelsHeight);
        return m;
    }();
    static const std::pair<uint32_t, uint32_t> kCommon[] = {
        {640, 480},   {800, 600},   {1024, 768},  {1280, 960},  {1440, 1080}, {1600, 1200}, {1920, 1440},
        {2048, 1536}, {2880, 2160}, {1280, 720},  {1600, 900},  {1920, 1080}, {2560, 1440}, {3200, 1800},
        {3840, 2160}};
    uint32_t dw = GetSystemMetrics(SM_CXSCREEN), dh = GetSystemMetrics(SM_CYSCREEN);
    std::vector<std::pair<uint32_t, uint32_t>> m;
    for (auto [w, h] : monitor)
        if (Shaped(w, h, a)) m.emplace_back(w, h);
    for (auto [w, h] : kCommon)
        if (Shaped(w, h, a) && w <= dw && h <= dh) m.emplace_back(w, h);
    std::sort(m.begin(), m.end(), [](auto x, auto y) { return x.first * x.second > y.first * y.second; });
    m.erase(std::unique(m.begin(), m.end()), m.end());
    return m;
}

int Wrap(int i, int n) { return ((i % n) + n) % n; }

// a range's next value a step away, on the step's grid (a value hppoa.toml set off it lands on it)
uint32_t StepRange(const Def& d, uint32_t v, int dir) {
    int s = int(d.step), n = dir > 0 ? (int(v) / s + 1) * s : ((int(v) + s - 1) / s - 1) * s;
    return uint32_t(std::clamp(n, int(d.lo), int(d.hi)));
}

int ChoiceIndex(const Def& d, uint32_t v) {
    for (int k = 0; k < d.count; k++)
        if (d.choices[k] == v) return k;
    return -1;
}

std::string ValueOf(const Settings& s, Id id) {
    const Def& d = kDefs[id];
    switch (id) {
        case kDisplayMode: return kDisplayNames[std::min<uint32_t>(uint32_t(s.display_mode), 2)];
        case kAspect: {
            std::string v = kAspectNames[std::min<uint32_t>(uint32_t(s.aspect), 2)];
            return s.aspect != BootAspect() && !StartMenuOpen() ? v + " (next start)" : v;
        }
        case kResolution:
            return s.width && s.height ? std::to_string(s.width) + " x " + std::to_string(s.height) : "Desktop";
        default: break;
    }
    if (d.flag) return s.*d.flag ? "On" : "Off";
    uint32_t v = s.*d.value;
    if (d.choices) {
        int k = ChoiceIndex(d, v);
        return k >= 0 ? d.names[k] : std::to_string(v);  // a value hppoa.toml set that the list lacks
    }
    if (v == 0 && d.zero) return d.zero;
    return std::to_string(v) + d.unit;
}

void StepOf(Settings& s, Id id, int dir) {
    const Def& d = kDefs[id];
    bool display = false;
    switch (id) {
        case kDisplayMode:
            s.display_mode = DisplayMode(Wrap(int(s.display_mode) + dir, 3));
            display = true;
            break;
        case kAspect:
            s.aspect = AspectMode(Wrap(int(s.aspect) + dir, 3));
            // a window or mode of the old shape: the biggest of the new one
            if (s.width && s.height && !Shaped(s.width, s.height, s.aspect)) {
                auto m = ModesFor(s.aspect);
                if (!m.empty()) s.width = m.front().first, s.height = m.front().second;
            }
            if (StartMenuOpen()) SetBootAspect(s.aspect);
            display = true;
            break;
        case kResolution: {  // "Desktop" (0 x 0) then the sizes of the aspect ratio
            auto m = ModesFor(s.aspect);
            m.insert(m.begin(), {0, 0});
            int c = 0;
            for (int k = 0; k < int(m.size()); k++)
                if (m[size_t(k)].first == s.width && m[size_t(k)].second == s.height) c = k;
            c = Wrap(c + dir, int(m.size()));
            s.width = m[size_t(c)].first, s.height = m[size_t(c)].second;
            display = true;
            break;
        }
        default:
            if (d.flag) {
                s.*d.flag = !(s.*d.flag);
            } else if (d.choices) {
                int k = ChoiceIndex(d, s.*d.value);
                s.*d.value = d.choices[Wrap(k < 0 ? 0 : k + dir, d.count)];
            } else {
                s.*d.value = StepRange(d, s.*d.value, dir);
            }
    }
    Save();
    if (display) ApplyDisplay();
}

// a row back to a new hppoa.toml's value; the aspect ratio, read at boot, stays
void DefaultOf(Settings& s, const Row& r) {
    const Settings d;
    if (r.kind == Kind::Keys) {
        std::copy(std::begin(d.keys), std::end(d.keys), std::begin(s.keys));
        return;
    }
    if (r.kind == Kind::PadMap) {
        std::copy(std::begin(d.pad_map), std::end(d.pad_map), std::begin(s.pad_map));
        return;
    }
    if (r.kind != Kind::Value) return;
    const Def& def = kDefs[r.id];
    if (r.id == kDisplayMode) s.display_mode = d.display_mode;
    else if (r.id == kResolution) s.width = d.width, s.height = d.height;
    else if (def.flag) s.*def.flag = d.*def.flag;
    else if (def.value) s.*def.value = d.*def.value;
}

}  // namespace

namespace rows {

const Row* Page(int page, size_t* count) {
    if (page < 0 || page >= kPages) return *count = 0, nullptr;
    *count = kPageDefs[page].count;
    return kPageDefs[page].rows;
}

const char* PageName(int page) { return page >= 0 && page < kPages ? kPageDefs[page].name : ""; }

std::string Value(const Row& row) {
    std::lock_guard lock(UiLock());
    return row.kind == Kind::Value ? ValueOf(Get(), Id(row.id)) : std::string();
}

void Step(const Row& row, int step) {
    std::lock_guard lock(UiLock());
    if (row.kind == Kind::Value && step && Enabled(row)) StepOf(Get(), Id(row.id), step < 0 ? -1 : 1);
}

void Defaults(int page) {
    std::lock_guard lock(UiLock());
    size_t n = 0;
    const Row* list = Page(page, &n);
    for (size_t i = 0; i < n; i++) DefaultOf(Get(), list[i]);
    Save();
    ApplyDisplay();
}

bool Shown(const Row& row) {
    std::lock_guard lock(UiLock());
    const Settings& s = Get();
    switch (row.id) {
        case kRenderScale: case kMaxHeight: return !s.internal_height;  // the window's height sets the render's
        case kCutsceneBars: return s.aspect == AspectMode::Wide;
        default: return true;
    }
}

bool Enabled(const Row& row) {
    std::lock_guard lock(UiLock());
    const Settings& s = Get();
    if (row.kind == Kind::Keys) return s.kbm;
    switch (row.id) {
        case kCutsceneSkip: case kIntroSkip: return false;
        case kVibrationStrength: return s.vibration;
        case kColorBlindStrength: return s.colorblind != kCbOff;
        default: return true;
    }
}

bool TwoValues(const Row& row) {
    return row.kind == Kind::Value && (kDefs[row.id].flag || kDefs[row.id].count == 2);
}

const char* Note(const Row& row) {
    std::lock_guard lock(UiLock());
    const Settings& s = Get();
    switch (row.id) {
        case kResolution:
            return s.display_mode == DisplayMode::Borderless
                       ? "Borderless covers the monitor; the size is for windowed / fullscreen." : nullptr;
        case kFps:
            return s.fps_limit > 30 ? "Above 30 fps the game clock follows real time. With vsync on, the monitor's rate caps it."
                                    : "Uses the game's own frame timing, capped at its console rate (30 fps on NTSC).";
        case kSpeakers:
            return "Takes effect the next time the game starts. 5.1 needs Windows' speaker setup set to 5.1\n"
                   "(or a 5.1 HDMI receiver).";
        case kController:
            return s.controller ? nullptr : "The game ignores the controller; keyboard and mouse still play.";
        case kBackgroundPad:
            return s.background_pad ? "The controller still plays while another window has the focus."
                                    : "The controller is ignored while another window has the focus.";
        case kSneakPad: return "Harry sneaks with a gentle push of the stick; the toggle holds that pace.";
        case kMoveMode: return "Mouse wheel in game:  down  Sneak  <  Walk  <  Run  up";
        case kCutsceneSkip: case kIntroSkip: return "Not working in this game yet";
        case kDebugMenu:
            return "The developers' chapter and room select, which the front end asks for on its title screen.";
        case kColorBlind: return "Shifts the colours a colour-blind player confuses toward ones they can tell apart.";
        case kDebugLogs:
            return "hppoa_run.log and xbox_kernel.log beside the game, for reporting a problem.\n"
                   "Takes effect the next time the game starts.";
        default: return nullptr;
    }
}

}  // namespace rows

}  // namespace app

// the game's own Settings pages (hle_frontend.c): 0 Gameplay, 1 Graphics, 2 Controls, each the rows of that F10 tab
// marked for the game
namespace {
const app::rows::Row* GameRow(int page, int i) {
    static const int kTab[] = {app::rows::kGameplay, app::rows::kGraphics, app::rows::kControls};
    if (page < 0 || page >= int(std::size(kTab))) return nullptr;
    size_t n = 0;
    const app::rows::Row* list = app::rows::Page(kTab[page], &n);
    for (size_t k = 0; k < n; k++)
        if (list[k].game && i-- == 0) return &list[k];
    return nullptr;
}
}  // namespace

extern "C" int app_PageOptionCount(int page) {
    int n = 0;
    while (GameRow(page, n)) n++;
    return n;
}
extern "C" const char* app_PageOptionLabel(int page, int i) {
    const app::rows::Row* r = GameRow(page, i);
    return r ? r->label : "";
}
extern "C" const char* app_PageOptionValue(int page, int i) {
    static char buf[48];
    const app::rows::Row* r = GameRow(page, i);
    std::snprintf(buf, sizeof buf, "%s", r ? app::rows::Value(*r).c_str() : "");
    return buf;
}
extern "C" void app_PageOptionStep(int page, int i, int dir) {
    if (const app::rows::Row* r = GameRow(page, i)) app::rows::Step(*r, dir);
}
