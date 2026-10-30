// Direct3D 8 (XDK 5659) entry points the title calls, implemented on a host
// D3D12 renderer instead of the emulated NV2A pushbuffer.
//
// Ported from SCDA-XBERecomp's src/xdk/d3d8.cpp (MIT, Copyright (c) 2026
// TGP482, Joshhhuaaa; see ../LICENSE.SCDA), which targets XDK 5849, by way of
// the HP: Chamber of Secrets port (XDK 5028). The addresses, CDevice offsets and argument counts below are this XBE's: the
// entry points from XbSymbolDatabase, the rest read from the functions' own
// code (the comment by each constant says where).
//
// Resource creation, locking and the XG helpers stay recompiled: they only
// manage memory. Everything that would push hardware commands is replaced, and
// what still pushes (anything not caught here) writes into a scratch buffer
// that MakeSpace rewinds, so the pushbuffer executor sees nothing.
//
// Each entry point keeps the generated XDK body as sub_X_gen, so
// RECOMP_HLE_D3D=0 runs the original Direct3D on the emulated NV2A.
// Built into the recompile with --exclude-manual pointing at this file.
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "app/app.h"
#include "core/log.h"
#include "gpu/backend.h"
#include "gpu/texture.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"

extern "C" {
#include "video/video_player.h"

// the recompiled code's registers (recomp_types.h), one set per guest thread
extern __thread uint32_t g_eax, g_ecx, g_edx, g_esp;
extern __thread uint32_t g_ebx, g_esi, g_edi;
typedef void (*recomp_func_t)(void);
recomp_func_t recomp_lookup(uint32_t xbox_va);
void xbox_SetVblankHook(void (*fn)(void));  // kernel_bridge.c
uint32_t xbox_Nv2aVblankCount(void);
int hppoa_FastForward(void);  // hle_skip.c: a cutscene fast-forwarding (1 when not)
}

using namespace kernel;

namespace d3d {

namespace {

// ── This XBE's D3D globals ──────────────────────────────────────────────────

constexpr uint32_t kDevice = 0x00275FF0;       // the static CDevice: Direct3D_CreateDevice stores it to g_pDevice
constexpr uint32_t kDeviceBytes = 0x928 * 4;   // Direct3D_CreateDevice clears this many dwords when Init fails
constexpr uint32_t kDevicePtr = 0x00275FE8;    // D3D__pDevice
constexpr uint32_t kDirtyFlags = 0x00275B48;   // D3D__DirtyFlags
constexpr uint32_t kTextureState = 0x00275B50; // D3D__TextureState [4][32]
constexpr uint32_t kRenderState = 0x00275D50;  // D3D__RenderState [166]
constexpr uint32_t kStreams = 0x00278490;      // g_Stream [16] stride, offset, buffer
constexpr uint32_t kSimpleEncode = 0x002C7AD8; // D3DSIMPLERENDERSTATEENCODE, SetRenderStateNotInline

// recompiled D3D helpers kept as guest code
constexpr uint32_t kDestroyResource = 0x0026A810;  // D3D_DestroyResource
constexpr uint32_t kInitDefaultState = 0x0026B180; // called by CDevice::Init after SetRenderTarget
constexpr uint32_t kSetRenderTarget = 0x00267720;  // D3DDevice_SetRenderTarget (HLE below), for Swap's BYPASSCOPY
constexpr uint32_t kSetTexture = 0x00268400;       // D3DDevice_SetTexture (HLE below)

// CDevice fields mirrored so the recompiled getters and resource code see what the XDK would (5659 layout)
constexpr uint32_t kDevPush = 0x00, kDevThreshold = 0x04;
constexpr uint32_t kDevFence = 0x2C;             // stored into a resource as it is released (SetTexture)
constexpr uint32_t kDevPsTextureModes = 0x790;   // SetRenderState_PSTextureModes
constexpr uint32_t kDevVertexShader = 0x794;     // m_VertexShader, by XbSymbolDatabase
constexpr uint32_t kDevZScale = 0x948;           // the viewport helper 0x00265070
constexpr uint32_t kDevSuperX = 0x95C, kDevSuperY = 0x960;
constexpr uint32_t kDevTransforms = 0xC60;       // SetTransform: 10 matrices
constexpr uint32_t kDevViewport = 0xEE0;         // SetViewport's copy, GetViewport reads it
constexpr uint32_t kDevScreenOffset = 0xEF8;     // SetScreenSpaceOffset
constexpr uint32_t kDevTextures = 0xF88;         // SetTexture
constexpr uint32_t kDevPalettes = 0xF98;         // SetPalette
constexpr uint32_t kDevRenderTarget = 0x1A04;    // GetRenderTarget2
constexpr uint32_t kDevDepth = 0x1A08;           // GetDepthStencilSurface2
constexpr uint32_t kDevFrameBuffers = 0x1A14;    // CDevice::Init: back buffer, front buffer, ..., +0xC auto depth
constexpr uint32_t kDevVBlankCallback = 0x1DB8;  // m_VBlankCallback, by XbSymbolDatabase
constexpr uint32_t kDevSwapCount = 0x2478;       // Swap's return
constexpr uint32_t kDevFlags = 0x8;              // state flags; 0x4000 copy swaps (CDevice::Init 0x0026B380)

gpu::State g_state;
std::unique_ptr<gpu::Backend> g_backend;
std::recursive_mutex g_lock;

std::unordered_map<uint32_t, std::shared_ptr<gpu::VertexShader>> g_vs;
std::unordered_map<uint32_t, std::shared_ptr<gpu::PixelShader>> g_ps;
std::array<uint32_t, 3> g_buffers{};  // front, back surfaces
uint32_t g_buffer_count = 2;
uint32_t g_depth = 0;
uint32_t g_scratch_push = 0;
uint32_t g_interval = 1;  // vertical blanks a frame is shown for, from the presentation parameters
// XDK 5849 CDevice::Init (0x0026B380) sets device +8 bit 0x4000 when the presentation parameters ask for
// multisampling (MultiSampleType bits 0x3000) or D3DSWAPEFFECT_COPY (3). Swap then keeps one front buffer and
// copies into it (sub_0026A2D0): see the Swap HLE.
bool g_copy_swap = false;

// ── Guest calls ─────────────────────────────────────────────────────────────

// runs a recompiled function from here on the current guest thread: the
// arguments pushed right to left, then a return address, as `call` leaves it
uint32_t CallGuest(uint32_t fn, std::initializer_list<uint32_t> args) {
    uint32_t esp = g_esp, ebx = g_ebx, esi = g_esi, edi = g_edi;
    const uint32_t* a = args.begin();
    for (size_t i = args.size(); i-- > 0;) {
        g_esp -= 4;
        W32(g_esp, a[i]);
    }
    g_esp -= 4;
    W32(g_esp, 0);
    recomp_func_t f = recomp_lookup(fn);
    if (f) f();
    else LOG_FIRST(8, "d3d: no recompiled function at %08X", fn);
    g_esp = esp;  // stdcall or cdecl alike
    g_ebx = ebx;
    g_esi = esi;
    g_edi = edi;
    return g_eax;
}

// the XDK's internal reference count lives in bits 19-22 of Common; the last one
// going (with no external references either) destroys the resource
void AddRef(uint32_t res) {
    if (res) W32(res, R32(res) + 0x80000);
}
void Release(uint32_t res) {
    if (!res) return;
    uint32_t c = R32(res) - 0x80000;
    W32(res, c);
    if (!(c & 0x78FFFF)) CallGuest(kDestroyResource, {res});
}

uint32_t Dev(uint32_t off) { return R32(kDevice + off); }
void SetDev(uint32_t off, uint32_t v) { W32(kDevice + off, v); }
float ArgF(uint32_t v) {
    float f;
    std::memcpy(&f, &v, 4);
    return f;
}
float GF(uint32_t va) {
    float f;
    std::memcpy(&f, HostPtr(va), 4);
    return f;
}

gpu::Surface SurfaceOf(uint32_t header) {
    gpu::Surface s;
    s.header = header;
    if (header) {
        s.data = R32(header + 4);
        s.format = R32(header + 12);
        s.size = R32(header + 16);
    }
    return s;
}

uint32_t NewSurface(uint32_t w, uint32_t h, uint32_t fmt, uint32_t bpp) {
    uint32_t pitch = (w * bpp + 63) & ~63u;
    uint32_t data = mem::AllocContiguous(pitch * h, 0, 0xFFFFFFFF, 0x1000);
    uint32_t hdr = mem::AllocSystem(24);
    W32(hdr + 0, 0x000D0001);  // surface, one reference, one internal reference held by the device
    W32(hdr + 4, mem::PhysicalFromVirtual(data));
    W32(hdr + 8, 0);
    W32(hdr + 12, 0x00010021 | (fmt << 8));  // dma A, 2D, one level
    W32(hdr + 16, (w - 1) | (h - 1) << 12 | (pitch / 64 - 1) << 24);
    W32(hdr + 20, 0);
    return hdr;
}

void SyncDeferred() {
    // deferred states live in the title's memory, the inline SetRenderState and SetTextureStageState write them
    for (uint32_t i = gpu::kRsDeferredFirst; i < gpu::kRsComplexFirst; i++) g_state.rs[i] = R32(kRenderState + i * 4);
    for (int s = 0; s < 4; s++)
        for (uint32_t i = 0; i < gpu::kTssBumpEnvMat00; i++) g_state.tss[s][i] = R32(kTextureState + (s * 32 + i) * 4);
}

float ZScale(const gpu::Surface& zs) {
    uint32_t f = zs.header ? (zs.format >> 8) & 0xFF : 0x2E;
    return f == 0x2C || f == 0x30 ? 65535.0f : 16777215.0f;
}

// c-38 and c-37 hold the viewport scale and offset, as D3D_GetViewportOffsetAndScale computes them
void UpdateViewportConstants() {
    auto& v = g_state.viewport;
    g_state.zscale = ZScale(g_state.zs);
    float hw = v.w * 0.5f, hh = v.h * 0.5f;
    g_state.vsc[58] = {hw, -hh, (v.max_z - v.min_z) * g_state.zscale, 0};
    g_state.vsc[59] = {v.x + g_state.ss_offset_x + hw, v.y + g_state.ss_offset_y + hh, v.min_z * g_state.zscale, 0};
}

void SetComplex(uint32_t rs, uint32_t v) {
    g_state.rs[rs] = v;
    W32(kRenderState + rs * 4, v);
}

// frame pacing on the runtime's vblank counter (kernel_bridge.c, 59.94 Hz): Swap returns once g_interval vblanks have
// passed since the last one, as the console flips at a vblank. A title that already waited for one
// (BlockUntilVerticalBlank) then swaps at once; a separate grid here made such a frame take two vblanks (30 fps menu).
void WaitVblank(uint32_t since, uint32_t count) {
    for (int guard = 0; xbox_Nv2aVblankCount() - since < count && guard < 200; guard++) Sleep(1);
}
// xboxrecomp: a cap above the console's rate (fps_limit 120..240) paces on the host clock instead of the 59.94 Hz
// vblank grid: a high-resolution waitable timer sleeps to within half a millisecond of the frame's deadline and the
// rest is a short spin. The deadlines advance by one period each frame, so the average holds the cap; after a stall
// (loading) they restart from now rather than rushing to catch up. The runtime's vblank count and the title's vblank
// callback keep their 59.94 Hz: the game steps its logic on its own clock (25 Hz, KeQuerySystemTime), not on frames.
void PaceAbove(uint32_t fps) {
    static HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                                 TIMER_ALL_ACCESS);
    static LARGE_INTEGER freq = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f; }();
    static int64_t next = 0;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    const int64_t period = freq.QuadPart / fps;
    if (next == 0 || now.QuadPart - next > period * 4) next = now.QuadPart;
    next += period;
    int64_t left = next - now.QuadPart;
    const int64_t spin = freq.QuadPart / 2000;  // the last 0.5 ms
    if (left > spin && timer) {
        LARGE_INTEGER due;
        due.QuadPart = -((left - spin) * 10000000 / freq.QuadPart);  // relative, 100 ns units
        if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) WaitForSingleObject(timer, INFINITE);
    }
    do {
        YieldProcessor();
        QueryPerformanceCounter(&now);
    } while (now.QuadPart < next);
}
void Pace() {
    if (app::Get().fps_limit == app::kFpsUnlocked) return;  // xboxrecomp: unlocked (experimental), vsync paces
    if (hppoa_FastForward() > 1) return;  // xboxrecomp: a cutscene fast-forwarding (hle_skip.c) runs flat out
    if (app::Get().fps_limit > 60) return PaceAbove(app::Get().fps_limit);
    static uint32_t last;
    // xboxrecomp: the 30 fps limit (fps_limit) shows each frame for two vblanks at least, as a title asking for
    // D3DPRESENT_INTERVAL_TWO does
    uint32_t n = std::max<uint32_t>(g_interval, app::Get().fps_limit == 30 ? 2 : 1);
    uint32_t now = xbox_Nv2aVblankCount();
    if (now - last < n) WaitVblank(last, n);
    last = xbox_Nv2aVblankCount();
}

