/* Region BSS self-check, after the first gameplay chapter finishes loading.
 * patch_gen.py calls this at CGameChapter::Start's epilogue: its stack buffer
 * still holds data/packages/plfs/<chapter>.pkg. Frontend chapters are skipped.
 * Reads only, on the guest thread; no polling thread or gameplay state changes.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "recomp/gen/recomp_types.h"

#ifndef REGION_PROFILE
#define REGION_PROFILE "poa-usa"
#endif

void hppoa_RegionCheck(uint32_t chapter_path)
{
    static int logged;
    const char *path = (const char *)XBOX_PTR(chapter_path);
    const char prefix[] = "data/packages/plfs/";
    if (logged || strncmp(path, prefix, sizeof(prefix) - 1) ||
        !strncmp(path + sizeof(prefix) - 1, "frontend", 8)) return;
    logged = 1;

    /* Same BSS objects used by hle_video.c and hle_d3d8.cpp. Timer layout:
     * docs/bringup.md, 60 fps; initialized by sub_001A7540.
     */
    uint32_t timer = 0x0038BA6C;
    uint32_t ticks = MEM32(timer), ms = MEM32(timer + 4), delta = MEM32(timer + 8);
    uint32_t divisor = MEM32(timer + 0x0C), floor = MEM32(timer + 0x18);
    uint32_t device = MEM32(0x00275FE8);
    int ok = ticks > 0 && ms > 0 && divisor > 0 && delta <= 100 &&
             floor > 0 && floor <= 40 && device == 0x00275FF0;
    fprintf(stderr, "[REGION] %s: %.63s, clock %u ticks/%u ms, delta %u, divisor %u, floor %u, "
            "device %08X -> %s\n", REGION_PROFILE, path + sizeof(prefix) - 1,
            ticks, ms, delta, divisor, floor, device, ok ? "ok" : "SUSPECT");
}
