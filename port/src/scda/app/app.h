// The part of SCDA-XBERecomp's app layer (src/app/app.h) its renderer reads:
// settings, the window, the output shape, the F10 options panel. Settings live
// in hppoa.toml beside the exe as SCDA's scda.toml does (app.cpp); a RECOMP_*
// environment variable set at launch wins over its line. The window is
// xboxrecomp's framebuffer window.
#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>

namespace app {

enum class DisplayMode : uint32_t { Windowed, Borderless, Fullscreen };  // as xboxrecomp's XBOX_DISPLAY_*
enum class AspectMode : uint32_t { Standard, Wide, Stretch };            // 4:3, 16:9, stretch
constexpr uint32_t kFpsUnlocked = 1000;  // fps_limit 'unlocked' (experimental): Swap and vblank waits do not wait
// fps_limit's choices (settings, F10 panel, launcher): 0 the game's own rate (30, the console's: PoA asks for
// D3DPRESENT_INTERVAL_TWO_OR_IMMEDIATE), 30, 60 (one vblank a frame), caps above 60 (paced by a high-resolution timer,
// hle_d3d8.cpp PaceAbove), unlocked. Above 30 the game clock's minimum frame time follows the chosen rate
// (hle_video.c sub_001A7450), so the game keeps its speed.
inline constexpr uint32_t kFpsChoices[] = {0, 30, 60, 120, 144, 165, 240, kFpsUnlocked};
inline const char* const kFpsChoiceNames[] = {"Game's rate (30 fps)", "30 fps", "60 fps", "120 fps", "144 fps",
                                             "165 fps", "240 fps", "Unlocked"};
constexpr uint32_t kFpsChoiceCount = 8;
inline int FpsChoice(uint32_t v) {
    for (uint32_t i = 0; i < kFpsChoiceCount; i++)
        if (kFpsChoices[i] == v) return int(i);
    return 0;
}
enum ColorBlind : uint32_t { kCbOff, kCbProtanopia, kCbDeuteranopia, kCbTritanopia };

// xboxrecomp: the pad's inputs in one order, the analog buttons as the Xbox's bAnalogButtons (A B X Y Black White LT
// RT) then the digital ones as wButtons' bits (Up Down Left Right Start Back LThumb RThumb)
constexpr uint32_t kPadInputs = 16;
inline const char* const kPadInputNames[kPadInputs] = {"A",    "B",     "X",     "Y",     "Black", "White",
                                                       "LT",   "RT",    "Up",    "Down",  "Left",  "Right",
                                                       "Start", "Back", "LThumb", "RThumb"};
// key bindings: the 16 pad inputs, then the left stick (forward back left right), the camera (up down left right)
// and the sneak toggle (input.cpp)
constexpr uint32_t kKeyBinds = kPadInputs + 9;
constexpr uint32_t kKeySneak = kPadInputs + 8;
inline const char* const kKeyBindNames[kKeyBinds] = {
    "A", "B", "X", "Y", "Black", "White", "LT", "RT", "D-pad up", "D-pad down", "D-pad left", "D-pad right", "Start",
    "Back", "Left stick click", "Right stick click", "Move forward", "Move back", "Move left", "Move right",
    "Camera up", "Camera down", "Camera left", "Camera right", "Sneak (toggle)"};
// Space, E, left mouse, right mouse, R, Q, Shift, Ctrl, 1-4, Escape, Tab, C, middle mouse; W S A D; arrows; Caps Lock
#define kDefaultKeys                                                                                          \
    0x20, 'E', 0x01, 0x02, 'R', 'Q', 0x10, 0x11, '1', '2', '3', '4', 0x1B, 0x09, 'C', 0x04, 'W', 'S', 'A', 'D', \
        0x26, 0x28, 0x25, 0x27, 0x14

struct Settings {
    DisplayMode display_mode = DisplayMode::Windowed;  // RECOMP_DISPLAY_MODE
    uint32_t width = 1920, height = 1080;  // RECOMP_WINDOW_SIZE: the window, or the fullscreen mode; 0 the desktop's
    AspectMode aspect = AspectMode::Standard;  // RECOMP_ASPECT; read at boot, the title picks its camera then
    uint32_t internal_height = 0;      // RECOMP_INTERNAL_HEIGHT: 3D render lines, 0 follows the window
    uint32_t render_percent = 100;     // RECOMP_RENDER_PERCENT, of the window height
    uint32_t render_max_height = 1080; // RECOMP_RENDER_MAX_HEIGHT, 0 no cap
    uint32_t brightness = 50;          // 0 to 100, 50 each leaves the picture alone
    uint32_t contrast = 50;
    bool fps_counter = false;
    bool vsync = true;                 // RECOMP_VSYNC
    bool intro_skip = true;            // xboxrecomp: the start-up logo movies are left out (hle_video.c)
    bool cutscene_skip = true;         // xboxrecomp: holding Start/Back fast-forwards a cutscene to its end (hle_skip.c)
    bool cutscene_bars = true;         // xboxrecomp: 16:9's zoomed cutscenes keep the console's black bars over the picture
    bool debug_menu = false;           // xboxrecomp: the front end may open the developer debug menu (hle_debug.c)
    bool debug_logs = false;           // RECOMP_DEBUG_LOGS: hppoa_run.log (launcher) and xbox_kernel.log, from the next start
    uint32_t aniso = 0;                // RECOMP_ANISO, 0 as the game asks
    uint32_t msaa = 0;                 // RECOMP_MSAA samples, 0 off
    bool smaa = false;                 // RECOMP_SMAA, over the finished frame; combines with msaa
    uint32_t shadow_resolution = 512;  // SCDA's shadow-map path; this title has none
    // SCDA's shader model 3 effects, kept off for this title (no setting)
    uint32_t shader_model = 1;
    bool hdr_rendering = false;
    bool tone_mapping = false;
    uint32_t ambient_occlusion = 0;    // SCDA's needs a hook into the game's translucent pass; none here yet
    bool controller = true;            // xboxrecomp: false: the PC's controller is read as at rest (keyboard and mouse stay)
    bool background_pad = true;        // xboxrecomp: false: the controller is read as at rest while another window has the focus
    bool async_shaders = true;         // RECOMP_ASYNC_SHADERS=0: wait for every pipeline
    bool show_launcher = true;         // xboxrecomp: hppoa_launcher.exe shows its page; false starts the game straight away (Shift shows it)
    bool start_menu = false;           // xboxrecomp: the settings page before the game starts (RECOMP_START_MENU); hppoa_launcher.exe is the usual way
    // xboxrecomp: frame rate, accessibility and controls (input.cpp, the final pass)
    uint32_t fps_limit = 60;           // default 60 (Austin, 2026-10-30); kFpsChoices: 0 the game's own rate (30), 30: two vblanks a frame at least, 60 one, 120..240 a cap, kFpsUnlocked none
    uint32_t colorblind = 0;           // ColorBlind: correction filter over the finished frame
    uint32_t colorblind_strength = 100;  // 0 to 100 %
    uint32_t speakers = 2;             // xboxrecomp: 2 stereo, 6 5.1 surround (hle_dsound.c, at start)
    bool kbm = true;                   // keyboard and mouse drive pad 1 alongside a controller
    uint32_t mouse_sensitivity = 50;   // 1 to 100
    bool mouse_invert_y = false;
    uint32_t deadzone_left = 10, deadzone_right = 10;          // % of full tilt read as centred
    uint32_t sensitivity_left = 100, sensitivity_right = 100;  // % response past the deadzone
    bool invert_x = false, invert_y = false;                   // the camera (right stick and mouse)
    bool vibration = true;
    uint32_t vibration_strength = 100;  // %
    uint32_t pad_map[kPadInputs] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};  // game input <- pad input
    uint32_t move_mode = 2;
    uint32_t pace_sneak = 44, pace_walk = 54;  // xboxrecomp: the keys' push sneaking / walking, % of full  // xboxrecomp: the movement keys' pace: kMoveSneak, kMoveWalk, kMoveRun (the mouse wheel)
    uint32_t sneak_pad = kPadInputs;  // xboxrecomp: the pad input that toggles sneaking (then not the game's), kPadInputs none
    uint32_t keys[kKeyBinds] = {kDefaultKeys};  // VK codes (1 2 4 5 6 the mouse buttons), 0 unbound
};