int g_on = -1, g_trace = 0;

}  // namespace

bool On() {
    if (g_on < 0) {
        const char* e = std::getenv("RECOMP_HLE_D3D");
        const char* t = std::getenv("RECOMP_D3D_TRACE");
        g_on = !(e && *e == '0');
        g_trace = t && *t && *t != '0';
        std::fprintf(stderr, "[D3D] %s\n", g_on ? "host D3D12 renderer (HLE, SCDA's)" : "XDK Direct3D on the emulated NV2A");
    }
    return g_on != 0;
}

}  // namespace d3d

// the generated XDK bodies this file wraps (tools/recomp/manual_scan.py reads this list)
extern "C" {
extern void sub_00264DD0_gen(void);
extern void sub_00266A90_gen(void);
extern void sub_00266BD0_gen(void);
extern void sub_00269050_gen(void);
extern void sub_00266190_gen(void);
extern void sub_00266230_gen(void);
extern void sub_00269640_gen(void);
extern void sub_0026C8F0_gen(void);
extern void sub_00266410_gen(void);
extern void sub_00267230_gen(void);
extern void sub_00267460_gen(void);
extern void sub_00267550_gen(void);
extern void sub_00267A60_gen(void);
extern void sub_00267FE0_gen(void);
extern void sub_002682A0_gen(void);
extern void sub_002685B0_gen(void);
extern void sub_00264E80_gen(void);
extern void sub_00264EB0_gen(void);
extern void sub_00264EE0_gen(void);
extern void sub_002651B0_gen(void);
extern void sub_002651F0_gen(void);
extern void sub_00265230_gen(void);
extern void sub_00265280_gen(void);
extern void sub_002652F0_gen(void);
extern void sub_00265330_gen(void);
extern void sub_00265370_gen(void);
extern void sub_002653D0_gen(void);
extern void sub_00265440_gen(void);
extern void sub_002654C0_gen(void);
extern void sub_00265540_gen(void);
extern void sub_002655A0_gen(void);
extern void sub_002655F0_gen(void);
extern void sub_00265650_gen(void);
extern void sub_002656D0_gen(void);
extern void sub_00265720_gen(void);
extern void sub_00265830_gen(void);
extern void sub_002658A0_gen(void);
extern void sub_002658E0_gen(void);
extern void sub_00265920_gen(void);
extern void sub_00266100_gen(void);
extern void sub_002662A0_gen(void);
extern void sub_002662D0_gen(void);
extern void sub_00266340_gen(void);
extern void sub_002663B0_gen(void);
extern void sub_002663D0_gen(void);
extern void sub_002663F0_gen(void);
extern void sub_00266440_gen(void);
extern void sub_00266470_gen(void);
extern void sub_002664F0_gen(void);
extern void sub_00266540_gen(void);
extern void sub_00266CB0_gen(void);
extern void sub_00266D40_gen(void);
extern void sub_00266DA0_gen(void);
extern void sub_00266E40_gen(void);
extern void sub_00267090_gen(void);
extern void sub_00267720_gen(void);
extern void sub_002672C0_gen(void);
extern void sub_002673E0_gen(void);
extern void sub_00267470_gen(void);
extern void sub_002674A0_gen(void);
extern void sub_002674F0_gen(void);
extern void sub_00267650_gen(void);
extern void sub_002679A0_gen(void);
extern void sub_00267D90_gen(void);
extern void sub_00267DB0_gen(void);
extern void sub_00268400_gen(void);
extern void sub_002686D0_gen(void);
extern void sub_00268700_gen(void);
extern void sub_002687E0_gen(void);
extern void sub_00268AE0_gen(void);
extern void sub_00270D00_gen(void);
extern void sub_0026AA80_gen(void);
extern void sub_0026A610_gen(void);
extern void sub_0026C360_gen(void);
extern void sub_0026C480_gen(void);
extern void sub_0026C540_gen(void);
extern void sub_0026C6B0_gen(void);
extern void sub_0026C7F0_gen(void);
extern void sub_0026C800_gen(void);
extern void sub_0026C850_gen(void);
extern void sub_0026C870_gen(void);
extern void sub_002694D0_gen(void);
extern void sub_00269780_gen(void);
extern void sub_00269820_gen(void);
extern void sub_00269A80_gen(void);
extern void sub_00269B20_gen(void);
extern void sub_00269B60_gen(void);
extern void sub_0026DA30_gen(void);
}

