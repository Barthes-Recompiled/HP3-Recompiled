/**
 * Display timing the D3D8 HLE no longer gets from the XDK's vblank interrupt:
 * the device's vblank counter, which the XDK's interrupt advances and which the
 * HLE CreateDevice (scda/xdk/hle_d3d8.cpp) never connects:
 *
 *   D3DDevice_GetDisplayFieldStatus   a movie player waits on its field count
 *   D3DDevice_GetOverlayUpdateStatus  "a vblank passed since the last overlay
 *                                     update" (always, here)
 *
 * Every entry point keeps the generated body as sub_X_gen, used when
 * RECOMP_HLE_D3D=0 (the XDK's own Direct3D on the emulated NV2A).
 * RECOMP_XMV_TRACE=1 logs the field status once a second.
 *
 * The movie player, sub_00164450(name, ., ., per-frame callback), plays the
 * main menu's moving background ("movies/frontend.thp" -> frontend.xmv) for as
 * long as the menu is up; hppoa_MenuMovie() tells d3d12.cpp's Squeeze2D so that,
 * in 16:9, it shows that picture's middle 360 lines across the full width
 * instead of the 4:3 frame between bars (RECOMP_MENU_MOVIE_CROP=0: left 4:3).
 * The loading screen (sprites/load_hognight.tga, set up by sub_00148400) is
 * drawn by its own thread, sub_00148100, looping until the level is in;
 * hppoa_LoadingScreen() gives its picture the same crop (RECOMP_LOADING_CROP=0:
 * left 4:3).
 *
 * Built into the recompile with --exclude-manual naming this file.
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* the register names (eax, esp, ...) the runtime macros expand to */
#define RECOMP_GENERATED_CODE
#include "recomp/gen/recomp_types.h"

#define ARG(i)  MEM32(g_esp + 4 + 4 * (i))

extern uint32_t xbox_Nv2aVblankCount(void);

static int d3d_hle(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("RECOMP_HLE_D3D");
        on = !(e && *e == '0');
    }
    return on;
}

static int trace_on(void)
{
    static int on = -1;
    if (on < 0) {
        const char *t = getenv("RECOMP_XMV_TRACE");
        on = t && *t && *t != '0';
    }
    return on;
}

/* D3DDevice_GetDisplayFieldStatus(pFieldStatus): { Field, VBlankCount }. The
 * runtime's 60 Hz count, progressive (field 3, as the XDK reports when the mode
 * is not interlaced). */
#if 0  /* PoA: hp1 001934C0 has no match in PoA yet (docs/bringup.md) */
void sub_001934C0(void)
{
    static DWORD t0;
    uint32_t p = ARG(0);
    if (d3d_hle() && p) {
        MEM32(p + 4) = xbox_Nv2aVblankCount();
        MEM32(p) = 3;
        g_esp += 8;
    } else {
        sub_001934C0_gen();
    }
    if (trace_on() && p) {
        DWORD now = GetTickCount();
        if (now - t0 >= 1000) {
            fprintf(stderr, "[XMV] field status: field=%u vblank=%u\n", MEM32(p), MEM32(p + 4));
            t0 = now;
        }
    }
}
#endif

/* D3DDevice_GetOverlayUpdateStatus(): TRUE once a vblank has passed since the
 * last UpdateOverlay. The title never shows an overlay, so always. */
#if 0  /* PoA: hp1 0019D910 has no match in PoA yet (docs/bringup.md) */
void sub_0019D910(void)
{
    if (!d3d_hle()) {
        sub_0019D910_gen();
        return;
    }
    g_eax = 1;
    g_esp += 4;
}
#endif

/* Hooks the renderer asks about the title's screens (scda/gpu/d3d12.cpp's Squeeze2D and captures). Philosopher's
 * Stone answered them from its movie player, loading thread and icon table; PoA's equivalents are not found yet
 * (docs/bringup.md), so none of these screens is recognised. */
int hppoa_MenuMovie(void) { return 0; }
int hppoa_LoadingScreen(void) { return 0; }
int hppoa_LoadingCount(void) { return 0; }
int hppoa_TextIcon(uint32_t tex) { (void)tex; return 0; }
void hppoa_TextIconDump(void) {}
