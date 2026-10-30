// hppoa_launcher.exe: picks 4:3 or 16:9 and the graphics settings, writes them to hppoa.toml beside it and starts
// hppoa.exe with the environment it needs (StartGame). A main menu picture made from the title's textures
// (launcher_bg.png beside the exe, or built in from src/launcher/launcher_bg.png; not in the repository) is drawn
// behind the page when there is one. Dear ImGui over Direct3D 11. A disc image in game\ (game\*.iso / *.xiso, no
// game\default.xbe) is unpacked there first, with a progress page (xiso.c).
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <wincodec.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cfloat>
#include <cstring>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>

#include "../single_instance.h"
#include "xiso.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {

// ── settings: hppoa.toml, the keys the game reads (scda/app/app.cpp) ─────────────────────────────────────────────

// fps_limit's choices, as the game's F10 panel offers them (scda/app/app.h kFpsChoices): 0 the game's rate,
// 30, caps above 60, 'unlocked'
// (PoA: no caps above its 30 fps until 60 fps is decoupled, see app.h)
const char* const kFpsValues[] = {"0", "30"};
const char* const kFpsNames[] = {"Game's rate (30 fps)", "30 fps"};

struct Settings {
    bool wide = false;            // aspect '16:9', otherwise '4:3'
    int display = 0;              // 'windowed', 'borderless', 'fullscreen'
    uint32_t width = 1920, height = 1080;
    uint32_t internal_height = 0;
    uint32_t msaa = 0, aniso = 0;
    bool smaa = false, vsync = true;
    bool show = true;             // show_launcher: false starts the game without this page
    int fps = 0;                  // fps_limit: an index into kFpsValues
    int colorblind = 0;           // 'off', 'protanopia', 'deuteranopia', 'tritanopia'
    uint32_t cb_strength = 100;
    bool kbm = true;              // keyboard_mouse
    uint32_t mouse_sens = 50;
    bool vibration = true, invert_x = false, invert_y = false;
    bool controller = true;  // the in-game F10 option of the same name
    bool cutscene_skip = true;  // cutscene_skip
    bool intro_skip = true;     // intro_skip
};

const char* const kColorBlindKeys[] = {"off", "protanopia", "deuteranopia", "tritanopia"};
const char* const kColorBlindNames[] = {"Off", "Protanopia (red-weak)", "Deuteranopia (green-weak)", "Tritanopia (blue-weak)"};

const char* const kDisplayKeys[] = {"windowed", "borderless", "fullscreen"};
const char* const kDisplayNames[] = {"Windowed", "Borderless", "Fullscreen"};
constexpr uint32_t kInternalValues[] = {0, 480, 720, 960, 1080, 1440, 1920, 2160};
const char* const kInternalNames[] = {"Follow window",  "480 (Xbox, 1x)", "720 (1.5x)", "960 (2x)",
                                      "1080 (2.25x)",   "1440 (3x)",      "1920 (4x)",  "2160 (4.5x, 4K)"};
constexpr uint32_t kMsaaValues[] = {0, 2, 4, 8};
const char* const kMsaaNames[] = {"Off", "2x", "4x", "8x"};
constexpr uint32_t kAnisoValues[] = {0, 2, 4, 8, 16};
const char* const kAnisoNames[] = {"Game's", "2x", "4x", "8x", "16x"};

std::wstring ExeDir() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf, n);
    return p.substr(0, p.find_last_of(L"\\/") + 1);
}