using namespace d3d;

// ── Entry point glue ────────────────────────────────────────────────────────

#define A(i) R32(g_esp + 4 + 4 * (i))
#define TRACE(...)                                                      \
    do {                                                                \
        if (g_trace) {                                                  \
            std::fprintf(stderr, "[D3D] " __VA_ARGS__);                 \
            std::fputc('\n', stderr);                                   \
        }                                                               \
    } while (0)

// a host body for the guest function at addr: __stdcall with nargs stack
// arguments (also __fastcall, whose first two are ecx and edx and the rest on
// the stack), or the XDK's own body when the HLE is off
#define HLE(addr, nargs)                                         \
    extern "C" void sub_##addr##_gen(void);                      \
    static uint32_t hle_##addr();                                \
    extern "C" void sub_##addr(void) {                           \
        if (!On()) {                                             \
            sub_##addr##_gen();                                  \
            return;                                              \
        }                                                        \
        uint32_t r = hle_##addr();                               \
        g_eax = r;                                               \
        g_esp += 4 + 4 * (nargs);                                \
    }                                                            \
    static uint32_t hle_##addr()

// device

// Direct3D_CreateDevice(adapter, type, window, flags, params, device**)
HLE(00264DD0, 6) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    uint32_t pp = A(4);
    uint32_t w = R32(pp), h = R32(pp + 4), fmt = R32(pp + 8), count = std::max<uint32_t>(R32(pp + 12), 1);
    uint32_t auto_depth = R32(pp + 0x20), depth_fmt = R32(pp + 0x24), interval = R32(pp + 0x30);
    if (!w) w = 640;
    if (!h) h = 480;
    LOGI("d3d: CreateDevice %ux%u format %X buffers %u depth %u format %X interval %X flags %X", w, h, fmt, count,
         auto_depth, depth_fmt, interval, R32(pp + 0x28));
    uint32_t multisample = R32(pp + 0x10), swap_effect = R32(pp + 0x14);
    g_copy_swap = (multisample >> 8 & 0x30) || swap_effect == 3;
    LOGI("d3d: CreateDevice multisample %X swap effect %u: %s", multisample, swap_effect,
         g_copy_swap ? "copy swaps (one front buffer)" : "flipping swaps");
    g_interval = interval == 2 ? 2 : interval == 3 ? 3 : interval == 4 ? 4 : 1;
    if (interval == 0x80000000u) g_interval = 1;
    std::memset(HostPtr(kDevice), 0, kDeviceBytes);
    W32(kDevicePtr, kDevice);
    // anything still pushing hardware commands writes into a scratch buffer that MakeSpace rewinds
    g_scratch_push = mem::AllocContiguous(0x10000, 0, 0xFFFFFFFF, 0x1000);
    SetDev(kDevPush, g_scratch_push);
    SetDev(kDevThreshold, g_scratch_push + 0xF000);

    uint32_t color_fmt = gpu::LinearColorFormat(fmt);
    g_buffer_count = std::min<uint32_t>(count + 1, 3);
    for (uint32_t i = 0; i < g_buffer_count; i++) g_buffers[i] = NewSurface(w, h, color_fmt, 4);
    g_depth = 0;
    if (auto_depth) {
        uint32_t zf = gpu::LinearDepthFormat(depth_fmt);
        g_depth = NewSurface(w, h, zf, zf == 0x30 || zf == 0x31 ? 2 : 4);  // 0x30 LIN_D16, 0x31 LIN_F16
    }
    if (!g_backend) {
        g_backend = gpu::CreateBackend();
        if (!g_backend->Init(w, h)) LOGE("d3d: renderer init failed");
        g_backend = gpu::CreateThreadedBackend(std::move(g_backend));
        // the window thread stops painting its own picture: the swap chain owns the window now
        xbox_FramebufferWindowSetPresenter([](void*, const XboxPresentInfo*) { return 1; });
        // xboxrecomp: the start menu (app/menu.cpp): the title waits here, before its first movie, while the page is
        // drawn over a black frame; its aspect ratio is the run's (below, and hle_video.c's widescreen check)
        if (app::Get().start_menu) {
            app::OpenStartMenu();
            LOGI("d3d: start menu up, the title waits for Start");
            while (app::StartMenuOpen()) {
                {
                    std::lock_guard<std::recursive_mutex> lock(g_lock);
                    g_backend->Present(SurfaceOf(g_buffers[1]), false);
                }
                Sleep(16);
            }
        }
    }
    g_state = gpu::State{};
    g_state.viewport = {0, 0, w, h, 0.0f, 1.0f};
    SetDev(kDevRenderTarget, g_buffers[1]);
    SetDev(kDevDepth, g_depth);
    SetDev(kDevFrameBuffers + 0, g_buffers[1]);
    SetDev(kDevFrameBuffers + 4, g_buffers[0]);
    SetDev(kDevFrameBuffers + 0xC, g_depth);
    if (g_copy_swap) SetDev(kDevFlags, Dev(kDevFlags) | 0x4000);
    float one = 1.0f, zs = g_depth && (R32(g_depth + 12) >> 8 & 0xFF) == 0x30 ? 65535.0f : 16777215.0f;
    std::memcpy(HostPtr(kDevice + kDevSuperX), &one, 4);
    std::memcpy(HostPtr(kDevice + kDevSuperY), &one, 4);
    std::memcpy(HostPtr(kDevice + kDevZScale), &zs, 4);
    g_state.rt = SurfaceOf(g_buffers[1]);
    g_state.zs = SurfaceOf(g_depth);
    UpdateViewportConstants();
    // the XDK's own default state setup, through the entry points below
    CallGuest(kInitDefaultState, {});
    if (A(5)) W32(A(5), kDevice);
    return 0;
}

// synchronisation, the renderer never runs behind

// D3D_BlockOnResource(resource)
HLE(0026C870, 1) {
    uint32_t res = A(0);
    if (res && g_backend) {
        std::lock_guard<std::recursive_mutex> lock(g_lock);
        uint32_t type = R32(res) & 0x70000;  // 0x40000 texture, 0x50000 surface
        if (type == 0x40000 || type == 0x50000) {
            uint32_t data = mem::VirtualFromPhysical(R32(res + 4));
            g_backend->Sync(data, 0);
        } else {
            g_backend->WaitResource(res);
        }
    }
    gpu::MarkDirty(res);
    return 0;
}

// Lock2DSurface(resource, face, level, locked, rect, flags): D3DLOCK_NOOVERWRITE skips the block above, yet a
// title may read what the GPU drew
extern "C" void sub_0026DA30_gen(void);
extern "C" void sub_0026DA30(void) {
    if (On()) {
        uint32_t res = A(0);
        if ((A(5) & 0x20) && res && g_backend) {
            std::lock_guard<std::recursive_mutex> lock(g_lock);
            g_backend->SyncLate(mem::VirtualFromPhysical(R32(res + 4)));
        }
    }
    sub_0026DA30_gen();
}

// D3D_BlockOnNonSurfaceResource(resource), 5659: what D3DVertexBuffer_Lock2 (and the other non-surface locks) wait
// in unless D3DLOCK_NOOVERWRITE. The XDK waits through D3D_BlockOnTime, which the HLE answers at once, so the title
// rewrote its ring vertex buffer of CPU-skinned characters while the render thread still read it: bodies drawn with
// another character's vertices (missing, flickering, eyes floating alone). Wait for the render thread instead.
HLE(0026C8F0, 1) {
    uint32_t res = A(0);
    if (res && g_backend) {
        std::lock_guard<std::recursive_mutex> lock(g_lock);
        g_backend->WaitResource(res);
    }
    return 0;
}

