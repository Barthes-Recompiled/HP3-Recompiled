// The options panel on F10, after SCDA-XBERecomp's (src/app/menu.cpp): one tab per page of the settings' row table
// (rows.cpp, shared with the game's own Settings pages), each row stepped left / right, applied at once and saved to
// hppoa.toml. Alt+Enter flips windowed and borderless. The fps counter draws here too.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <cstring>
#include <windows.h>

#include <algorithm>
#include <cfloat>
#include <atomic>
#include <cstdio>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>
#include <backends/imgui_impl_win32.h>

#include "app/app.h"
#include "core/log.h"

extern "C" int hppoa_GamePaused(void);  // hle_pause.c: between APT's PauseGame and UnpauseGame
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
extern "C" void xbox_FramebufferWindowSetMsgHook(int (*fn)(void*, unsigned, uintptr_t, intptr_t));

namespace app {

namespace {

std::atomic<bool> g_open{false};
std::atomic<bool> g_start{false};  // xboxrecomp: the start menu is up, the title waits for it
std::atomic<bool> g_ui{false};  // ImGui is set up

// a setting changed: applied by the renderer next frame, kept in hppoa.toml
void Changed(bool display = false) {
    Save();
    if (display) ApplyDisplay();
}

void Section(const char* text) {
    ImGui::Spacing();
    ImGui::SeparatorText(text);
}

// xboxrecomp: a value row as SCDA's OptionRow: the label left, the value between arrows right. The row is the one nav
// stop (left / right on it step the value, held arrows keep stepping); -1 or 1 when it was stepped
int OptionRow(const char* label, const char* value) {
    const ImGuiStyle& style = ImGui::GetStyle();
    float scale = ImGui::GetIO().FontGlobalScale, arrow = ImGui::GetFrameHeight();
    float value_w = 220 * scale, block_w = value_w + 2 * (arrow + style.ItemSpacing.x);
    ImVec2 at = ImGui::GetCursorPos();
    float row_w = ImGui::GetContentRegionAvail().x;
    ImGui::PushID(label);
    int step = 0;
    ImGui::Selectable("##row", false, ImGuiSelectableFlags_AllowOverlap, ImVec2(row_w, arrow));
    if (ImGui::IsItemFocused()) {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft)) step = -1;
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight)) step = 1;
    }
    ImGui::SetCursorPos(at);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::PushItemFlag(ImGuiItemFlags_NoNav | ImGuiItemFlags_ButtonRepeat, true);
    float x = at.x + row_w - block_w;
    ImGui::SetCursorPos(ImVec2(x, at.y));
    if (ImGui::ArrowButton("##dec", ImGuiDir_Left)) step = -1;
    float pad = std::max((value_w - ImGui::CalcTextSize(value).x) * 0.5f, 0.0f);
    ImGui::SetCursorPos(ImVec2(x + arrow + style.ItemSpacing.x + pad, at.y));
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(value);
    ImGui::SetCursorPos(ImVec2(x + arrow + value_w + 2 * style.ItemSpacing.x, at.y));
    if (ImGui::ArrowButton("##inc", ImGuiDir_Right)) step = 1;
    ImGui::PopItemFlag();
    ImGui::PopID();
    return step;
}