std::string Narrow(const std::wstring& w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(size_t(std::max(n, 1) - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    return s;
}

std::string Trim(std::string s) {
    s.erase(0, s.find_first_not_of(" \t\r\n"));
    s.erase(s.find_last_not_of(" \t\r\n") + 1);
    return s;
}

// hppoa.toml as its lines, so a save keeps the comments and every key this page does not touch
std::vector<std::string> g_lines;

std::string Value(const std::string& key) {
    for (const auto& l : g_lines) {
        std::string t = Trim(l);
        if (t.empty() || t[0] == '#') continue;
        size_t eq = t.find('=');
        if (eq == std::string::npos || Trim(t.substr(0, eq)) != key) continue;
        std::string v = Trim(t.substr(eq + 1));
        if (v.size() >= 2 && (v[0] == '\'' || v[0] == '"')) v = v.substr(1, v.size() - 2);
        return v;
    }
    return {};
}

void SetValue(const std::string& key, const std::string& v) {
    for (auto& l : g_lines) {
        std::string t = Trim(l);
        if (t.empty() || t[0] == '#') continue;
        size_t eq = t.find('=');
        if (eq != std::string::npos && Trim(t.substr(0, eq)) == key) {
            l = key + " = " + v;
            return;
        }
    }
    g_lines.push_back(key + " = " + v);
}

std::string TomlPath() { return Narrow(ExeDir()) + "hppoa.toml"; }

Settings Load() {
    Settings s;
    g_lines.clear();
    std::ifstream f(TomlPath());
    for (std::string l; std::getline(f, l);) g_lines.push_back(l);
    auto u = [](const std::string& v, uint32_t d) { return v.empty() ? d : uint32_t(std::strtoul(v.c_str(), nullptr, 10)); };
    s.wide = Value("aspect") == "16:9";
    std::string dm = Value("display_mode");
    for (int i = 0; i < 3; i++)
        if (dm == kDisplayKeys[i]) s.display = i;
    s.width = u(Value("width"), s.width);
    s.height = u(Value("height"), s.height);
    s.internal_height = u(Value("internal_height"), 0);
    s.msaa = u(Value("msaa"), 0);
    s.aniso = u(Value("anisotropic_filtering"), 0);
    s.smaa = Value("smaa") == "true";
    s.vsync = Value("vsync") != "false";
    s.show = Value("show_launcher") != "false";
    s.fps = 0;
    for (int i = 0; i < int(std::size(kFpsValues)); i++) {
        std::string v = kFpsValues[i];
        if (v.front() == '\'') v = v.substr(1, v.size() - 2);
        if (Value("fps_limit") == v) s.fps = i;
    }
    std::string cb = Value("colorblind");
    for (int i = 0; i < 4; i++)
        if (cb == kColorBlindKeys[i]) s.colorblind = i;
    s.cb_strength = std::min(u(Value("colorblind_strength"), 100), 100u);
    s.kbm = Value("keyboard_mouse") != "false";
    s.mouse_sens = std::clamp(u(Value("mouse_sensitivity"), 50), 1u, 100u);
    s.vibration = Value("vibration") != "false";
    s.controller = Value("controller") != "false";
    s.cutscene_skip = Value("cutscene_skip") != "false";
    s.intro_skip = Value("intro_skip") != "false";
    s.invert_x = Value("invert_camera_x") == "true";
    s.invert_y = Value("invert_camera_y") == "true";
    return s;
}

void Save(const Settings& s) {
    auto q = [](const char* v) { return std::string("'") + v + "'"; };
    SetValue("display_mode", q(kDisplayKeys[s.display]));
    SetValue("width", std::to_string(s.width));
    SetValue("height", std::to_string(s.height));
    SetValue("aspect", q(s.wide ? "16:9" : "4:3"));
    SetValue("internal_height", std::to_string(s.internal_height));
    SetValue("msaa", std::to_string(s.msaa));
    SetValue("smaa", s.smaa ? "true" : "false");
    SetValue("anisotropic_filtering", std::to_string(s.aniso));
    SetValue("vsync", s.vsync ? "true" : "false");
    SetValue("show_launcher", s.show ? "true" : "false");
    SetValue("start_menu", "false");  // this page instead of the in-game one
    SetValue("fps_limit", kFpsValues[s.fps]);
    SetValue("colorblind", q(kColorBlindKeys[s.colorblind]));
    SetValue("colorblind_strength", std::to_string(s.cb_strength));
    SetValue("keyboard_mouse", s.kbm ? "true" : "false");
    SetValue("mouse_sensitivity", std::to_string(s.mouse_sens));
    SetValue("vibration", s.vibration ? "true" : "false");
    SetValue("controller", s.controller ? "true" : "false");
    SetValue("cutscene_skip", s.cutscene_skip ? "true" : "false");
    SetValue("intro_skip", s.intro_skip ? "true" : "false");
    SetValue("invert_camera_x", s.invert_x ? "true" : "false");
    SetValue("invert_camera_y", s.invert_y ? "true" : "false");
    std::ofstream f(TomlPath(), std::ios::trunc);
    for (const auto& l : g_lines) f << l << "\n";
}

// sizes of the chosen shape, biggest first: the monitor's own and the common ones that fit the desktop
bool Shaped(uint32_t w, uint32_t h, bool wide) {
    float r = float(w) / float(h), want = wide ? 16.0f / 9.0f : 4.0f / 3.0f;
    return r > want - 0.02f && r < want + 0.02f;
}

std::vector<std::pair<uint32_t, uint32_t>> Sizes(bool wide) {
    static const std::pair<uint32_t, uint32_t> kCommon[] = {
        {640, 480},   {800, 600},   {1024, 768},  {1280, 960},  {1440, 1080}, {1600, 1200}, {1920, 1440},
        {2048, 1536}, {2880, 2160}, {1280, 720},  {1600, 900},  {1920, 1080}, {2560, 1440}, {3200, 1800},
        {3840, 2160}};
    uint32_t dw = GetSystemMetrics(SM_CXSCREEN), dh = GetSystemMetrics(SM_CYSCREEN);
    std::vector<std::pair<uint32_t, uint32_t>> m;
    DEVMODEA dm{};
    dm.dmSize = sizeof(dm);
    for (DWORD i = 0; EnumDisplaySettingsA(nullptr, i, &dm); i++)
        if (dm.dmPelsWidth >= 640 && Shaped(dm.dmPelsWidth, dm.dmPelsHeight, wide))
            m.emplace_back(dm.dmPelsWidth, dm.dmPelsHeight);
    for (auto [w, h] : kCommon)
        if (Shaped(w, h, wide) && w <= dw && h <= dh) m.emplace_back(w, h);
    std::sort(m.begin(), m.end(), [](auto a, auto b) { return a.first * a.second > b.first * b.second; });
    m.erase(std::unique(m.begin(), m.end()), m.end());
    return m;
}

// the size kept when it has the shape, otherwise the biggest that has
void FitSize(Settings& s) {
    if (s.width && s.height && Shaped(s.width, s.height, s.wide)) return;
    auto m = Sizes(s.wide);
    if (m.empty()) return;
    s.width = m.front().first;
    s.height = m.front().second;
}

// hppoa.exe beside this one, with the environment the game expects (debug.env beside it can add to it)
bool StartGame() {
    // one started from elsewhere while this page was open: the log below would be truncated under it
    if (hppoa_GameRunning()) {
        hppoa_ReportAlreadyRunning();
        return true;
    }
    const std::pair<const wchar_t*, const wchar_t*> kEnv[] = {
        {L"RECOMP_TITLE_KEVENTS", L"1"}, {L"RECOMP_VBLANK", L"1"},       {L"RECOMP_AC97_READY", L"1"},
        {L"RECOMP_DSP_ACK", L"0x834D0810"}, {L"RECOMP_PB_EXEC", L"1"}, {L"RECOMP_FB_WINDOW", L"1"},
        {L"RECOMP_USB", L"1"},           {L"RECOMP_START_MENU", L"0"},  {L"RECOMP_ACK_SLEEP_US", L"250"},
        {L"RECOMP_HEAP_RECLAIM", L"1"},
    };
    for (auto [k, v] : kEnv)
        if (!GetEnvironmentVariableW(k, nullptr, 0)) SetEnvironmentVariableW(k, v);
    // hppoa.exe refuses to start without it (single_instance.h)
    SetEnvironmentVariableW(HPPOA_LAUNCHER_ENV, L"1");
    std::wstring dir = ExeDir(), exe = dir + L"hppoa.exe";
    std::wstring cmd = L"\"" + exe + L"\"";
    // the game's console output (its [CRASH] reports among it) to hppoa_run.log beside it: a windowed launcher has no
    // console to hand down. Only with debug_logs (F10 > Debug) on; off, the output goes nowhere and no file is made
    Load();
    bool logs = Value("debug_logs") == "true";
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE log = logs ? CreateFileW((dir + L"hppoa_run.log").c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa,
                                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)
                      : INVALID_HANDLE_VALUE;
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    if (log != INVALID_HANDLE_VALUE) {
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = si.hStdError = log;
    }
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, TRUE, 0, nullptr, dir.c_str(), &si, &pi);
    if (log != INVALID_HANDLE_VALUE) CloseHandle(log);
    if (!ok) return false;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

// ── Disc image ──────────────────────────────────────────────────────────────────────────────────────────────────────
// game\ holds a disc image rather than its files: unpacked into game\ on a thread of its own before the game can
// start. Stopped or failed, the next start carries on where it stopped (files already whole are kept).

struct Unpack {
    XisoProgress p{};
    HANDLE thread = nullptr;
    bool ok = false;
    std::wstring image, dir;
    std::string name;  // the image's file name, for the page
};
Unpack g_unpack;

DWORD WINAPI UnpackThread(void*) {
    g_unpack.ok = xiso_Extract(g_unpack.image.c_str(), g_unpack.dir.c_str(), &g_unpack.p);
    return 0;
}

bool StartUnpack() {
    WCHAR image[MAX_PATH];
    g_unpack.dir = ExeDir() + L"game";
    if (!xiso_Find(g_unpack.dir.c_str(), image)) return false;
    g_unpack.image = image;
    g_unpack.name = Narrow(g_unpack.image.substr(g_unpack.image.find_last_of(L"\\/") + 1));
    g_unpack.thread = CreateThread(nullptr, 0, UnpackThread, nullptr, 0, nullptr);
    return g_unpack.thread != nullptr;
}

void DrawUnpack() {
    ImGuiIO& io = ImGui::GetIO();
    float sc = io.FontGlobalScale;
    float pw = 560 * sc;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.97f), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    ImGui::SetNextWindowSize(ImVec2(pw, 0), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.72f);
    ImGui::Begin("##unpack", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextUnformatted("Unpacking the game files");
    ImGui::TextWrapped("From %s into the game folder. This happens once; the disc image is left as it is and can "
                       "be deleted afterwards.", g_unpack.name.c_str());
    ImGui::Spacing();
    double done = double(g_unpack.p.done), total = double(g_unpack.p.total);
    char text[64];
    std::snprintf(text, sizeof text, "%.2f of %.2f GB", done / 1e9, total / 1e9);
    ImGui::ProgressBar(total > 0 ? float(done / total) : 0.0f, ImVec2(-FLT_MIN, 28 * sc), text);
    ImGui::Spacing();
    bool cancelling = g_unpack.p.cancel != 0;
    if (cancelling) ImGui::BeginDisabled();
    if (ImGui::Button("Cancel", ImVec2(160 * sc, 36 * sc))) InterlockedExchange(&g_unpack.p.cancel, 1);
    if (cancelling) ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("The next start carries on where it stopped.");
    ImGui::End();
}

// ── Direct3D 11 ─────────────────────────────────────────────────────────────────────────────────────────────────────

ID3D11Device* g_dev = nullptr;
ID3D11DeviceContext* g_ctx = nullptr;
IDXGISwapChain* g_swap = nullptr;
ID3D11RenderTargetView* g_rtv = nullptr;
ID3D11ShaderResourceView* g_bg = nullptr;
float g_bg_aspect = 4.0f / 3.0f;

void MakeRtv() {
    ID3D11Texture2D* bb = nullptr;
    g_swap->GetBuffer(0, IID_PPV_ARGS(&bb));
    g_dev->CreateRenderTargetView(bb, nullptr, &g_rtv);
    bb->Release();
}

bool InitD3D(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL fl;
    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                                             &sd, &g_swap, &g_dev, &fl, &g_ctx)))
        return false;
    MakeRtv();
    return true;
}