// D3DResource_BlockUntilNotBusy(resource)
// xdk_port: whole-library run match (not a unique body): suspect this binding first (docs/bringup.md)
HLE(0026AA80, 1) {
    if (A(0) && g_backend) {
        std::lock_guard<std::recursive_mutex> lock(g_lock);
        g_backend->WaitResource(A(0));
    }
    gpu::MarkDirty(A(0));
    return 0;
}
HLE(002686D0, 0) { return 0; }  // D3DDevice_IsBusy
HLE(0026C540, 2) { return 0; }  // D3D_BlockOnTime(time, ...)
// D3D_SetFence(flags)
HLE(0026C480, 1) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    return g_backend ? g_backend->Fence() : 0;
}
// D3D_KickOffAndWaitForIdle
HLE(0026C850, 0) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    if (g_backend) g_backend->Flush();
    return 0;
}
HLE(0026C360, 0) { return 0; }  // CDevice_KickOff (__thiscall, no stack arguments)

// D3DDevice_MakeSpace
HLE(0026C7F0, 0) {
    LOG_FIRST(16, "d3d: MakeSpace from %08X, some caller still pushes hardware commands", R32(g_esp));
    SetDev(kDevPush, g_scratch_push);
    return g_scratch_push;
}
// D3D_MakeRequestedSpace(minimum, requested)
HLE(0026C6B0, 2) {
    LOG_FIRST(16, "d3d: MakeRequestedSpace from %08X", R32(g_esp));
    SetDev(kDevPush, g_scratch_push);
    return g_scratch_push;
}
// D3DDevice_BeginStateBig(count): room for a long run of pushes
HLE(0026C800, 1) {
    LOG_FIRST(16, "d3d: BeginStateBig from %08X", R32(g_esp));
    SetDev(kDevPush, g_scratch_push);
    return g_scratch_push;
}

// D3DDevice_BlockUntilVerticalBlank
HLE(00267470, 0) {
    // xboxrecomp: unlocked and caps above 60 (Pace) do not wait for the 59.94 Hz grid, nor a cutscene fast-forwarding
    if (app::Get().fps_limit > 60 || hppoa_FastForward() > 1) return 0;
    WaitVblank(xbox_Nv2aVblankCount(), 1);  // the next vblank
    return 0;
}

// fences (SCDA): the renderer's own
// D3DDevice_InsertFence()
// xdk_port: anchored match (not a unique body): suspect this binding first (docs/bringup.md)
HLE(00267550, 0) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    return g_backend ? g_backend->Fence() : 0;
}
// D3DDevice_IsFencePending(fence)
#if 0  /* PoA: hp1 00195B90 not found in PoA by body match or XbSymbolDatabase 5849; no title push through it (check_hle_coverage) */
HLE(00195B90, 1) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    return g_backend && g_backend->FencePending(A(0)) ? 1 : 0;
}
#endif
// D3DDevice_BlockOnFence(fence)
HLE(00267460, 1) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    if (g_backend) g_backend->WaitFence(A(0));
    return 0;
}

// D3DDevice_SetGammaRamp(flags, ramp): 3 x 256 bytes, red, green, blue (SCDA)
HLE(00267230, 2) {
    uint8_t ramp[768];
    std::memcpy(ramp, HostPtr(A(1)), 768);
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    if (g_backend) g_backend->SetGamma(ramp);
    return 0;
}

// D3DDevice_SetVerticalBlankCallback(callback). The XDK calls it from its vblank interrupt's DPC with a
// D3DVBLANKDATA { VBlank, Swap, Flags }; the HLE device has no interrupt, so the runtime's vblank tick does, on its
// timer thread (kernel_bridge.c xbox_SetVblankHook). This title's callback runs its ten registered vblank handlers.
namespace {
uint32_t g_vblank_data = 0;
void VblankHook() {
    uint32_t cb = Dev(kDevVBlankCallback);
    if (!cb || !g_vblank_data) return;
    static bool first = true;
    if (first) {
        first = false;
        LOGI("d3d: vblank callback %08X runs from the runtime's vblank", cb);
    }
    W32(g_vblank_data + 0, xbox_Nv2aVblankCount());
    W32(g_vblank_data + 4, Dev(kDevSwapCount));
    W32(g_vblank_data + 8, 0);
    CallGuest(cb, {g_vblank_data});
}
}  // namespace
// PoA: SetVerticalBlankCallback is not linked (nothing in PoA's D3D reads or writes device +0x1DB8); the CDevice
// layout here matches 5659's (BlockUntilVerticalBlank 0x00267470 waits on the same +0x1DBC event), so the constant
// stands for titles that do call it.
#if 0  /* PoA: not linked, see above */
HLE(00192070, 1) {
    if (!g_vblank_data) {
        g_vblank_data = mem::AllocSystem(16, 16);
        std::memset(HostPtr(g_vblank_data), 0, 16);
        xbox_SetVblankHook(VblankHook);
    }
    SetDev(kDevVBlankCallback, A(0));
    TRACE("SetVerticalBlankCallback %08X", A(0));
    return 0;
}
#endif
HLE(002674A0, 1) { return 0; }  // D3DDevice_SetFlickerFilter
HLE(002674F0, 1) { return 0; }  // D3DDevice_SetSoftDisplayFilter
HLE(00268700, 2) { return 0; }  // D3D_SetTileNoWait: the console's memory controller's business

// D3DDevice_GetDisplayMode(mode): Width, Height, RefreshRate, Flags, Format of the device's buffers
HLE(00267650, 1) {
    uint32_t m = A(0);
    uint32_t w = 640, h = 480;
    if (g_buffers[1]) gpu::SurfaceSize(SurfaceOf(g_buffers[1]), w, h);
    W32(m + 0, w);
    W32(m + 4, h);
    W32(m + 8, 60);
    W32(m + 12, 0);
    W32(m + 16, 0x1E);  // D3DFMT_LIN_X8R8G8B8
    return 0;
}

// targets and presentation

// D3DDevice_SetRenderTarget(target, depth)
HLE(00267720, 2) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    uint32_t rt = A(0) ? A(0) : Dev(kDevRenderTarget), zs = A(1);
    AddRef(rt);
    AddRef(zs);
    uint32_t old_rt = Dev(kDevRenderTarget), old_zs = Dev(kDevDepth);
    SetDev(kDevRenderTarget, rt);
    SetDev(kDevDepth, zs);
    Release(old_rt);
    Release(old_zs);
    g_state.rt = SurfaceOf(rt);
    g_state.zs = SurfaceOf(zs);
    // SetRenderTarget resets the viewport to the whole target
    uint32_t w, h;
    gpu::SurfaceSize(g_state.rt, w, h);
    g_state.viewport = {0, 0, w, h, 0.0f, 1.0f};
    float zscale = ZScale(g_state.zs);
    std::memcpy(HostPtr(kDevice + kDevZScale), &zscale, 4);
    UpdateViewportConstants();
    return 0;
}

// D3DDevice_GetBackBuffer2(index): -1 the front buffer
HLE(002679A0, 1) {
    int32_t i = int32_t(A(0));
    uint32_t s = i < 0 ? g_buffers[0] : g_buffers[std::min<uint32_t>(uint32_t(i) + 1, g_buffer_count - 1)];
    W32(s, R32(s) + 1);
    return s;
}
// D3DDevice_GetRenderTarget2
#if 0  /* PoA: hp1 00192BC0 not found in PoA by body match or XbSymbolDatabase 5849; no title push through it (check_hle_coverage) */
HLE(00192BC0, 0) {
    uint32_t s = Dev(kDevRenderTarget);
    if (s) W32(s, R32(s) + 1);
    return s;
}
#endif
// D3DDevice_GetDepthStencilSurface2
HLE(00267D90, 0) {
    uint32_t s = Dev(kDevDepth);
    if (s) W32(s, R32(s) + 1);
    return s;
}

// D3DDevice_Swap(flags)
static std::atomic<uint64_t> g_last_swap_ms{0};
static std::atomic<int> g_hud_trace{0};  // xboxrecomp debug: HudTrace, F12
static uint32_t g_hud_n = 0;