Settings& Get();
void Save();
// xboxrecomp: the movement keys' paces (input.cpp tunes the keys' push to each; the mouse wheel steps them), as in
// HP:CoS, with HP:PS's own three: sneak, walk, run
enum MoveMode : uint32_t { kMoveSneak, kMoveWalk, kMoveRun, kMoveModes };
extern std::atomic<unsigned long long> g_pace_note;  // GetTickCount64 when the wheel last stepped it (menu.cpp shows it)
inline const char* const kMoveModeNames[kMoveModes] = {"Sneak", "Walk", "Run"};
// re-reads hppoa.toml when it changed on disk; the renderer calls it once a frame
void Poll();
// the window to the display settings, any thread
void ApplyDisplay();
void* Window();  // HWND of the framebuffer window
float RenderScale(uint32_t guest_height);
// the output's shape: 4:3, or the window's own for 16:9 / stretch
float Aspect();
AspectMode BootAspect();  // the aspect setting this run started with
void SetBootAspect(AspectMode a);  // xboxrecomp: the start menu's choice, before the title asks
inline float Narrow() { return 4.0f / 3.0f / Aspect(); }
inline bool Widescreen() { return Aspect() > 1.34f; }
std::filesystem::path ExeDir();

// options panel (menu.cpp), drawn by the renderer each frame with the lock held
std::recursive_mutex& UiLock();
bool MenuOpen();
void ToggleMenu();
// xboxrecomp: the start menu, shown before the title's first frame: open while it waits for Start
void OpenStartMenu();
bool StartMenuOpen();
void InitUi();  // ImGui's context and Win32 side, once the window is up
void DrawUi();
// xboxrecomp: controls (input.cpp): key names as hppoa.toml spells them, the window's keys and mouse, the pointer held
// in the window, and the menu's "press a key" for a binding (Escape clears it)
const char* KeyName(uint32_t vk);
int KeyFromName(const char* name);  // -1 not a key name
void InputMessage(void* hwnd, unsigned msg, uintptr_t wp, intptr_t lp);
void InputFrame();
void StartKeyCapture(int binding);
int KeyCaptureBinding();
bool TakeCapturedKey(uint32_t* vk);
void CancelKeyCapture();
bool SneakOn();  // xboxrecomp: the sneak toggle is on (input.cpp)
inline std::atomic<float> g_fps{0};