// launcher_bg.png through WIC into a texture
void LoadBackground() {
    std::wstring path = ExeDir() + L"launcher_bg.png";
    IWICImagingFactory* wic = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic)))) return;
    IWICBitmapDecoder* dec = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* conv = nullptr;
    // a launcher_bg.png beside the exe, else the copy built into it (launcher.rc)
    IWICStream* stream = nullptr;
    bool from_file = GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
    if (from_file) {
        if (FAILED(wic->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &dec)))
            dec = nullptr;
    } else if (HRSRC r = FindResourceW(nullptr, MAKEINTRESOURCEW(1), MAKEINTRESOURCEW(10) /* RT_RCDATA */)) {
        HGLOBAL g = LoadResource(nullptr, r);
        void* bytes = g ? LockResource(g) : nullptr;
        if (bytes && SUCCEEDED(wic->CreateStream(&stream)) &&
            SUCCEEDED(stream->InitializeFromMemory(static_cast<BYTE*>(bytes), SizeofResource(nullptr, r))) &&
            FAILED(wic->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &dec)))
            dec = nullptr;
    }
    if (dec && SUCCEEDED(dec->GetFrame(0, &frame)) && SUCCEEDED(wic->CreateFormatConverter(&conv)) &&
        SUCCEEDED(conv->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0,
                                   WICBitmapPaletteTypeCustom))) {
        UINT w = 0, h = 0;
        conv->GetSize(&w, &h);
        std::vector<uint8_t> px(size_t(w) * h * 4);
        if (SUCCEEDED(conv->CopyPixels(nullptr, w * 4, UINT(px.size()), px.data()))) {
            D3D11_TEXTURE2D_DESC td{};
            td.Width = w;
            td.Height = h;
            td.MipLevels = td.ArraySize = 1;
            td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            td.SampleDesc.Count = 1;
            td.Usage = D3D11_USAGE_DEFAULT;
            td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA init{px.data(), w * 4, 0};
            ID3D11Texture2D* tex = nullptr;
            if (SUCCEEDED(g_dev->CreateTexture2D(&td, &init, &tex))) {
                g_dev->CreateShaderResourceView(tex, nullptr, &g_bg);
                tex->Release();
                g_bg_aspect = float(w) / float(h);
            }
        }
    }
    if (conv) conv->Release();
    if (frame) frame->Release();
    if (dec) dec->Release();
    if (stream) stream->Release();
    wic->Release();
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return 1;
    switch (msg) {
        case WM_SIZE:
            if (g_swap && wp != SIZE_MINIMIZED) {
                if (g_rtv) g_rtv->Release(), g_rtv = nullptr;
                g_swap->ResizeBuffers(0, LOWORD(lp), HIWORD(lp), DXGI_FORMAT_UNKNOWN, 0);
                MakeRtv();
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ── the page ────────────────────────────────────────────────────────────────────────────────────────────────────────

template <size_t N>
int IndexOf(const uint32_t (&values)[N], uint32_t v) {
    for (size_t i = 0; i < N; i++)
        if (values[i] == v) return int(i);
    return 0;
}

ImFont* g_title_font = nullptr;  // Georgia, for the title without a picture (WinMain)

// xboxrecomp: no picture (the builds given to others carry no game art): a dark backdrop with the title as plain
// text in a Windows font, so the page still says which game it starts
void DrawTitle() {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    float ww = io.DisplaySize.x, wh = io.DisplaySize.y;
    dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(ww, wh), IM_COL32(28, 22, 40, 255), IM_COL32(28, 22, 40, 255),
                                IM_COL32(8, 6, 12, 255), IM_COL32(8, 6, 12, 255));
    ImFont* f = g_title_font ? g_title_font : ImGui::GetFont();
    const char* lines[2] = {"Harry Potter and the", "Prisoner of Azkaban"};
    float size = std::min(wh * 0.085f, ww * 0.075f);
    float y = wh * 0.08f;
    for (const char* line : lines) {
        ImVec2 t = f->CalcTextSizeA(size, FLT_MAX, 0, line);
        ImVec2 p((ww - t.x) * 0.5f, y);
        dl->AddText(f, size, ImVec2(p.x + 2, p.y + 2), IM_COL32(0, 0, 0, 200), line);
        dl->AddText(f, size, p, IM_COL32(222, 186, 92, 255), line);
        y += size * 1.15f;
    }
    const char* sub = "Unofficial PC port";
    float ss = size * 0.38f;
    ImVec2 t = f->CalcTextSizeA(ss, FLT_MAX, 0, sub);
    dl->AddText(f, ss, ImVec2((ww - t.x) * 0.5f, y + size * 0.15f), IM_COL32(190, 180, 200, 220), sub);
}

// the art behind, covering the window
void DrawBackground() {
    ImGuiIO& io = ImGui::GetIO();
    if (!g_bg) {
        DrawTitle();
        return;
    }
    float ww = io.DisplaySize.x, wh = io.DisplaySize.y;
    float w = ww, h = ww / g_bg_aspect;
    if (h < wh) h = wh, w = wh * g_bg_aspect;
    ImVec2 p((ww - w) * 0.5f, 0);
    ImGui::GetBackgroundDrawList()->AddImage(ImTextureID(g_bg), p, ImVec2(p.x + w, p.y + h));
}

// returns true when Play is pressed (settings saved)
bool DrawPage(Settings& s, std::string& error) {
    ImGuiIO& io = ImGui::GetIO();
    float sc = io.FontGlobalScale;
    float pw = 560 * sc;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.97f), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    ImGui::SetNextWindowSize(ImVec2(pw, 0), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.72f);
    ImGui::Begin("##page", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);

    // the shape, two big choices
    float bw = (pw - ImGui::GetStyle().WindowPadding.x * 2 - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    for (int i = 0; i < 2; i++) {
        bool on = s.wide == (i == 1);
        if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.72f, 0.55f, 0.15f, 1.0f));
        if (ImGui::Button(i ? "16:9  widescreen" : "4:3  original", ImVec2(bw, 44 * sc))) {
            s.wide = i == 1;
            FitSize(s);
        }
        if (on) ImGui::PopStyleColor();
        if (!i) ImGui::SameLine();
    }
    ImGui::Spacing();
    ImGui::PushItemWidth(pw * 0.5f);

    ImGui::Combo("Display mode", &s.display, kDisplayNames, 3);
    {
        auto sizes = Sizes(s.wide);
        std::string cur = std::to_string(s.width) + " x " + std::to_string(s.height);
        if (ImGui::BeginCombo(s.display == 0 ? "Window size" : "Resolution", cur.c_str())) {
            for (auto [w, h] : sizes) {
                std::string name = std::to_string(w) + " x " + std::to_string(h);
                if (ImGui::Selectable(name.c_str(), w == s.width && h == s.height)) {
                    s.width = w;
                    s.height = h;
                }
            }
            ImGui::EndCombo();
        }
    }
    int ir = IndexOf(kInternalValues, s.internal_height);
    if (ImGui::Combo("Internal resolution", &ir, kInternalNames, int(std::size(kInternalNames))))
        s.internal_height = kInternalValues[ir];
    int ms = IndexOf(kMsaaValues, s.msaa);
    if (ImGui::Combo("MSAA", &ms, kMsaaNames, int(std::size(kMsaaNames)))) s.msaa = kMsaaValues[ms];
    int an = IndexOf(kAnisoValues, s.aniso);
    if (ImGui::Combo("Anisotropic filtering", &an, kAnisoNames, int(std::size(kAnisoNames)))) s.aniso = kAnisoValues[an];
    ImGui::Checkbox("SMAA", &s.smaa);
    ImGui::SameLine(0, 30 * sc);
    ImGui::Checkbox("Vsync", &s.vsync);
    ImGui::Combo("Frame rate", &s.fps, kFpsNames, int(std::size(kFpsNames)));
    ImGui::Combo("Colour-blind filter", &s.colorblind, kColorBlindNames, 4);
    if (s.colorblind) {
        int st = int(s.cb_strength);
        if (ImGui::SliderInt("Filter strength %", &st, 0, 100)) s.cb_strength = uint32_t(st);
    }
    ImGui::Checkbox("Keyboard and mouse", &s.kbm);
    if (s.kbm) {
        ImGui::SameLine(0, 30 * sc);
        ImGui::SetNextItemWidth(pw * 0.3f);
        int ms = int(s.mouse_sens);
        if (ImGui::SliderInt("Mouse sensitivity", &ms, 1, 100)) s.mouse_sens = uint32_t(ms);
    }
    ImGui::Checkbox("Use controller", &s.controller);
    ImGui::SameLine(0, 30 * sc);
    ImGui::Checkbox("Vibration", &s.vibration);
    ImGui::SameLine(0, 30 * sc);
    ImGui::Checkbox("Invert camera X", &s.invert_x);
    ImGui::SameLine(0, 30 * sc);
    ImGui::Checkbox("Invert camera Y", &s.invert_y);
    ImGui::TextDisabled("Key bindings, button remapping and stick settings: F10 > Controls in the game.");
    ImGui::Checkbox("Hold Start or Back to fast-forward cutscenes", &s.cutscene_skip);
    ImGui::SameLine(0, 30 * sc);
    ImGui::Checkbox("Skip the start-up logos", &s.intro_skip);
    ImGui::PopItemWidth();

    ImGui::Spacing();
    bool skip = !s.show;
    if (ImGui::Checkbox("Don't show this at the next startup", &skip)) s.show = !skip;
    if (skip) ImGui::TextDisabled("Play starts the game directly. Hold Shift while starting, or use F10 in the game, to bring this back.");
    ImGui::Spacing();
    bool play = false;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.45f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    if (ImGui::Button("Play", ImVec2(bw, 40 * sc))) play = true;
    ImGui::PopStyleColor(2);
    ImGui::SetItemDefaultFocus();
    ImGui::SameLine();
    if (ImGui::Button("Quit", ImVec2(bw, 40 * sc))) PostQuitMessage(0);
    if (!error.empty()) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", error.c_str());
    ImGui::TextDisabled("Saved to hppoa.toml. F10 in the game changes the picture settings too.");
    ImGui::End();

    if (play) Save(s);
    return play;
}

}  // namespace

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int show) {
    // xboxrecomp: a game already running: say so and show it, no settings page (its changes would not apply)
    if (hppoa_GameRunning()) {
        hppoa_ReportAlreadyRunning();
        return 0;
    }
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    // xboxrecomp: a disc image to unpack first shows its progress page even with the settings page turned off
    bool unpacking = StartUnpack();
    // the page was turned off: straight to the game with the saved settings, unless Shift is held
    bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    if (!unpacking && !Load().show && !shift && StartGame()) {
        CoUninitialize();
        return 0;
    }
    SetProcessDPIAware();
    // xboxrecomp: the title image icon (launcher.rc) on the window and taskbar
    WNDCLASSEXW wc{sizeof(wc), CS_CLASSDC, WndProc, 0, 0, inst, LoadIconW(inst, MAKEINTRESOURCEW(1)), LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)),
                   nullptr, nullptr, L"hppoa_launcher", nullptr};
    RegisterClassExW(&wc);
    // 4:3 like the art, a good part of the screen
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int ch = std::clamp(sh * 2 / 3, 600, 1200), cw = ch * 4 / 3;
    RECT rc{0, 0, cw, ch};
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&rc, style, FALSE);
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"Harry Potter and the Prisoner of Azkaban", style, CW_USEDEFAULT,
                              CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, inst, nullptr);
    if (!InitD3D(hwnd)) {
        MessageBoxW(hwnd, L"Direct3D 11 could not start.", L"Launcher", MB_ICONERROR);
        return 1;
    }
    LoadBackground();
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.FontGlobalScale = std::max(1.0f, float(ch) / 600.0f);
    // xboxrecomp: the page's own font first (the default), then Windows' Georgia for the title drawn when there is
    // no picture (DrawBackground); a Windows font, so nothing is shipped for it, and the default font stands in
    io.Fonts->AddFontDefault();
    {
        wchar_t windir[MAX_PATH];
        if (GetWindowsDirectoryW(windir, MAX_PATH)) {
            std::string font = Narrow(std::wstring(windir) + L"\\Fonts\\georgia.ttf");
            if (GetFileAttributesA(font.c_str()) != INVALID_FILE_ATTRIBUTES)
                g_title_font = io.Fonts->AddFontFromFileTTF(font.c_str(), 64.0f);
        }
    }
    ImGui::StyleColorsDark();
    ImGui::GetStyle().WindowRounding = 6;
    ImGui::GetStyle().FrameRounding = 4;
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_dev, g_ctx);

    Settings s = Load();
    FitSize(s);
    std::string error;
    bool started = false;
    for (MSG msg{}; !started;) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) goto done;
        }
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        DrawBackground();
        if (unpacking) {
            DrawUnpack();
            if (WaitForSingleObject(g_unpack.thread, 0) == WAIT_OBJECT_0) {
                CloseHandle(g_unpack.thread);
                g_unpack.thread = nullptr;
                unpacking = false;
                if (!g_unpack.ok)
                    error = Narrow(g_unpack.p.error) + " The game can't start until it is unpacked; open the launcher "
                            "again to carry on.";
                else if (!s.show && !shift && StartGame())  // the page was turned off: on to the game as before
                    started = true;
            }
        } else if (DrawPage(s, error)) {
            WCHAR image[MAX_PATH];
            if (xiso_Find((ExeDir() + L"game").c_str(), image))  // stopped part-way: not playable yet
                error = "The game files are not unpacked yet. Open the launcher again to carry on.";
            else if (StartGame()) started = true;
            else error = "hppoa.exe could not be started (it must sit beside the launcher).";
        }
        ImGui::Render();
        const float black[4] = {0, 0, 0, 1};
        g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
        g_ctx->ClearRenderTargetView(g_rtv, black);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swap->Present(1, 0);
    }
done:
    // closed while unpacking: stop it (the file being written is removed; the next start carries on)
    if (g_unpack.thread) {
        InterlockedExchange(&g_unpack.p.cancel, 1);
        WaitForSingleObject(g_unpack.thread, INFINITE);
        CloseHandle(g_unpack.thread);
    }
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (g_bg) g_bg->Release();
    if (g_rtv) g_rtv->Release();
    if (g_swap) g_swap->Release();
    if (g_ctx) g_ctx->Release();
    if (g_dev) g_dev->Release();
    DestroyWindow(hwnd);
    CoUninitialize();
    return 0;
}