// xboxrecomp: the debug trainer's report asks for an F12 capture of the next frame (hle_trainer.c)
extern "C" void hppoa_RequestCapture(void) { gpu::g_capture = true; }

// xboxrecomp: the run is 16:9 (hle_video.c widens the 4:3 cameras)
extern "C" int hppoa_Widescreen(void) { return app::BootAspect() == app::AspectMode::Wide; }

// A movie frame went up (hle_video.c's UpdateOverlay). The NV2A scans the overlay out by itself, so a title playing
// an XMV need not Swap; when it has not swapped lately, present the last frame for the overlay to be drawn over.
extern "C" void hle_d3d8_overlay_present(void) {
    if (!On() || GetTickCount64() - g_last_swap_ms.load() < 50) return;
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    if (g_backend && g_buffers[1]) g_backend->Present(SurfaceOf(g_buffers[1]), false);
}

// Swap(flags): 0 means COPY | FINISH (5). Flipping swaps (the XDK's path without device flag 0x4000) do nothing
// until FINISH (4), which shows the back buffer. Copy swaps (sub_0026A2D0) keep one front buffer: COPY (1) or
// BYPASSCOPY (2) counts the swap; COPY copies the back buffer into the front buffer itself (shown here by presenting
// the back buffer), while BYPASSCOPY saves the render target, depth and textures (sub_0026A050) and makes the front
// buffer the render target (no depth) and the back buffer texture 0 (stages 1-3 empty) for the title to copy it (PoA: a full-screen quad,
// sub_00034110 from its present routine sub_000345B0, between Swap(2) and Swap(4)). FINISH (sub_0026A0E0) puts the
// saved target, depth and textures back and shows the front buffer.
namespace {
struct SavedTargets {
    bool active = false;
    uint32_t rt = 0, zs = 0, tex[4] = {};
} g_bypass;

void SaveTargets() {
    g_bypass.active = true;
    g_bypass.rt = Dev(kDevRenderTarget);
    g_bypass.zs = Dev(kDevDepth);
    AddRef(g_bypass.rt);
    AddRef(g_bypass.zs);
    for (uint32_t i = 0; i < 4; i++) {
        g_bypass.tex[i] = Dev(kDevTextures + i * 4);
        AddRef(g_bypass.tex[i]);
    }
}

void RestoreTargets() {
    CallGuest(kSetRenderTarget, {g_bypass.rt, g_bypass.zs});
    for (uint32_t i = 0; i < 4; i++) CallGuest(kSetTexture, {i, g_bypass.tex[i]});
    Release(g_bypass.rt);
    Release(g_bypass.zs);
    for (uint32_t t : g_bypass.tex) Release(t);
    g_bypass = {};
}
}  // namespace

HLE(0026A610, 1) {
    uint32_t flags = A(0) ? A(0) : 5;
    bool show_front = false;
    if (g_copy_swap) {
        if ((flags & 2) && !g_bypass.active) {
            SaveTargets();
            CallGuest(kSetRenderTarget, {g_buffers[0], 0});  // the front buffer, no depth
            CallGuest(kSetTexture, {0, g_buffers[1]});       // the back buffer
            for (uint32_t i = 1; i < 4; i++) CallGuest(kSetTexture, {i, 0});
        }
        if (flags & 3) SetDev(kDevSwapCount, Dev(kDevSwapCount) + 1);
        if (!(flags & 4)) return Dev(kDevSwapCount);
        // COPY: the XDK draws the copy itself, so the back buffer is the picture; BYPASSCOPY: the title drew it
        show_front = !(flags & 1) && g_bypass.active;
        if (g_bypass.active) RestoreTargets();
    } else if (!(flags & 4)) {
        return Dev(kDevSwapCount);
    }
    g_last_swap_ms.store(GetTickCount64());
    {   // xboxrecomp debug: F12 down starts a [HUD] trace of the next frame, this Swap ends one
        static bool was_down;
        // Insert as well: some desktops and terminals keep F12 for themselves
        bool down = ((GetAsyncKeyState(VK_F12) | GetAsyncKeyState(VK_INSERT)) & 0x8000) != 0;
        if (g_hud_trace.exchange(0)) fprintf(stderr, "[HUD] end\n");
        // RECOMP_CAPTURE_AFTER=<seconds>[:<every>:<count>]: a capture that long after the first Swap, then <count>
        // more <every> seconds apart (unattended screenshots)
        static double cap_after, cap_every;
        static int cap_left = [] {
            const char* e = std::getenv("RECOMP_CAPTURE_AFTER");
            int n = 0;
            if (e && std::sscanf(e, "%lf:%lf:%d", &cap_after, &cap_every, &n) < 3) n = 0;
            return e && cap_after > 0 ? n + 1 : 0;
        }();
        static uint64_t first_swap_ms;
        if (!first_swap_ms) first_swap_ms = GetTickCount64();
        if (cap_left > 0 && GetTickCount64() - first_swap_ms >= uint64_t(cap_after * 1000)) {
            cap_left--;
            cap_after += cap_every;
            gpu::g_capture = true;
            fprintf(stderr, "[D3D] timed capture\n");
        }
        if (down && !was_down) {
            gpu::g_capture = true;  // this GetAsyncKeyState takes F12's pressed bit the renderer's own check reads
            g_hud_n = 0;
            g_hud_trace.store(1);
            fprintf(stderr, "[HUD] begin\n");
        }
        was_down = down;
    }
    {   // frame pacing, every 5 s: frames, the slowest one and how many took over 25 ms (a dropped 60 Hz frame)
        static LARGE_INTEGER freq, last, t0;
        static uint32_t frames, slow;
        static double worst;
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        if (!freq.QuadPart) {
            QueryPerformanceFrequency(&freq);
            t0 = last = now;
        }
        double dt = double(now.QuadPart - last.QuadPart) * 1000.0 / double(freq.QuadPart);
        last = now;
        frames++;
        if (dt > worst) worst = dt;
        if (dt > 25.0) slow++;
        double span = double(now.QuadPart - t0.QuadPart) / double(freq.QuadPart);
        if (span >= 5.0) {
            fprintf(stderr, "[FRAME] %.1f fps, slowest %.1f ms, %u frames over 25 ms\n", frames / span, worst, slow);
            frames = slow = 0;
            worst = 0;
            t0 = now;
        }
    }
    if (g_copy_swap) {
        std::lock_guard<std::recursive_mutex> lock(g_lock);
        if (g_backend) g_backend->Present(SurfaceOf(g_buffers[show_front ? 0 : 1]), false);
    } else {
        std::lock_guard<std::recursive_mutex> lock(g_lock);
        if (g_backend) g_backend->Present(SurfaceOf(g_buffers[1]), false);
        // like the XDK, the surfaces stay put and their memory rotates: titles keep back buffer pointers
        uint32_t last = g_buffers[g_buffer_count - 1], data = R32(last + 4), lk = R32(last + 8);
        for (uint32_t i = g_buffer_count - 1; i > 0; i--) {
            W32(g_buffers[i] + 4, R32(g_buffers[i - 1] + 4));
            W32(g_buffers[i] + 8, R32(g_buffers[i - 1] + 8));
        }
        W32(g_buffers[0] + 4, data);
        W32(g_buffers[0] + 8, lk);
        g_state.rt = SurfaceOf(Dev(kDevRenderTarget));
    }
    xbox_FramebufferWindowFlipped();
    xbox_FramebufferWindowFrameStats(0);
    Pace();
    if (g_copy_swap) return Dev(kDevSwapCount);  // counted by COPY / BYPASSCOPY above
    uint32_t n = Dev(kDevSwapCount) + 1;
    SetDev(kDevSwapCount, n);
    return n;
}

// D3DDevice_Clear(count, rects, flags, color, z, stencil)
// xdk_port: XbSymbolDatabase signature match (5849; hp1's body differs, docs/bringup.md); ret 0x18
HLE(00269050, 6) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    const uint8_t* rects = A(0) && A(1) ? static_cast<const uint8_t*>(HostPtr(A(1))) : nullptr;
    if (g_backend) g_backend->Clear(g_state, A(0), rects, A(2), A(3), ArgF(A(4)), A(5));
    return 0;
}

// D3DDevice_CopyRects(source, rects, count, destination, points) (SCDA)
HLE(00267A60, 5) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    if (g_backend) g_backend->Copy(SurfaceOf(A(0)), SurfaceOf(A(3)), A(2), A(1), A(4));
    return 0;
}

