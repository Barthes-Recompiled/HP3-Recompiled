/**
 * The debug trainer's game side (the trainer itself is hppoa_trainer.exe, src/trainer): a named shared block
 * (trainer_shm.h) the trainer finds at boot.
 *
 * What works for PoA so far is title-independent: pad input injection on port 0 (hle_input.c adds it to the pad the
 * game reads) and the debug report (every thread's native stack, a frame capture and the state below, into the
 * folder the trainer names, from a thread of its own so a hung title still answers). Philosopher's Stone's
 * game-side features (noclip, god mode, insta-kill, cards, spells, beans, level warps, checkpoints, undo) were tied
 * to that title's state and are gone; PoA's come back as its state is found (docs/bringup.md).
 *
 * Built into the recompile with --exclude-manual naming this file.
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define RECOMP_GENERATED_CODE
#include "recomp/gen/recomp_types.h"
#include "input/xinput_xbox.h"
#include "trainer_shm.h"

extern void hppoa_StackDumpTo(const char *path);
extern void hppoa_RequestCapture(void);
extern ptrdiff_t g_xbox_mem_offset;

/* hle_input.c: the pad as the game last read it */
extern XBOX_GAMEPAD hppoa_last_pad;

static HppoaTrainerShm *g_shm;

static HppoaTrainerShm *shm(void)
{
    static int tried;
    if (!g_shm && !tried) {
        tried = 1;
        HANDLE m = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(HppoaTrainerShm),
                                      HPPOA_TRAINER_NAME);
        if (m) g_shm = (HppoaTrainerShm *)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(HppoaTrainerShm));
        if (g_shm) {
            memset(g_shm, 0, sizeof(*g_shm));
            g_shm->version = HPPOA_TRAINER_VERSION;
            g_shm->pid = GetCurrentProcessId();
            g_shm->mem_offset = (uint64_t)g_xbox_mem_offset;
            g_shm->magic = HPPOA_TRAINER_MAGIC;
        }
    }
    return g_shm;
}

/* hle_input.c: the trainer's injected pad state, added to port 0's while it lasts (inject_seq / inject_ms) */
void hppoa_TrainerInject(XBOX_GAMEPAD *g)
{
    static uint32_t seen;
    static DWORD until;
    HppoaTrainerShm *t = shm();
    if (!t) return;
    if (t->inject_seq != seen) {
        seen = t->inject_seq;
        until = GetTickCount() + t->inject_ms;
        fprintf(stderr, "[TRAINER] input %04X analog %02X%02X%02X%02X%02X%02X%02X%02X for %u ms\n", t->inject_buttons,
                t->inject_analog[0], t->inject_analog[1], t->inject_analog[2], t->inject_analog[3], t->inject_analog[4],
                t->inject_analog[5], t->inject_analog[6], t->inject_analog[7], t->inject_ms);
    }
    if ((LONG)(GetTickCount() - until) >= 0) return;
    g->wButtons |= t->inject_buttons;
    for (int i = 0; i < 8; i++)
        if (t->inject_analog[i] > g->bAnalogButtons[i]) g->bAnalogButtons[i] = t->inject_analog[i];
    if (t->inject_stick[0] || t->inject_stick[1]) g->sThumbLX = t->inject_stick[0], g->sThumbLY = t->inject_stick[1];
    if (t->inject_stick[2] || t->inject_stick[3]) g->sThumbRX = t->inject_stick[2], g->sThumbRY = t->inject_stick[3];
}

static void write_state(const char *path, HppoaTrainerShm *t)
{
    FILE *f = fopen(path, "w");
    if (!f) return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(f, "time %04u-%02u-%02u %02u:%02u:%02u, %.1f s since boot
", st.wYear, st.wMonth, st.wDay, st.wHour,
            st.wMinute, st.wSecond, GetTickCount64() / 1000.0);
    fprintf(f, "guest -> host offset %+lld
", (long long)g_xbox_mem_offset);
    XBOX_GAMEPAD g = hppoa_last_pad;
    fprintf(f, "pad buttons %04X analog", g.wButtons);
    for (int i = 0; i < 8; i++) fprintf(f, " %u", g.bAnalogButtons[i]);
    fprintf(f, " sticks %d %d %d %d
", g.sThumbLX, g.sThumbLY, g.sThumbRX, g.sThumbRY);
    fprintf(f, "trainer: input injections %u
", t->inject_seq);
    fclose(f);
}

static DWORD WINAPI report_thread(LPVOID arg)
{
    HppoaTrainerShm *t = (HppoaTrainerShm *)arg;
    for (;; Sleep(100)) {
        uint32_t seq = t->report_seq;
        if (seq == t->report_done) continue;
        char dir[260], path[300];
        memcpy(dir, (const char *)t->report_dir, sizeof dir);
        dir[sizeof dir - 1] = 0;
        hppoa_RequestCapture();
        snprintf(path, sizeof path, "%s\\stacks.txt", dir);
        hppoa_StackDumpTo(path);
        snprintf(path, sizeof path, "%s\\game_state.txt", dir);
        write_state(path, t);
        fprintf(stderr, "[TRAINER] debug report written to %s\n", dir);
        t->report_done = seq;
    }
    return 0;
}

/* at boot, so the trainer finds the block before the game is up */
void hppoa_TrainerInit(void)
{
    HppoaTrainerShm *t = shm();
    if (!t) return;
    fprintf(stderr, "[TRAINER] shared block %s ready
", HPPOA_TRAINER_NAME);
    CloseHandle(CreateThread(NULL, 0, report_thread, t, 0, NULL));
}
