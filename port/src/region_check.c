/*
 * Region self-check (tools/region/README.md): logs once, as Harry first stands in a level, whether the game state
 * the hand-written code reads sits where it expects. These addresses are in bss, which a retargeted build maps by
 * the pointers into it rather than by comparing bytes, so a run on the other region's XBE confirms them here:
 *
 *   [REGION] ps-usa: level 7, player 80A1B2C0 (entity), health 3/3, spells 1 0 0, progress level 7 -> ok
 *
 * Reads only, from its own thread, every half second; it never changes the game.
 */
#include <stdint.h>
#include <stdio.h>

#include "recomp/gen/recomp_types.h"
#include "trainer_shm.h"

#ifdef _WIN32
#include <windows.h>
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <pthread.h>
#include <unistd.h>
#define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

#ifndef REGION_PROFILE
#define REGION_PROFILE "canonical"
#endif

/* the same guest addresses as hle_trainer.c */
enum { kPlayer = 0x3565B0, kLevelNow = 0x356CC4, kHealth = 0x329B90, kHealthMax = 0x329B94, kSlots = 0x329DD5,
       kLevel = 0x329E6E };

extern size_t xbox_GetMappedSize(void);

static int guest_ptr(uint32_t p) { return p >= 0x80000000u && p < 0x84000000u; }

static int spell_ok(uint8_t s)
{
    return s == kSpellNone || s == kSpellFlipendo || s == kSpellLumos || s == kSpellDiffindo ||
           s == kSpellSpongify || s == kSpellIncendio || s == kSpellAlohomora;
}

static void check(void)
{
    while (!xbox_GetMappedSize()) SLEEP_MS(500);
    for (int t = 0; t < 2 * 600; t++, SLEEP_MS(500)) {
        uint32_t level = MEM32(kLevelNow), pl = MEM32(kPlayer);
        if (level < HPPOA_LEVEL_FIRST || level > HPPOA_LEVEL_LAST || !guest_ptr(pl)) continue;
        int entity = MEM32(pl + 8) == pl;
        float h = MEMF(kHealth), hmax = MEMF(kHealthMax);
        uint8_t s0 = MEM8(kSlots), s1 = MEM8(kSlots + 1), s2 = MEM8(kSlots + 2);
        int ok = entity && h >= 0 && h <= hmax && hmax > 0 && hmax <= 1000 &&
                 spell_ok(s0) && spell_ok(s1) && spell_ok(s2);
        fprintf(stderr, "[REGION] %s: level %u, player %08X (%s), health %g/%g, spells %u %u %u, "
                "progress level %u -> %s\n", REGION_PROFILE, level, pl, entity ? "entity" : "not an entity",
                h, hmax, s0, s1, s2, MEM8(kLevel), ok ? "ok" : "SUSPECT: report this log");
        return;
    }
    fprintf(stderr, "[REGION] %s: no level with Harry in 10 minutes (level %u, player %08X): "
            "SUSPECT unless the game sat in menus\n", REGION_PROFILE, MEM32(kLevelNow), MEM32(kPlayer));
}

#ifdef _WIN32
static DWORD WINAPI check_thread(LPVOID arg) { (void)arg; check(); return 0; }
#else
static void *check_thread(void *arg) { (void)arg; check(); return NULL; }
#endif

__attribute__((constructor)) static void region_check_start(void)
{
#ifdef _WIN32
    HANDLE h = CreateThread(NULL, 0, check_thread, NULL, 0, NULL);
    if (h) CloseHandle(h);
#else
    pthread_t th;
    if (pthread_create(&th, NULL, check_thread, NULL) == 0) pthread_detach(th);
#endif
}
