/**
 * Cutscene fast-forward (hppoa.toml cutscene_skip, on by default): holding Start or Back for half a second while a
 * cutscene plays runs it through to its own end at SKIP_SCALE times the speed, with the sound muted and the picture
 * held (a frame every quarter of a second shows how far it got). Nothing in the cutscene is left out: its scripts,
 * speech and camera all run as they would, only faster, so whatever the cutscene sets up is set up.
 *
 * What counts as a cutscene is title state: Philosopher's Stone used its letterbox bars. PoA's equivalent is not found
 * yet (docs/bringup.md), so cutscene() is always false and the fast-forward never starts.
 *
 * The clock: sub_00195129 is XAPI's QueryPerformanceCounter (an rdtsc). It runs SKIP_SCALE times as fast while
 * fast-forwarding, so each frame steps the logic that many times further; the DirectSound mixer (hle_dsound.c) mixes
 * that many blocks a block and outputs silence, so speech and streams end in step, and the frame pacing (hle_d3d8.cpp)
 * and the renderer's presents (d3d12.cpp) stand aside. Start and Back still reach the game as always.
 *
 * Built into the recompile with --exclude-manual naming this file.
 */

#include <stdint.h>
#include <stdio.h>
#include <windows.h>

#define RECOMP_GENERATED_CODE
#include "recomp/gen/recomp_types.h"
#include "input/xinput_xbox.h"

extern int app_CutsceneSkip(void);           /* hppoa.toml cutscene_skip (scda/app/app.cpp) */

#define BTN_START       0x0010u
#define BTN_BACK        0x0020u
#define HOLD_MS         500u
#define SKIP_SCALE      16
#define RESUME_MS       2000u    /* a cutscene that starts again this soon after one ends goes on fast too */
#define SKIP_MAX_MS     180000u  /* a cutscene is a few minutes at most: past that the bars are stuck, not a cutscene */

static volatile LONG g_scale = 1;

int hppoa_FastForward(void) { return (int)g_scale; }

static int cutscene(void)
{
    return 0;  /* PoA's cutscene state: not found yet */
}

/* hle_input.c, port 0's pad as the game reads it: the hold that starts the fast-forward, and its end */
void hppoa_SkipPad(const XBOX_GAMEPAD *g)
{
    static DWORD held_since, ff_since, ended;
    static int held;
    DWORD now = GetTickCount();
    /* the next part of a cutscene that came in two (a moment of no cutscene between its shots) */
    if (g_scale == 1 && ended && now - ended < RESUME_MS && cutscene() && app_CutsceneSkip()) {
        ended = 0;
        ff_since = now;
        InterlockedExchange(&g_scale, SKIP_SCALE);
        fprintf(stderr, "[SKIP] cutscene fast-forward goes on into the next part\n");
        return;
    }
    if (g_scale > 1) {
        if (!cutscene() || now - ff_since > SKIP_MAX_MS) {
            InterlockedExchange(&g_scale, 1);
            ended = cutscene() ? 0 : now;
            fprintf(stderr, "[SKIP] cutscene fast-forward ended after %lu ms\n", (unsigned long)(now - ff_since));
        }
        return;
    }
    int down = (g->wButtons & (BTN_START | BTN_BACK)) != 0;
    if (!down || !cutscene() || !app_CutsceneSkip()) {
        held = 0;
        return;
    }
    if (!held) {
        held = 1;
        held_since = now;
    } else if (now - held_since >= HOLD_MS) {
        held = 0;
        ff_since = now;
        InterlockedExchange(&g_scale, SKIP_SCALE);
        fprintf(stderr, "[SKIP] cutscene fast-forward, %dx\n", SKIP_SCALE);
    }
}

/* the game's clock: the counter's own steps, SKIP_SCALE times each while fast-forwarding (it never goes back) */
static uint64_t game_clock(void)
{
    static SRWLOCK lock = SRWLOCK_INIT;
    static uint64_t last_real, clock;
    AcquireSRWLockExclusive(&lock);
    uint64_t real = xbox_ReadTimeStampCounter();
    if (!last_real) clock = real;
    else if (real > last_real) clock += (real - last_real) * (uint64_t)g_scale;
    last_real = real;
    uint64_t c = clock;
    ReleaseSRWLockExclusive(&lock);
    return c;
}

/* sub_00195129(LARGE_INTEGER *count): XAPI's QueryPerformanceCounter (rdtsc), ret 4, returns TRUE */
void sub_00195129(void)
{
    uint32_t out = MEM32(g_esp + 4);
    uint64_t c = game_clock();
    MEM32(out) = (uint32_t)c;
    MEM32(out + 4) = (uint32_t)(c >> 32);
    g_eax = 1;
    g_esp += 8;
}