// xboxrecomp: the Controls tab's keyboard and mouse bindings: click, then press a key or a mouse button; Escape clears
void DrawKeyBindings(Settings& s, const char* label) {
    if (!ImGui::TreeNode(label)) return;
    if (ImGui::BeginTable("##keys", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        for (uint32_t i = 0; i < kKeyBinds; i++) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(kKeyBindNames[i]);
            ImGui::TableNextColumn();
            ImGui::PushID(int(i));
            bool waiting = KeyCaptureBinding() == int(i);
            std::string text = waiting ? "Press input (Esc clears)" : KeyName(s.keys[i]);
            if (ImGui::Button(text.c_str(), ImVec2(-1, 0))) {
                if (waiting) CancelKeyCapture();
                else StartKeyCapture(int(i));
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (ImGui::Button("Default keys")) {
        Settings d;
        std::copy(std::begin(d.keys), std::end(d.keys), std::begin(s.keys));
        Changed();
    }
    ImGui::TreePop();
}

// xboxrecomp: the controller's remapping: the pad button each game button reads
void DrawPadMap(Settings& s, const char* label) {
    if (!ImGui::TreeNode(label)) return;
    ImGui::TextWrapped("The pad button each game button reads (a 360/One pad's LB is White, RB Black).");
    ImGui::PushItemWidth(160 * ImGui::GetIO().FontGlobalScale);
    for (uint32_t i = 0; i < kPadInputs; i++) {
        int m = int(std::min(s.pad_map[i], kPadInputs - 1));
        std::string name = std::string("Game ") + kPadInputNames[i];
        if (ImGui::Combo(name.c_str(), &m, kPadInputNames, int(kPadInputs))) {
            s.pad_map[i] = uint32_t(m);
            Changed();
        }
    }
    ImGui::PopItemWidth();
    if (ImGui::Button("Default buttons")) {
        for (uint32_t i = 0; i < kPadInputs; i++) s.pad_map[i] = i;
        Changed();
    }
    ImGui::TreePop();
}

// xboxrecomp: one F10 tab: its rows from the shared table (rows.cpp), as the game's own Settings pages list them
void DrawRows(Settings& s, int page) {
    size_t count = 0;
    const rows::Row* list = rows::Page(page, &count);
    for (size_t i = 0; i < count; i++) {
        const rows::Row& r = list[i];
        if (r.kind == rows::Kind::Heading) {
            Section(r.label);
            continue;
        }
        if (!rows::Shown(r)) continue;
        bool enabled = rows::Enabled(r);
        ImGui::BeginDisabled(!enabled);
        if (r.kind == rows::Kind::Keys) {
            DrawKeyBindings(s, r.label);
        } else if (r.kind == rows::Kind::PadMap) {
            DrawPadMap(s, r.label);
        } else if (int step = OptionRow(r.label, rows::Value(r).c_str()); step && enabled) {
            rows::Step(r, step);
        }
        ImGui::EndDisabled();
        if (const char* note = rows::Note(r)) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("%s", note);
            ImGui::PopStyleColor();
        }
    }
}

// start: the start menu (the aspect ratio applies at once, Start game instead of Close); otherwise F10's panel
void DrawPanel(bool start) {
    Settings& s = Get();
    ImGuiIO& io = ImGui::GetIO();
    uint32_t vk;
    if (int b = KeyCaptureBinding(); b >= 0 && TakeCapturedKey(&vk)) {
        s.keys[b] = vk;
        Changed();
    }
    // keep the rows inside the window, with room for the tabs and footer even at 640 x 480
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(680 * io.FontGlobalScale, io.DisplaySize.x - 24),
                                    io.DisplaySize.y - 24), ImGuiCond_Always);
    bool open = true;
    const char* title = start ? "Harry Potter and the Prisoner of Azkaban - Settings###start" : "Options (F10)";
    if (!ImGui::Begin(title, start ? nullptr : &open, ImGuiWindowFlags_NoCollapse |
                                                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::End();
        if (!open) g_open = false;
        return;
    }

    static int page = rows::kGraphics;  // the open tab, for Defaults
    if (ImGui::BeginTabBar("##tabs")) {
        for (int p = 0; p < rows::kPages; p++) {
            if (!ImGui::BeginTabItem(rows::PageName(p))) continue;
            page = p;
            float footer = ImGui::GetFrameHeightWithSpacing() * 3 + ImGui::GetStyle().ItemSpacing.y * 2;
            ImGui::BeginChild("##rows", ImVec2(0, -footer), ImGuiChildFlags_NavFlattened,
                              ImGuiWindowFlags_AlwaysVerticalScrollbar);
            DrawRows(s, p);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("Defaults (this page)")) rows::Defaults(page);
    ImGui::SameLine();
    if (start) {
        if (ImGui::Checkbox("Show this page at startup", &s.start_menu)) Changed();
        ImGui::Spacing();
        if (ImGui::Button("Start game", ImVec2(160 * io.FontGlobalScale, 0))) {
            SetBootAspect(s.aspect);
            Changed();
            g_start = false;
        }
        ImGui::SetItemDefaultFocus();
        ImGui::TextDisabled("Saved to hppoa.toml; F10 in game changes them too");
    } else {
        if (ImGui::Button("Close")) open = false;
        if (ImGui::Checkbox("Show the launcher at startup", &s.show_launcher)) Changed();
        ImGui::TextDisabled("Saved to hppoa.toml");
    }
    ImGui::End();
    if (!open) g_open = false;
}

void DrawFps() {
    char text[32];
    std::snprintf(text, sizeof(text), "%.0f FPS", g_fps.load());
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    float sz = ImGui::GetFontSize() * 1.25f;
    ImVec2 p(12, 8);
    dl->AddText(ImGui::GetFont(), sz, ImVec2(p.x + 1, p.y + 1), IM_COL32(0, 0, 0, 200), text);
    dl->AddText(ImGui::GetFont(), sz, p, IM_COL32(255, 255, 80, 255), text);
}

// xboxrecomp: the pause menu's PC Settings page. PoA's menus are APT movies, so instead of a row in them (HP1/HP2's
// hle_frontend.c) the pad's Back opens this page while the game is paused (hle_pause.c): the rows marked for the
// game's own pages, under their tabs' headings, drawn over the paused game inside its centred 4:3 menu area.
std::atomic<bool> g_page{false};
std::atomic<int> g_page_sel{0};
std::atomic<int> g_page_keys{0};  // arrow keys / Enter / Esc from the window thread, as pad bits (kPad*)
ImFont* g_page_font = nullptr;    // a book face for the page (Windows' Georgia), ImGui's own if it is missing

ImFont* PageFont() { return g_page_font ? g_page_font : ImGui::GetFont(); }

constexpr uint16_t kPadUp = 0x0001, kPadDown = 0x0002, kPadLeft = 0x0004, kPadRight = 0x0008, kPadBack = 0x0020;
constexpr uint16_t kPadA = 0x1000, kPadB = 0x2000;  // the analog A / B, folded into the digital bits here

struct PageItem {
    rows::Row row;
    bool heading;
};

// the page's list: each game row the panel shows, a heading before each group that has one
std::vector<PageItem> PageItems() {
    std::vector<PageItem> out;
    for (int p = 0; p < rows::kPages; p++) {
        size_t n = 0;
        const rows::Row* list = rows::Page(p, &n);
        const rows::Row* heading = nullptr;
        for (size_t i = 0; i < n; i++) {
            const rows::Row& r = list[i];
            if (r.kind == rows::Kind::Heading) {
                heading = &r;
                continue;
            }
            if (!r.game || r.kind != rows::Kind::Value || !rows::Shown(r)) continue;
            if (heading) out.push_back({*heading, true}), heading = nullptr;
            out.push_back({r, false});
        }
    }
    return out;
}

// the selectable row nearest the chosen one, moving by dir (-1 / 1) when asked
int PageMove(const std::vector<PageItem>& items, int sel, int dir) {
    int n = int(items.size());
    if (!n) return 0;
    sel = std::clamp(sel, 0, n - 1);
    for (int k = 0; k < n; k++) {
        int i = dir ? ((sel + dir * (k + 1)) % n + n) % n : (sel + k) % n;
        if (!items[size_t(i)].heading) return i;
    }
    return sel;
}

void ClosePage() {
    g_page = false;
    g_page_keys = 0;
}

// the game thread, once per pad read: buttons pressed this read (with key repeat) act on the page
void PageInput(uint16_t pressed) {
    std::vector<PageItem> items = PageItems();
    int sel = PageMove(items, g_page_sel, 0);
    if (pressed & (kPadB | kPadBack)) {
        ClosePage();
        return;
    }
    if (pressed & kPadUp) sel = PageMove(items, sel, -1);
    if (pressed & kPadDown) sel = PageMove(items, sel, 1);
    if (!items.empty()) {
        const rows::Row& r = items[size_t(sel)].row;
        if (pressed & kPadLeft) rows::Step(r, -1);
        if (pressed & kPadRight) rows::Step(r, 1);
        if ((pressed & kPadA) && rows::TwoValues(r)) rows::Step(r, 1);
    }
    g_page_sel = sel;
}

void PageText(ImDrawList* dl, float size, ImVec2 at, ImU32 colour, const char* text) {
    dl->AddText(PageFont(), size, ImVec2(at.x + size * 0.06f, at.y + size * 0.06f), IM_COL32(0, 0, 0, 170), text);
    dl->AddText(PageFont(), size, at, colour, text);
}

float PageTextWidth(float size, const char* text) {
    return PageFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, text).x;
}

// a button name in a pill, as the game's own hints show buttons; returns the width it took
float PagePill(ImDrawList* dl, float size, ImVec2 at, const char* name) {
    float w = PageTextWidth(size, name) + size * 0.9f, h = size * 1.3f;
    dl->AddRectFilled(at, ImVec2(at.x + w, at.y + h), IM_COL32(20, 16, 10, 220), h * 0.5f);
    dl->AddRect(at, ImVec2(at.x + w, at.y + h), IM_COL32(214, 190, 130, 255), h * 0.5f, 0, std::max(1.0f, size * 0.07f));
    dl->AddText(PageFont(), size, ImVec2(at.x + size * 0.45f, at.y + size * 0.15f), IM_COL32(240, 228, 196, 255),
                name);
    return w;
}

// the game's 4:3 menu area: the window's middle 4:3 (all of it in a 4:3 window)
void MenuArea(float* x0, float* w, float* h) {
    ImGuiIO& io = ImGui::GetIO();
    *h = io.DisplaySize.y;
    *w = std::min(io.DisplaySize.x, *h * 4.0f / 3.0f);
    *x0 = (io.DisplaySize.x - *w) * 0.5f;
}

void DrawPausePage() {
    std::vector<PageItem> items = PageItems();
    int sel = PageMove(items, g_page_sel, 0);
    float x0, aw, ah;
    MenuArea(&x0, &aw, &ah);
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    // the whole window dimmed, the page a parchment-dark panel inside the 4:3 area
    dl->AddRectFilled(ImVec2(0, 0), ImGui::GetIO().DisplaySize, IM_COL32(0, 0, 0, 120));
    ImVec2 p0(x0 + aw * 0.08f, ah * 0.07f), p1(x0 + aw * 0.92f, ah * 0.93f);
    float r = ah * 0.012f;
    dl->AddRectFilled(p0, p1, IM_COL32(28, 22, 14, 236), r);
    dl->AddRect(p0, p1, IM_COL32(184, 152, 88, 255), r, 0, std::max(1.0f, ah / 360.0f));
    float title = ah / 16.0f, text = ah / 26.0f, small = ah / 34.0f, pad = aw * 0.04f;
    const char* name = "PC Settings";
    PageText(dl, title, ImVec2((p0.x + p1.x - PageTextWidth(title, name)) * 0.5f, p0.y + ah * 0.025f),
             IM_COL32(236, 214, 156, 255), name);
    float top = p0.y + ah * 0.025f + title * 1.35f;
    // the description and the hint take the bottom; the rows scroll to keep the chosen one in view
    float bottom = p1.y - small * 5.0f;
    float step = text * 1.32f;
    int visible = std::max(1, int((bottom - top) / step));
    int first = 0;
    if (int(items.size()) > visible) first = std::clamp(sel - visible / 2, 0, int(items.size()) - visible);
    float value_x = p0.x + (p1.x - p0.x) * 0.60f, value_w = p1.x - pad - value_x;
    for (int i = first; i < int(items.size()) && i < first + visible; i++) {
        const PageItem& it = items[size_t(i)];
        float y = top + float(i - first) * step;
        if (it.heading) {
            PageText(dl, small, ImVec2(p0.x + pad, y + (text - small)), IM_COL32(184, 152, 88, 255), it.row.label);
            continue;
        }
        bool lit = i == sel, enabled = rows::Enabled(it.row);
        if (lit)
            dl->AddRectFilled(ImVec2(p0.x + pad * 0.5f, y - text * 0.12f), ImVec2(p1.x - pad * 0.5f, y + text * 1.18f),
                              IM_COL32(120, 92, 40, 150), r);
        ImU32 c = !enabled ? IM_COL32(140, 128, 108, 255) : lit ? IM_COL32(255, 244, 214, 255) : IM_COL32(222, 204, 168, 255);
        PageText(dl, text, ImVec2(p0.x + pad * 1.4f, y), c, it.row.label);
        std::string v = rows::Value(it.row);
        std::string shown = lit && enabled ? "<  " + v + "  >" : v;
        float vw = PageTextWidth(text, shown.c_str());
        PageText(dl, text, ImVec2(value_x + std::max(0.0f, (value_w - vw) * 0.5f), y), c, shown.c_str());
    }
    if (first > 0) PageText(dl, small, ImVec2(p1.x - pad, top - small * 1.1f), IM_COL32(184, 152, 88, 255), "^");
    if (first + visible < int(items.size()))
        PageText(dl, small, ImVec2(p1.x - pad, bottom - small * 0.6f), IM_COL32(184, 152, 88, 255), "v");
    // the chosen row's description, wrapped, then the hint
    if (!items.empty()) {
        if (const char* note = rows::Note(items[size_t(sel)].row))
            dl->AddText(PageFont(), small, ImVec2(p0.x + pad, bottom + small * 0.3f), IM_COL32(200, 186, 156, 255),
                        note, nullptr, p1.x - p0.x - 2 * pad);
    }
    float hy = p1.y - small * 1.8f, hx = p0.x + pad;
    hx += PagePill(dl, small, ImVec2(hx, hy), "Left / Right") + small * 0.4f;
    PageText(dl, small, ImVec2(hx, hy + small * 0.15f), IM_COL32(236, 224, 196, 255), "Change");
    hx += PageTextWidth(small, "Change") + small * 1.4f;
    hx += PagePill(dl, small, ImVec2(hx, hy), "B") + small * 0.4f;
    PageText(dl, small, ImVec2(hx, hy + small * 0.15f), IM_COL32(236, 224, 196, 255), "Back");
}

// paused, page closed: where to find the page, under the pause menu's own rows
void DrawPauseHint() {
    float x0, aw, ah;
    MenuArea(&x0, &aw, &ah);
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    float small = ah / 34.0f;
    ImVec2 at(x0 + aw * 0.05f, ah * 0.93f - small * 1.3f);
    float w = PagePill(dl, small, at, "Back") + small * 0.4f;
    PageText(dl, small, ImVec2(at.x + w, at.y + small * 0.15f), IM_COL32(236, 224, 196, 255), "PC Settings");
}

// the window thread's messages: the pointer hidden, F10 and Alt+Enter, then ImGui; while the panel is open it keeps the
// keyboard and mouse
int MsgHook(void* hwnd, unsigned msg, uintptr_t wp, intptr_t lp) {
    InputMessage(hwnd, msg, wp, lp);  // xboxrecomp: keyboard and mouse as controller 1 (input.cpp)
    // xboxrecomp: the pause page takes the arrows, Enter and Esc (handed to the game thread's PageInput)
    if (g_page && !g_open && msg == WM_KEYDOWN) {
        int bit = wp == VK_UP ? kPadUp : wp == VK_DOWN ? kPadDown : wp == VK_LEFT ? kPadLeft : wp == VK_RIGHT ? kPadRight
                : wp == VK_RETURN ? kPadA : wp == VK_ESCAPE || wp == VK_BACK ? kPadB : 0;
        if (bit) {
            g_page_keys |= bit;
            return 1;
        }
    }
    // xboxrecomp: no mouse pointer over the game; the panel shows it again
    if (msg == WM_SETCURSOR && LOWORD(lp) == HTCLIENT && !g_open && !g_start) {
        SetCursor(nullptr);
        return 1;
    }
    bool down = msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN;
    if (down && wp == VK_F10) {
        if (!(lp & (1 << 30)) && !g_start) ToggleMenu();  // not on auto-repeat; the start menu is already up
        return 1;
    }
    if (msg == WM_SYSKEYDOWN && wp == VK_RETURN && (lp & (1 << 29))) {
        if (!(lp & (1 << 30))) {
            Settings& s = Get();
            s.display_mode = s.display_mode == DisplayMode::Windowed ? DisplayMode::Borderless : DisplayMode::Windowed;
            Changed(true);
        }
        return 1;
    }
    if (!g_ui) return 0;
    std::lock_guard lock(UiLock());
    ImGui_ImplWin32_WndProcHandler(static_cast<HWND>(hwnd), msg, WPARAM(wp), LPARAM(lp));
    if (!g_open && !g_start && !g_page) return 0;
    switch (msg) {
        case WM_KEYDOWN: case WM_CHAR: case WM_MOUSEWHEEL: case WM_LBUTTONDOWN: case WM_RBUTTONDOWN:
            return 1;
        case WM_SYSKEYDOWN:
            return wp != VK_F4;  // Alt+F4 still closes (fb_present.c ends the process)
        default:
            return 0;
    }
}

}  // namespace