// xboxrecomp: every setting the F10 panel and the game's own Settings pages show, as one table of rows (rows.cpp),
// after SCDA-XBERecomp's app::rows (src/app/menu.cpp, MIT): a row is a heading, a value stepped left / right, or one of
// the Controls tab's binding lists. Each call takes UiLock, so the game thread and the panel can both use it.
namespace rows {
enum class Kind { Heading, Value, Keys, PadMap };
// F10's tabs, in order
enum Page { kGraphics, kAudio, kControls, kGameplay, kAccessibility, kDebug, kPages };
struct Row {
    Kind kind;
    int id;             // the row's setting (rows.cpp), -1 for a heading
    const char* label;
    bool game = false;  // also on the game's own Settings page
};
const Row* Page(int page, size_t* count);
const char* PageName(int page);
std::string Value(const Row& row);
void Step(const Row& row, int step);  // -1 or 1, saved to hppoa.toml at once
void Defaults(int page);              // the page's rows as a new hppoa.toml has them (the aspect ratio stays)
bool Shown(const Row& row);           // F10: false leaves it out for now (render scale with a fixed resolution)
bool Enabled(const Row& row);         // F10: false greys it (vibration strength with vibration off)
bool TwoValues(const Row& row);      // A / Enter can toggle it on the pause page
const char* Note(const Row& row);     // F10: the grey line under it, or null
}  // namespace rows

}  // namespace app
