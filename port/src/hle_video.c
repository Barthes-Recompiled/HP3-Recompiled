/**
 * Display timing the D3D8 HLE no longer gets from the XDK's vblank interrupt.
 *
 * Philosopher's Stone (XDK 5659) read the device's vblank count (+0x1DE8)
 * through two entry points overridden here: D3DDevice_GetDisplayFieldStatus
 * (its XMV movie player waited on the field count) and
 * D3DDevice_GetOverlayUpdateStatus (its loading loop). PoA (XDK 5849) links
 * neither: the only code touching +0x1DE8 is the vblank interrupt handler
 * 0x00269C90 (hp1 0x0019DA10) that increments it, and the HLE device never
 * connects that interrupt. So both overrides stay disabled, kept for the
 * other HP titles' layout (docs/bringup.md, "Display timing and XAPI").
 * The HLE's Swap paces on the runtime's own vblank count instead.
 *
 * PoA's movies are `.mad` files played by sub_001AC0A0 / sub_0005DCD0 and
 * drawn as a YUY2 textured quad; its loading screens are not found yet.
 * Every entry point keeps the generated body as sub_X_gen, used when
 * RECOMP_HLE_D3D=0. RECOMP_XMV_TRACE=1 logged the field status once a second.
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
#if 0  /* PoA: not linked (no PoA caller; nothing reads device +0x1DE8), docs/bringup.md */
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
#if 0  /* PoA: not linked (no PoA caller), docs/bringup.md */
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