bool MenuOpen() { return g_open || g_start || g_page; }

void ToggleMenu() { g_open = !g_open; }

void OpenStartMenu() { g_start = true; }

bool StartMenuOpen() { return g_start; }

void InitUi() {
    std::lock_guard lock(UiLock());
    if (g_ui) return;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad |
                      ImGuiConfigFlags_NoMouseCursorChange;
    // xboxrecomp: ImGui's own font stays F10's; the pause page draws in a book face, rasterised large so it stays sharp
    io.Fonts->AddFontDefault();
    char fonts[MAX_PATH];
    if (GetWindowsDirectoryA(fonts, MAX_PATH)) {
        std::string georgia = std::string(fonts) + "\\Fonts\\georgia.ttf";
        if (GetFileAttributesA(georgia.c_str()) != INVALID_FILE_ATTRIBUTES)
            g_page_font = io.Fonts->AddFontFromFileTTF(georgia.c_str(), 48.0f);
    }
    ImGui::StyleColorsDark();
    ImGui::GetStyle().WindowRounding = 4;
    ImGui_ImplWin32_Init(Window());
    g_ui = true;
    xbox_FramebufferWindowSetMsgHook(MsgHook);
    LOGI("options: F10 opens the panel, Alt+Enter flips windowed / borderless");
}