// D3DDevice_SetViewport(viewport)
HLE(00267DB0, 1) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    uint32_t v = A(0);
    g_state.viewport = {R32(v), R32(v + 4), R32(v + 8), R32(v + 12), GF(v + 16), GF(v + 20)};
    g_state.scissor_count = 0;
    // the XDK's copy, which GetViewport and GetViewportOffsetAndScale read
    std::memcpy(HostPtr(kDevice + kDevViewport), HostPtr(v), 24);
    UpdateViewportConstants();
    return 0;
}

// D3DDevice_SetScissors(count, exclusive, rects)
HLE(002687E0, 3) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    g_state.scissor_count = std::min<uint32_t>(A(0), 8);
    for (uint32_t i = 0; i < g_state.scissor_count * 4; i++) g_state.scissors[i] = R32(A(2) + i * 4);
    return 0;
}

// D3DDevice_SetScreenSpaceOffset(x, y)
HLE(00268AE0, 2) {
    g_state.ss_offset_x = ArgF(A(0));
    g_state.ss_offset_y = ArgF(A(1));
    W32(kDevice + kDevScreenOffset, A(0));
    W32(kDevice + kDevScreenOffset + 4, A(1));
    UpdateViewportConstants();
    return 0;
}

// render states

// D3DDevice_SetRenderState_Simple(method, value), __fastcall
HLE(00264EB0, 0) {
    uint32_t method = g_ecx & 0x1FFC, value = g_edx;
    g_state.simple[method >> 2] = value;
    return 0;
}

// D3DDevice_SetRenderStateNotInline(state, value)
HLE(00264EE0, 2) {
    uint32_t rs = A(0), v = A(1);
    if (rs < gpu::kRsDeferredFirst) {
        uint32_t method = R32(kSimpleEncode + rs * 4) & 0x1FFC;
        g_state.simple[method >> 2] = v;
        W32(kRenderState + rs * 4, v);
    } else if (rs < gpu::kRsComplexFirst) {
        W32(kRenderState + rs * 4, v);
    } else if (rs < gpu::kRsMax) {
        SetComplex(rs, v);
    }
    return 0;
}

// D3DDevice_SetRenderState_ZBias(value). Not a plain complex state: the XDK turns it into the polygon offset simple
// states, which are what the renderer keys depth bias on (decals such as windows z-fight without it). As 5028 does:
// f = -(float)(DWORD)value; PolygonOffsetZSlopeScale (0x4D) = f / 4, PolygonOffsetZOffset (0x4E) = f, and the
// point/wireframe/solid offset enables (0x4F-0x51) = value != 0.
HLE(002654C0, 1) {
    uint32_t v = A(0);
    float f = -float(v);  // the XDK reads the DWORD unsigned
    float slope = f * 0.25f;
    uint32_t bits_slope, bits_f, on = v != 0;
    std::memcpy(&bits_slope, &slope, 4);
    std::memcpy(&bits_f, &f, 4);
    const uint32_t states[5][2] = {{0x4D, bits_slope}, {0x4E, bits_f}, {0x4F, on}, {0x50, on}, {0x51, on}};
    for (const auto& sv : states) {
        uint32_t method = R32(kSimpleEncode + sv[0] * 4) & 0x1FFC;
        g_state.simple[method >> 2] = sv[1];
        W32(kRenderState + sv[0] * 4, sv[1]);
    }
    SetComplex(gpu::kRsZBias, v);
    return 0;
}

#define COMPLEX_STATE(addr, rs) \
    HLE(addr, 1) {              \
        SetComplex(rs, A(0));   \
        return 0;               \
    }
// D3DDevice_SetRenderState_<name>(value), at this XBE's addresses
COMPLEX_STATE(00264E80, gpu::kRsPsTextureModes)
COMPLEX_STATE(002656D0, gpu::kRsVertexBlend)
COMPLEX_STATE(00265230, gpu::kRsFogColor)
COMPLEX_STATE(002655A0, gpu::kRsFillMode)
COMPLEX_STATE(002655F0, gpu::kRsBackFillMode)
COMPLEX_STATE(00265650, gpu::kRsTwoSidedLighting)
COMPLEX_STATE(00265330, gpu::kRsNormalizeNormals)
COMPLEX_STATE(00266100, gpu::kRsZEnable)
// by their bodies (docs/bringup.md): store D3D__RenderState[144] / [145] (0x275F90 / 0x275F94) and push NV097 0x32C
// (stencil test enable) / 0x370 (stencil op fail)
COMPLEX_STATE(00266190, gpu::kRsStencilEnable)
COMPLEX_STATE(00266230, gpu::kRsStencilFail)
COMPLEX_STATE(002652F0, gpu::kRsFrontFace)
COMPLEX_STATE(00265280, gpu::kRsCullMode)
COMPLEX_STATE(00265370, gpu::kRsTextureFactor)
COMPLEX_STATE(00265540, gpu::kRsLogicOp)
COMPLEX_STATE(002651B0, gpu::kRsEdgeAntiAlias)
COMPLEX_STATE(00266470, gpu::kRsMultiSampleAntiAlias)
COMPLEX_STATE(002664F0, gpu::kRsMultiSampleMask)
COMPLEX_STATE(00266440, gpu::kRsMultiSampleRenderTargetMode)
COMPLEX_STATE(00266410, gpu::kRsMultiSampleMode)
COMPLEX_STATE(002651F0, gpu::kRsShadowFunc)
COMPLEX_STATE(002653D0, gpu::kRsLineWidth)
COMPLEX_STATE(00266540, gpu::kRsSampleAlpha)
COMPLEX_STATE(00265440, gpu::kRsDxt1NoiseEnable)
COMPLEX_STATE(002662A0, gpu::kRsYuvEnable)
// xdk_port: anchored match (not a unique body): suspect this binding first (docs/bringup.md)
COMPLEX_STATE(002662D0, gpu::kRsOcclusionCullEnable)
// xdk_port: anchored match (not a unique body): suspect this binding first (docs/bringup.md)
COMPLEX_STATE(00266340, gpu::kRsStencilCullEnable)
// xdk_port: anchored match (not a unique body): suspect this binding first (docs/bringup.md)
COMPLEX_STATE(002663B0, gpu::kRsRopZCmpAlwaysRead)
// xdk_port: anchored match (not a unique body): suspect this binding first (docs/bringup.md)
COMPLEX_STATE(002663D0, gpu::kRsRopZRead)
// xdk_port: anchored match (not a unique body): suspect this binding first (docs/bringup.md)
COMPLEX_STATE(002663F0, gpu::kRsDoNotCullUncompressed)

// D3DDevice_SetTextureState_TexCoordIndex(stage, value)
HLE(00265720, 2) {
    g_state.tss[A(0) & 3][gpu::kTssTexCoordIndex] = A(1);
    W32(kTextureState + ((A(0) & 3) * 32 + gpu::kTssTexCoordIndex) * 4, A(1));
    return 0;
}
// D3DDevice_SetTextureState_BumpEnv(stage, type, value)
HLE(00265830, 3) {
    if (A(1) >= gpu::kTssBumpEnvMat00 && A(1) <= gpu::kTssBumpEnvLOffset) g_state.tss[A(0) & 3][A(1)] = A(2);
    return 0;
}
// D3DDevice_SetTextureState_BorderColor(stage, value)
HLE(002658A0, 2) {
    g_state.tss[A(0) & 3][gpu::kTssBorderColor] = A(1);
    return 0;
}
// D3DDevice_SetTextureState_ColorKeyColor(stage, value)
HLE(002658E0, 2) {
    g_state.tss[A(0) & 3][gpu::kTssColorKeyColor] = A(1);
    return 0;
}
// D3DDevice_SetTextureStageStateNotInline(stage, type, value)
HLE(00265920, 3) {
    uint32_t stage = A(0) & 3, type = A(1), v = A(2);
    if (type < gpu::kTssCount) {
        g_state.tss[stage][type] = v;
        W32(kTextureState + (stage * 32 + type) * 4, v);
    }
    return 0;
}

