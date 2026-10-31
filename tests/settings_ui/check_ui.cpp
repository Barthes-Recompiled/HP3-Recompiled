// Headless layout regression check: uses ImGui and app rows, with host/game services stubbed.
#include "../../port/src/scda/app/menu.cpp"
#include <imgui_internal.h>
#include <cassert>
namespace app {
Settings settings;
Settings& Get() { return settings; }
void Save() {}
void ApplyDisplay() {}
void* Window() { return nullptr; }
std::recursive_mutex& UiLock() { static std::recursive_mutex m; return m; }
AspectMode BootAspect() { return AspectMode::Standard; }
void SetBootAspect(AspectMode) {}
void InputFrame() {}
void InputMessage(void*, unsigned, uintptr_t, intptr_t) {}
int KeyCaptureBinding() { return 24; }
bool TakeCapturedKey(uint32_t*) { return false; }
void CancelKeyCapture() {}
void StartKeyCapture(int) {}
const char* KeyName(uint32_t) { return "Caps Lock"; }
bool SneakOn() { return false; }
std::atomic<unsigned long long> g_pace_note{0};
}
extern "C" void xbox_FramebufferWindowSetMsgHook(int (*)(void*, unsigned, uintptr_t, intptr_t)) {}
IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM) { return 0; }
bool ImGui_ImplWin32_Init(void*) { return true; }
static int g_paused;
extern "C" int hppoa_GamePaused(void) { return g_paused; }

int main() {
    const ImVec2 sizes[] = {{640,480},{1024,768},{1280,720},{1920,1080},{3840,2160}};
    for (auto size : sizes) for (bool start : {false,true}) for (int page = 0; page < app::rows::kPages; ++page) for (bool wide : {false,true}) {
        app::settings = app::Settings{};
        app::settings.aspect = wide ? app::AspectMode::Wide : app::AspectMode::Standard;
        app::settings.background_pad = !wide;
        app::settings.controller = !wide;
        app::settings.display_mode = wide ? app::DisplayMode::Borderless : app::DisplayMode::Windowed;
        app::settings.colorblind = wide ? 2 : 0;
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = size;
        io.FontGlobalScale = std::max(1.0f, size.y / 720.0f);
        unsigned char* pixels; int w,h;
        io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
        for (int frame=0; frame<8; ++frame) {
            ImGui::NewFrame();
            if (frame) {
                auto* bar = GImGui->TabBars.GetByIndex(0);
                for (auto& tab : bar->Tabs)
                    if (!strcmp(ImGui::TabBarGetTabName(bar, &tab), app::rows::PageName(page))) bar->NextSelectedTabId = tab.ID;
            }
            app::DrawPanel(start);
            auto* panel = ImGui::FindWindowByName(start ? "###start" : "Options (F10)");
            assert(panel);
            assert(panel->Pos.x >= 0 && panel->Pos.y >= 0);
            assert(panel->Pos.x + panel->Size.x <= size.x && panel->Pos.y + panel->Size.y <= size.y);
            if (frame>=4) {
                assert(!panel->ScrollbarX && !panel->ScrollbarY);
                for (auto* child : panel->DC.ChildWindows) {
                    assert(!child->ScrollbarX);
                    assert(child->ContentSize.x <= child->InnerRect.GetWidth()+1);
                    // Expand the binding trees on the next frame.
                    child->StateStorage.SetInt(child->GetID("Key bindings"),1);
                    child->StateStorage.SetInt(child->GetID("Button remapping"),1);
                    float width = child->WorkRect.GetWidth();
                    float arrow = ImGui::GetFrameHeight();
                    float value_width = 220*io.FontGlobalScale;
                    float block = value_width + 2*(arrow+ImGui::GetStyle().ItemSpacing.x);
                    size_t n; auto* rows=app::rows::Page(page,&n);
                    for(size_t i=0;i<n;++i) if(rows[i].kind==app::rows::Kind::Value) {
                        assert(ImGui::CalcTextSize(rows[i].label).x + 8 <= width-block);
                        assert(ImGui::CalcTextSize(app::rows::Value(rows[i]).c_str()).x <= value_width);
                    }
                }
            }
            ImGui::Render();
        }
        // The footer must stay below the scrolling rows.
        auto* panel = ImGui::FindWindowByName(start ? "###start" : "Options (F10)");
        assert(panel->DC.CursorMaxPos.y <= panel->InnerRect.Max.y);
        ImGui::DestroyContext();
    }
    // the pause page and its hint: every game row's label clears the value column, every value (lit, with its
    // arrows) fits that column, and everything stays inside the window
    for (auto size : sizes) for (bool wide : {false,true}) {
        app::settings = app::Settings{};
        app::settings.aspect = wide ? app::AspectMode::Wide : app::AspectMode::Standard;
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = size;
        io.Fonts->AddFontDefault();
        app::g_page_font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\georgia.ttf", 48.0f);  // as InitUi
        unsigned char* pixels; int w,h;
        io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
        g_paused = 1;
        ImGui::NewFrame();
        auto items = app::PageItems();
        assert(!items.empty() && items.front().heading);
        float x0, aw, ah;
        app::MenuArea(&x0, &aw, &ah);
        assert(x0 >= 0 && x0 + aw <= size.x + 0.5f);
        float p0 = x0 + aw * 0.08f, p1 = x0 + aw * 0.92f, pad = aw * 0.04f, text = ah / 26.0f;
        float value_x = p0 + (p1 - p0) * 0.60f, value_w = p1 - pad - value_x;
        for (auto& it : items) {
            if (it.heading) continue;
            assert(p0 + pad * 1.4f + app::PageTextWidth(text, it.row.label) + text <= value_x);
            std::string lit = "<  " + app::rows::Value(it.row) + "  >";
            if (app::PageTextWidth(text, lit.c_str()) > value_w) printf("too wide at %.0fx%.0f: %s\n", size.x, size.y, lit.c_str());
            assert(app::PageTextWidth(text, lit.c_str()) <= value_w);
        }
        ImGui::Render();
        for (int open : {0, 1}) {
            app::g_page = open != 0;
            ImGui::NewFrame();
            app::DrawUi();
            ImGui::Render();
            for (auto* list : ImGui::GetDrawData()->CmdLists)
                for (auto& v : list->VtxBuffer) assert(v.pos.x >= -1 && v.pos.y >= -1 && v.pos.x <= size.x + 1 && v.pos.y <= size.y + 1);
        }
        app::g_page = false;
        app::g_page_font = nullptr;
        g_paused = 0;
        ImGui::DestroyContext();
    }
    puts("PASS: pause page and hint at every size, 4:3 and 16:9");
    puts("PASS: all six tabs, 4:3/16:9 settings, expanded bindings and capture, F10 and startup at 640x480, 1024x768, 1280x720, 1920x1080, 3840x2160");
}