void DrawUi() {
    InputFrame();
    ImGuiIO& io = ImGui::GetIO();
    // ImGui's 13 px font at 720 lines, bigger with the window
    io.FontGlobalScale = std::max(1.0f, io.DisplaySize.y / 720.0f);
    io.MouseDrawCursor = false;
    if (g_start) DrawPanel(true);
    else if (g_open) DrawPanel(false);
    else if (g_page) DrawPausePage();
    else if (hppoa_GamePaused()) DrawPauseHint();
    if (Get().fps_counter) DrawFps();
    // xboxrecomp: the keys' pace a moment after the mouse wheel steps it, so a turn of the wheel is seen to do something
    if (GetTickCount64() - g_pace_note.load() < 2000 && !SneakOn()) {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        float sz = ImGui::GetFontSize() * 1.25f;
        ImVec2 p(12, Get().fps_counter ? 8 + sz * 1.3f : 8);
        char text[32];
        std::snprintf(text, sizeof text, "Keys: %s", kMoveModeNames[std::min<uint32_t>(Get().move_mode, kMoveModes - 1)]);
        dl->AddText(ImGui::GetFont(), sz, ImVec2(p.x + 1, p.y + 1), IM_COL32(0, 0, 0, 200), text);
        dl->AddText(ImGui::GetFont(), sz, p, IM_COL32(255, 255, 255, 255), text);
    }
    if (SneakOn()) {  // xboxrecomp: the sneak toggle is on: a small mark, so it is not left on unknowingly
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        float sz = ImGui::GetFontSize() * 1.25f;
        ImVec2 p(12, Get().fps_counter ? 8 + sz * 1.3f : 8);
        dl->AddText(ImGui::GetFont(), sz, ImVec2(p.x + 1, p.y + 1), IM_COL32(0, 0, 0, 200), "Sneaking");
        dl->AddText(ImGui::GetFont(), sz, p, IM_COL32(170, 200, 255, 255), "Sneaking");
    }
}

}  // namespace app