// D3DDevice_SetTransform(type, matrix): the XDK's types are view 0, projection 1, texture 2-5, world 6-9; the
// renderer keeps Direct3D 8's numbers (view 2, projection 3, texture 16, world 256). The device copy is what
// GetTransform reads.
HLE(002672C0, 2) {
    uint32_t t = A(0);
    if (t >= 10) return 0;
    std::memcpy(HostPtr(kDevice + kDevTransforms + t * 64), HostPtr(A(1)), 64);
    uint32_t pc = t < 2 ? t + 2 : t < 6 ? t + 14 : t + 250;
    std::memcpy(g_state.transforms[pc].data(), HostPtr(A(1)), 64);
    return 0;
}

// D3DDevice_SetMaterial(material)
HLE(002673E0, 1) {
    std::memcpy(g_state.material.data(), HostPtr(A(0)), 17 * 4);
    return 0;
}

// the XDK keeps its own light lists, the renderer gets a copy (SCDA)
// D3DDevice_SetLight(index, light)
extern "C" void sub_00267FE0_gen(void);
extern "C" void sub_00267FE0(void) {
    uint32_t i = A(0), l = A(1);
    if (On() && i < 8 && l) {
        std::lock_guard<std::recursive_mutex> lock(g_lock);
        std::memcpy(g_state.lights[i].data(), HostPtr(l), 26 * 4);
    }
    sub_00267FE0_gen();
}
// D3DDevice_LightEnable(index, enable)
extern "C" void sub_002682A0_gen(void);
extern "C" void sub_002682A0(void) {
    uint32_t i = A(0), on = A(1);
    if (On() && i < 8) {
        std::lock_guard<std::recursive_mutex> lock(g_lock);
        g_state.light_enable = on ? g_state.light_enable | 1u << i : g_state.light_enable & ~(1u << i);
    }
    sub_002682A0_gen();
}

// resources in use

// D3DDevice_SetTexture(stage, texture)
HLE(00268400, 2) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    uint32_t stage = A(0) & 3, tex = A(1);
    static int traced = 0;
    if (g_trace && tex && traced < 60) {
        traced++;
        TRACE("SetTexture(%u, %08X) from %08X common %08X data %08X format %08X size %08X", stage, tex, R32(g_esp),
              R32(tex), R32(tex + 4), R32(tex + 12), R32(tex + 16));
    }
    uint32_t old = Dev(kDevTextures + stage * 4);
    AddRef(tex);
    SetDev(kDevTextures + stage * 4, tex);
    if (old) W32(old + 8, Dev(kDevFence));
    Release(old);
    g_state.textures[stage] = tex;
    W32(kDirtyFlags, R32(kDirtyFlags) | 0x4800);
    return 0;
}

// D3DDevice_SetPalette(stage, palette): the device's slot at +0xF98, references as the XDK keeps them
HLE(002685B0, 2) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    uint32_t stage = A(0) & 3, pal = A(1);
    uint32_t old = Dev(kDevPalettes + stage * 4);
    AddRef(pal);
    SetDev(kDevPalettes + stage * 4, pal);
    if (old) W32(old + 8, Dev(kDevFence));
    Release(old);
    g_state.palettes[stage] = pal;
    return 0;
}

// D3DDevice_SetStreamSource(stream, buffer, stride)
HLE(00266CB0, 3) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    uint32_t n = A(0) & 15, vb = A(1), stride = A(2);
    uint32_t old = R32(kStreams + n * 12 + 8);
    AddRef(vb);
    W32(kStreams + n * 12, stride);
    W32(kStreams + n * 12 + 8, vb);
    Release(old);
    g_state.streams[n] = {vb, stride};
    return 0;
}

// shaders

// D3DDevice_CreateVertexShader(declaration, function, handle*, usage) (from hp2); ret 0x10. PoA's renderer init
// (sub_00030850) assembles its skinning program with XGRPH and creates it here.
HLE(00266A90, 4) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    uint32_t decl = A(0), func = A(1), out = A(2);
    auto vs = std::make_shared<gpu::VertexShader>();
    if (decl) {
        for (uint32_t i = 0; i < 1024; i++) {
            uint32_t t = R32(decl + i * 4);
            vs->decl.push_back(t);
            if (t == 0xFFFFFFFF) break;
        }
    }
    if (func) {
        uint32_t hdr = R32(func);
        vs->program_type = hdr & 0xFFFF;
        uint32_t n = hdr >> 16;
        for (uint32_t i = 0; i < n * 4; i++) vs->program.push_back(R32(func + 4 + i * 4));
    }
    gpu::ParseDeclaration(*vs);
    uint32_t rec = mem::AllocSystem(16, 16);
    vs->handle = rec | 1;
    g_vs[vs->handle] = vs;
    W32(out, vs->handle);
    LOG_FIRST(64, "d3d: vertex shader %08X, %zu decl tokens, %zu instructions, type %X", vs->handle, vs->decl.size(),
              vs->program.size() / 4, vs->program_type);
    return 0;
}

// D3DDevice_SetVertexShader(handle)
HLE(00267090, 1) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    uint32_t h = A(0);
    g_state.vs_handle = h;
    g_state.vs = nullptr;
    SetDev(kDevVertexShader, h);
    if (h & 1) {
        auto it = g_vs.find(h);
        if (it != g_vs.end()) {
            g_state.vs = it->second;
            // constants declared with the shader load when it is selected
            for (auto& [reg, v] : it->second->constants)
                if (reg < 192) g_state.vsc[reg] = v;
        } else {
            LOG_FIRST(8, "d3d: SetVertexShader with unknown handle %08X", h);
        }
    }
    return 0;
}

namespace {
// constants by hardware register (D3D register + 96), also into the XDK's mirror that GetVertexShaderConstant reads
void VsConstants(uint32_t reg, uint32_t src, uint32_t count) {
    // xboxrecomp debug: RECOMP_VSC_TRACE=<hw register, e.g. 109> logs each change of that constant and its caller
    static int trace = [] { const char* e = std::getenv("RECOMP_VSC_TRACE"); return e && *e ? std::atoi(e) : -1; }();
    if (trace >= 0 && uint32_t(trace) >= reg && uint32_t(trace) < reg + count) {
        static float last[4];
        const float* v = reinterpret_cast<const float*>(HostPtr(src + (trace - reg) * 16));
        if (std::memcmp(v, last, 8)) {  // x and y: the fog constant's z and w are stack garbage
            std::memcpy(last, v, 16);
            std::fprintf(stderr, "[VSC] c%d = %g %g %g %g from %08X (stack %08X %08X %08X %08X)\n", trace, v[0], v[1], v[2],
                         v[3], R32(g_esp), R32(g_esp + 4), R32(g_esp + 8), R32(g_esp + 12), R32(g_esp + 16));
        }
    }
    for (uint32_t i = 0; i < count && reg + i < 192; i++) {
        std::memcpy(g_state.vsc[reg + i].data(), HostPtr(src + i * 16), 16);
    }
}
}  // namespace

// D3DDevice_SetVertexShaderConstantNotInlineFast(register, data, dwords), __fastcall (ecx, edx); ret 4. In 5849 it
// only pushes NV097 0x1EA4 (constant load: ecx, the hardware register) and the data; no memory mirror as in 5028.
HLE(00266BD0, 1) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    VsConstants(g_ecx, g_edx, A(0) / 4);
    return 0;
}

// D3DDevice_LoadVertexShader(handle, address)
HLE(00266D40, 2) {
    LOG_FIRST(4, "d3d: LoadVertexShader %08X at %u is not supported", A(0), A(1));
    return 0;
}
// D3DDevice_SelectVertexShader(handle, address)
HLE(00266DA0, 2) {
    LOG_FIRST(4, "d3d: SelectVertexShader %08X at %u is not supported", A(0), A(1));
    return 0;
}
HLE(00266E40, 1) { return 0; }  // D3DDevice_SetShaderConstantMode

// D3DDevice_CreatePixelShader(definition, handle*)
#if 0  /* PoA: hp1 001952B0 not found in PoA by body match or XbSymbolDatabase 5849; no title push through it (check_hle_coverage) */
HLE(001952B0, 2) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    auto ps = std::make_shared<gpu::PixelShader>();
    std::memcpy(ps->def, HostPtr(A(0)), sizeof(ps->def));
    uint32_t rec = mem::AllocSystem(16, 16);
    ps->handle = rec;
    g_ps[rec] = ps;
    W32(A(1), rec);
    return 0;
}
#endif

// D3DDevice_SetPixelShader(handle)
HLE(00270D00, 1) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    g_state.ps = nullptr;
    if (!A(0)) return 0;
    auto it = g_ps.find(A(0));
    if (it == g_ps.end()) {
        LOG_FIRST(8, "d3d: SetPixelShader with unknown handle %08X", A(0));
        return 0;
    }
    g_state.ps = it->second;
    // the definition loads the combiner state like the XDK pushes it
    const uint32_t* d = it->second->def;
    auto put = [&](uint32_t method, uint32_t v) { g_state.simple[method >> 2] = v; };
    for (int i = 0; i < 8; i++) {
        put(gpu::kMPsAlphaInputs + i * 4, d[0 + i]);
        put(gpu::kMPsConstant0 + i * 4, d[10 + i]);
        put(gpu::kMPsConstant1 + i * 4, d[18 + i]);
        put(gpu::kMPsAlphaOutputs + i * 4, d[26 + i]);
        put(gpu::kMPsRgbInputs + i * 4, d[34 + i]);
        put(gpu::kMPsRgbOutputs + i * 4, d[45 + i]);
    }
    put(gpu::kMPsFinalAbcd, d[8]);
    put(gpu::kMPsFinalEfgh, d[9]);
    put(gpu::kMPsCompareMode, d[42]);
    put(gpu::kMPsFinalConstant0, d[43]);
    put(gpu::kMPsFinalConstant1, d[44]);
    put(gpu::kMPsCombinerCount, d[53]);
    put(gpu::kMPsDotMapping, d[55]);
    put(gpu::kMPsInputTexture, d[56]);
    SetComplex(gpu::kRsPsTextureModes, d[54]);
    return 0;
}

// D3DDevice_SetPixelShaderConstant(register, data, count)
#if 0  /* PoA: hp1 00195500 not found in PoA by body match or XbSymbolDatabase 5849; no title push through it (check_hle_coverage) */
HLE(00195500, 3) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    uint32_t reg = A(0), src = A(1), n = A(2);
    for (uint32_t i = 0; i < n && reg + i < 16; i++) std::memcpy(g_state.psc[reg + i].data(), HostPtr(src + i * 16), 16);
    gpu::ApplyPixelConstants(g_state, reg, n);
    return 0;
}
#endif

// drawing

// xboxrecomp debug: pressing F12 also logs the next frame's small undepth-tested draws from the title's side ([HUD]
// lines): their screen rectangle, texture, and the title's return addresses found on the stack, to find what draws
// each HUD element
namespace {
void HudTrace(uint32_t prim, uint32_t count, uint32_t verts, uint32_t stride) {
    if (!g_hud_trace.load() || count > 32 || g_state.rs[gpu::kRsZEnable]) return;
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    if (verts && stride >= 8) {
        for (uint32_t v = 0; v < count; v++) {
            float xy[2];
            std::memcpy(xy, HostPtr(verts + v * stride), 8);
            x0 = std::min(x0, xy[0]), y0 = std::min(y0, xy[1]), x1 = std::max(x1, xy[0]), y1 = std::max(y1, xy[1]);
        }
    }
    fprintf(stderr, "[HUD] %04u prim %u count %u vs %08X tex %08X rect %.1f %.1f %.1f %.1f rets", g_hud_n++, prim, count,
            g_state.vs_handle, g_state.textures[0], x0, y0, x1, y1);
    int found = 0;
    for (uint32_t i = 0; i < 256 && found < 10; i++) {
        uint32_t v = R32(g_esp + 4 * i);
        if (v < 0x11005 || v >= 0x264C64) continue;  // the title's own code (.text)
        if (R8(v - 5) == 0xE8 || R8(v - 2) == 0xFF || R8(v - 3) == 0xFF || R8(v - 6) == 0xFF) {
            fprintf(stderr, " %08X", v);
            found++;
        }
    }
    fprintf(stderr, "\n");
}

// the draw's vertices for HudTrace: stream 0 from start
uint32_t StreamVertices(uint32_t start) {
    const auto& st = g_state.streams[0];
    if (!st.vb || !st.stride) return 0;
    return mem::VirtualFromPhysical(R32(st.vb + 4)) + start * st.stride;
}
}  // namespace

// D3DDevice_DrawVertices(primitive, start, count)
HLE(00269780, 3) {
    HudTrace(A(0), A(2), StreamVertices(A(1)), g_state.streams[0].stride);
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    SyncDeferred();
    g_state.caller = R32(g_esp);
    if (g_backend) g_backend->Draw(g_state, A(0), A(1), A(2), nullptr);
    return 0;
}

// D3DDevice_DrawIndexedVertices(primitive, count, indices)
HLE(00269820, 3) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    SyncDeferred();
    const uint8_t* indices = A(2) ? static_cast<const uint8_t*>(HostPtr(A(2))) : nullptr;
    g_state.caller = R32(g_esp);
    if (g_backend) g_backend->Draw(g_state, A(0), 0, A(1), indices);
    return 0;
}

// DrawVerticesUP(prim, count, vertices, stride): the vertices are copied to a ring the render thread reads, stream 0
// points there through a vertex buffer header for the draw. A half of the ring is written again once the draws
// that read it are done.
namespace {
constexpr uint32_t kUpRing = 4u << 20, kUpHalf = kUpRing / 2;
uint32_t g_up_ring = 0, g_up_pos = 0, g_up_header = 0, g_up_fence[2] = {};

uint32_t UpVertices(uint32_t src, uint32_t bytes) {
    if (!g_up_ring) {
        g_up_ring = mem::AllocContiguous(kUpRing, 0, 0xFFFFFFFF, 0x1000);
        g_up_header = mem::AllocSystem(20);
        std::memset(HostPtr(g_up_header), 0, 20);
        W32(g_up_header, 1);
    }
    bytes = std::min(bytes, kUpHalf);
    uint32_t room = (bytes + 63) & ~63u;
    uint32_t pos = g_up_pos + room > kUpRing ? 0 : g_up_pos;
    uint32_t from = g_up_pos ? (g_up_pos - 1) / kUpHalf : 1, to = (pos + room - 1) / kUpHalf;
    if (to != from) {
        g_up_fence[from] = g_backend->Fence();
        g_backend->WaitFence(g_up_fence[to]);
    }
    std::memcpy(HostPtr(g_up_ring + pos), HostPtr(src), bytes);
    g_up_pos = pos + room;
    W32(g_up_header + 4, mem::PhysicalFromVirtual(g_up_ring + pos));
    return g_up_header;
}
}  // namespace

// D3DDevice_DrawVerticesUP(primitive, count, vertices, stride)
HLE(002694D0, 4) {
    HudTrace(A(0), A(1), A(2), A(3));
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    if (!g_backend || !A(1) || !A(3)) return 0;
    SyncDeferred();
    auto stream = g_state.streams[0];
    g_state.streams[0] = {UpVertices(A(2), A(1) * A(3)), A(3)};
    g_state.caller = R32(g_esp);
    g_backend->Draw(g_state, A(0), 0, A(1), nullptr);
    g_state.streams[0] = stream;
    return 0;
}

// D3DDevice_DrawIndexedVerticesUP(primitive, count, indices, vertices, stride) (from hp2): the vertices up to the
// largest index go through the same ring
HLE(00269640, 5) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    uint32_t count = A(1), stride = A(4);
    if (!g_backend || !count || !stride || !A(2)) return 0;
    SyncDeferred();
    const uint16_t* idx = static_cast<const uint16_t*>(HostPtr(A(2)));
    uint32_t top = 0;
    for (uint32_t i = 0; i < count; i++) top = std::max<uint32_t>(top, idx[i]);
    auto stream = g_state.streams[0];
    g_state.streams[0] = {UpVertices(A(3), (top + 1) * stride), stride};
    g_state.caller = R32(g_esp);
    g_backend->Draw(g_state, A(0), 0, count, reinterpret_cast<const uint8_t*>(idx));
    g_state.streams[0] = stream;
    return 0;
}

// immediate mode: not drawn, said once
HLE(00269B20, 1) {  // D3DDevice_Begin(primitive)
    LOG_FIRST(4, "d3d: Begin/End immediate mode is not supported (from %08X)", R32(g_esp));
    return 0;
}
HLE(00269B60, 0) { return 0; }  // D3DDevice_End
HLE(00269A80, 3) { return 0; }  // D3DDevice_SetVertexData2f(register, a, b)