// hle_input.c: the panel has the controller while it is open
extern "C" int hppoa_MenuOpen(void) { return app::MenuOpen(); }

// hle_input.c, port 0, after remapping, keyboard and mouse and the trainer: Back while paused opens the PC Settings
// page, which then reads the pad here. A press that opens or closes the page, and anything held across it, stays
// from the game until it is let go. (RECOMP_HLE_XINPUT=0 reads the pad elsewhere and has no page.)
extern "C" void hppoa_PausePad(uint16_t* buttons, uint8_t* analog, int16_t* thumbs) {
    using namespace app;
    static uint16_t prev;
    static bool swallow;
    static ULONGLONG held_since, last_repeat;
    uint16_t b = uint16_t(*buttons & 0x0FFF);
    if (analog[0] > 30) b |= kPadA;
    if (analog[1] > 30) b |= kPadB;
    if (thumbs[1] > 16000) b |= kPadUp;
    if (thumbs[1] < -16000) b |= kPadDown;
    if (thumbs[0] < -16000) b |= kPadLeft;
    if (thumbs[0] > 16000) b |= kPadRight;
    uint16_t pressed = uint16_t(b & ~prev);
    ULONGLONG now = GetTickCount64();
    constexpr uint16_t kRepeat = kPadUp | kPadDown | kPadLeft | kPadRight;
    if (pressed & kRepeat) held_since = last_repeat = now;
    else if ((b & kRepeat) && now - held_since > 400 && now - last_repeat > 90) {
        pressed |= uint16_t(b & kRepeat);
        last_repeat = now;
    }
    prev = b;
    if (g_page && !hppoa_GamePaused()) ClosePage();
    if (!g_page) {
        if (hppoa_GamePaused() && (pressed & kPadBack) && !g_open && !g_start) {
            g_page = true;
            swallow = true;
            std::fprintf(stderr, "[PAGE] PC Settings opened\n");
        }
    } else {
        pressed |= uint16_t(g_page_keys.exchange(0));
        PageInput(pressed);
        if (!g_page) {
            swallow = true;
            std::fprintf(stderr, "[PAGE] PC Settings closed\n");
        }
    }
    if (swallow && !b) swallow = false;
    if (g_page || swallow) {
        *buttons = 0;
        std::memset(analog, 0, 8);
        std::memset(thumbs, 0, 4 * sizeof(int16_t));
    }
}
