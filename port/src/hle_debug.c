/**
 * Debug wrappers (log only; every call still runs the generated body).
 *
 * The title's file objects: open 0x159620 (path, flags) -> 0x2C-byte object,
 * close 0x1597E0 (object). A disk file keeps its 64 KB buffer at +0x28, an
 * archive (.hog) entry keeps the entry at +8. Starting a new game crashed in
 * close freeing +0x28 == 1 (logs/run7.log). Each open is recorded; a close of
 * an object that was never opened, closed twice, or whose +0/+4/+8/+0x28
 * changed since the open is reported as [FILEOBJ] with the object's bytes and
 * the guest call chain.
 *
 * RECOMP_CRAB_TRACE=1 logs [CRAB] for the fire crabs (AI type 0x31, update sub_00067380(entity), controller
 * [entity+0xDC]): each change of the crab's state (+0x70) with its timers (+0x74, +0xC0: seconds, counted up by the
 * frame time 0x21B7BC; it may start an attack from state 0, 1 or 7 once +0x74 >= 0.5 and +0xC0 > 5), its distance to
 * Harry and Harry's state ([harry+0xDC]+0x2564), and once a second the same for each crab within 2000 units.
 *
 * RECOMP_SCRIPT_TRACE=1 logs [SCRIPT] for every level-script command the title runs (sub_0014DD30 calls each
 * command's handler, entry + 0x20, from its table [0x356F90], 0x48 bytes an entry: the name inline at +0, the
 * argument count at +0x24; hppoa_ScriptCommand is put into the generated code just before by port/tools/patch_gen.py,
 * which regen.sh runs): the time, the
 * script context, the command, each argument (argv + 8 * i: the object it names, or its value [+4] -> a string or a
 * number) and Harry's position.
 *
 * Built into the recompile with --exclude-manual naming this file.
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define RECOMP_GENERATED_CODE
#include "recomp/gen/recomp_types.h"

#define MAX_OBJS 512

typedef struct {
    uint32_t obj, f0, f4, f8, f28;
    char path[96];
    int open;
} FileObj;

static FileObj g_objs[MAX_OBJS];

static FileObj *slot(uint32_t obj, int make)
{
    FileObj *free_slot = NULL;
    for (int i = 0; i < MAX_OBJS; i++) {
        if (g_objs[i].obj == obj) return &g_objs[i];
        if (!free_slot && !g_objs[i].open) free_slot = &g_objs[i];
    }
    return make ? free_slot : NULL;
}

static void guest_chain(uint32_t sp)
{
    int n = 0;
    fprintf(stderr, "  [FILEOBJ]   chain:");
    for (uint32_t i = 0; i < 128 && n < 10; i++) {
        uint32_t v = MEM32(sp + 4 * i);
        if (v >= 0x11005 && v < 0x190714 && (MEM8(v - 5) == 0xE8 || MEM8(v - 2) == 0xFF || MEM8(v - 6) == 0xFF)) {
            fprintf(stderr, " %08X", v);
            n++;
        }
    }
    fprintf(stderr, "\n");
}

/* file open(path, flags) -> object or 0 */
extern void sub_00159620_gen(void);
void sub_00159620(void)
{
    uint32_t path = MEM32(g_esp + 4), obj;
    char p[96] = "?";
    if (path) {
        for (int i = 0; i < 95; i++) {
            p[i] = (char)MEM8(path + i);
            if (!p[i]) break;
        }
        p[95] = 0;
    }
    sub_00159620_gen();
    obj = g_eax;
    if (obj) {
        FileObj *f = slot(obj, 1);
        if (f) {
            if (f->obj == obj && f->open)
                fprintf(stderr, "[FILEOBJ] open %08X (%s) returned an object still open as %s\n", obj, p, f->path);
            f->obj = obj;
            f->f0 = MEM32(obj);
            f->f4 = MEM32(obj + 4);
            f->f8 = MEM32(obj + 8);
            f->f28 = MEM32(obj + 0x28);
            strcpy(f->path, p);
            f->open = 1;
        }
    }
}

/* file close(object) */
extern void sub_001597E0_gen(void);
void sub_001597E0(void)
{
    uint32_t obj = MEM32(g_esp + 4);
    FileObj *f = obj ? slot(obj, 0) : NULL;
    if (obj) {
        uint32_t f0 = MEM32(obj), f4 = MEM32(obj + 4), f8 = MEM32(obj + 8), f28 = MEM32(obj + 0x28);
        const char *why = !f ? "never opened" : !f->open ? "closed twice" :
                          (f0 != f->f0 || f4 != f->f4 || f8 != f->f8 || f28 != f->f28) ? "fields changed" : NULL;
        if (why) {
            fprintf(stderr, "[FILEOBJ] close %08X: %s (%s)\n", obj, why, f ? f->path : "?");
            if (f)
                fprintf(stderr, "  [FILEOBJ]   at open: +0 %08X +4 %08X +8 %08X +28 %08X\n", f->f0, f->f4, f->f8, f->f28);
            fprintf(stderr, "  [FILEOBJ]   now:     +0 %08X +4 %08X +8 %08X +28 %08X, header [-4] %08X\n", f0, f4, f8, f28,
                    MEM32(obj - 4));
            fprintf(stderr, "  [FILEOBJ]   bytes:");
            for (int i = -4; i < 0x30; i += 4) fprintf(stderr, " %08X", MEM32(obj + i));
            fprintf(stderr, "\n");
            guest_chain(g_esp);
        }
        if (f) f->open = 0;
    }
    sub_001597E0_gen();
}

/* ── fire crabs ───────────────────────────────────────────────────────────── */
static int crab_trace_on(void)
{
    static int on = -1;
    if (on < 0) {
        const char *t = getenv("RECOMP_CRAB_TRACE");
        on = t && *t && *t != '0';
    }
    return on;
}

static int crab_ptr(uint32_t p) { return p >= 0x80000000u && p < 0x84000000u; }

/* an entity's world position (its object's current matrix), 0 without one */
static int crab_pos(uint32_t ent, float out[3])
{
    uint32_t obj = MEM32(ent + 0x28);
    if (!crab_ptr(obj)) return 0;
    uint32_t m = obj + 0x40 * (MEM32(obj + 0x2C) + 1);
    for (int i = 0; i < 3; i++) out[i] = MEMF(m + 0x30 + 4 * i);
    return 1;
}

/* fire crab update(entity) */
extern void sub_00067380_gen(void);
void sub_00067380(void)
{
    enum { kCrabs = 64 };
    static struct { uint32_t ent, state, last_ms; } seen[kCrabs];
    static int lines;
    uint32_t crab = MEM32(g_esp + 4);
    sub_00067380_gen();
    if (!crab_trace_on() || lines > 20000 || !crab_ptr(crab)) return;
    uint32_t ctrl = MEM32(crab + 0xDC), harry = MEM32(0x3565B0);
    if (!crab_ptr(ctrl) || !crab_ptr(harry)) return;
    int i = 0;
    while (i < kCrabs && seen[i].ent && seen[i].ent != crab) i++;
    if (i == kCrabs) return;
    if (!seen[i].ent) seen[i].ent = crab, seen[i].state = 0xFFFFFFFFu;
    float pc[3], ph[3], d = -1;
    if (crab_pos(crab, pc) && crab_pos(harry, ph))
        d = sqrtf((pc[0] - ph[0]) * (pc[0] - ph[0]) + (pc[1] - ph[1]) * (pc[1] - ph[1]) + (pc[2] - ph[2]) * (pc[2] - ph[2]));
    uint32_t st = MEM32(ctrl + 0x70), now = GetTickCount(), hctrl = MEM32(harry + 0xDC);
    int changed = st != seen[i].state;
    if (!changed && !(d >= 0 && d < 2000 && now - seen[i].last_ms >= 1000)) return;
    seen[i].state = st;
    seen[i].last_ms = now;
    lines++;
    uint32_t name = MEM32(crab + 0x0C);
    fprintf(stderr, "[CRAB] %9.2f s %-14s %s state %u | t74 %.2f tC0 %.2f | dist %.0f crab %.0f %.0f %.0f harry state 0x%X "
            "dt %.4f\n", now / 1000.0, crab_ptr(name) ? (const char *)XBOX_PTR(name) : "?", changed ? "->" : "  ", st,
            MEMF(ctrl + 0x74), MEMF(ctrl + 0xC0), d, d >= 0 ? pc[0] : 0, d >= 0 ? pc[1] : 0, d >= 0 ? pc[2] : 0,
            crab_ptr(hctrl) ? MEM32(hctrl + 0x2564) : 0, MEMF(0x21B7BC));
}

/* ── level scripts ────────────────────────────────────────────────────────── */
static int script_trace_on(void)
{
    static int on = -1;
    if (on < 0) {
        const char *t = getenv("RECOMP_SCRIPT_TRACE");
        on = t && *t && *t != '0';
    }
    return on;
}

/* a printable string at guest p (up to 40 characters), or 0 */
static const char *guest_str(uint32_t p)
{
    if (!((p >= 0x00011000u && p < 0x04000000u) || crab_ptr(p))) return 0;
    const char *c = (const char *)XBOX_PTR(p);
    int n = 0;
    while (n < 40 && c[n] >= 0x20 && c[n] < 0x7F) n++;
    return n > 0 && (n == 40 || c[n] == 0) ? c : 0;
}

extern void hppoa_DiffCutscene(void) __attribute__((weak));

/* a command's arguments as text: "a, "b", 3" (the object each one names, its string, or its number) */
static int format_args(uint32_t entry, uint32_t argv, char *buf, int size)
{
    uint32_t n = MEM32(entry + 0x24);
    int k = 0;
    buf[0] = 0;
    for (uint32_t i = 0; i < n && i < 8 && k < size - 64; i++) {
        uint32_t a = MEM32(argv + 8 * i), pv = MEM32(argv + 8 * i + 4);
        const char *sep = i ? ", " : "";
        uint32_t v = (pv >= 0x00011000u && pv < 0x04000000u) || crab_ptr(pv) ? MEM32(pv) : 0;
        const char *str = guest_str(v);
        float f;
        memcpy(&f, &v, 4);
        if (crab_ptr(a) && MEM32(a + 8) == a && guest_str(MEM32(a + 0x0C))) {
            /* an object: its name; a script variable is one too (named after its script group), so its value
             * follows unless the value is the object itself */
            k += snprintf(buf + k, size - k, "%s%s", sep, guest_str(MEM32(a + 0x0C)));
            if (!pv || pv == a || v == a) continue;
            sep = "=";
        }
        if (str) k += snprintf(buf + k, size - k, "%s\"%s\"", sep, str);
        else if (v && v < 0x01000000u) k += snprintf(buf + k, size - k, "%s%u", sep, v);  /* a whole number */
        else k += snprintf(buf + k, size - k, "%s%g", sep, (double)f);
    }
    return k;
}

/* ── speech ──────────────────────────────────────────────────────────────────
 * [SPEECH], with the debug logs on (RECOMP_DEBUG_LOGS=1, F10 > Debug) or RECOMP_SPEECH_TRACE=1: why a line is not
 * heard. The PlaySpeech command (handler 0x48220) plays nothing, by design, while
 *   - the cutscene flag [0x331161] is set: StartCutScene (0x4BC00) sets it as a cutscene starts and clears it only
 *     when the cutscene's time is up (or the counter [0x33FEFC] is set) and no speech plays; a cutscene whose
 *     StartCutScene is never called to its end leaves it set, and every later line is silent;
 *   - or the player object's +0x2C is 0x11 and sub_00150100() is 0x16 or 1, or Harry's state (+0x2564) is 0x23/0x24.
 * Speech itself: state [0x331770] (0 idle; IsSpeechPlaying is state != 0), stream [0x331670] / sound [0x33162C],
 * moved on by sub_00050C90 when the player reports the line finished. Logged: every PlaySpeech / Subtitle /
 * FadeOutSpeech / WaitForSpeech with those values, and each change of the flag, the speech state and the level (a
 * watcher thread, 20 Hz), naming the cutscene that set the flag. */
#define kCutsceneFlag 0x331161u
#define kSpeechState  0x331770u
#define kSpeechStream 0x331670u
#define kSpeechSound  0x33162Cu
#define kCutCounter   0x33FEFCu
#define kLevelNow     0x356CC4u

static char g_last_cut[96];      /* the last StartCutScene's arguments */
static char g_flag_cut[96];      /* the one that set the flag */
static DWORD g_flag_since;

static int speech_trace_on(void)
{
    static int on = -1;
    if (on < 0) {
        const char *t = getenv("RECOMP_SPEECH_TRACE"), *d = getenv("RECOMP_DEBUG_LOGS");
        on = (t && *t && *t != '0') || (d && !strcmp(d, "1"));
    }
    return on;
}

static void speech_state_text(char *buf, int size)
{
    uint32_t player = MEM32(0x3565B0), p2c = 0, hstate = 0;
    if (crab_ptr(player)) {
        p2c = MEM32(player + 0x2C);
        uint32_t ctrl = MEM32(player + 0xDC);
        if (crab_ptr(ctrl)) hstate = MEM32(ctrl + 0x2564);
    }
    snprintf(buf, size, "cutscene flag %u%s%s | speech state %u stream %08X sound %08X | counter %u level %u | "
             "player +2C 0x%X harry state 0x%X", MEM8(kCutsceneFlag), MEM8(kCutsceneFlag) ? " set by " : "",
             MEM8(kCutsceneFlag) ? g_flag_cut : "", MEM32(kSpeechState), MEM32(kSpeechStream), MEM32(kSpeechSound),
             MEM32(kCutCounter), MEM32(kLevelNow), p2c, hstate);
}

static DWORD WINAPI speech_watch(LPVOID unused)
{
    uint8_t flag = MEM8(kCutsceneFlag);
    uint32_t state = MEM32(kSpeechState), level = MEM32(kLevelNow);
    int warned = 0;
    (void)unused;
    for (;; Sleep(50)) {
        uint8_t f = MEM8(kCutsceneFlag);
        uint32_t s = MEM32(kSpeechState), l = MEM32(kLevelNow);
        DWORD now = GetTickCount();
        if (f != flag || s != state || l != level) {
            char st[256];
            if (f && !flag) {
                memcpy(g_flag_cut, g_last_cut, sizeof g_flag_cut);
                g_flag_since = now;
                warned = 0;
            }
            speech_state_text(st, sizeof st);
            fprintf(stderr, "[SPEECH] %9.2f s %s%s%s | %s\n", now / 1000.0,
                    f != flag ? (f ? "cutscene flag SET " : "cutscene flag cleared ") : "",
                    s != state ? "speech state changed " : "", l != level ? "level changed " : "", st);
            flag = f;
            state = s;
            level = l;
        }
        /* a cutscene is a minute or two at most: past five, the flag is stuck and every PlaySpeech is skipped */
        if (f && !warned && now - g_flag_since > 300000) {
            warned = 1;
            fprintf(stderr, "[SPEECH] %9.2f s cutscene flag set for %lu s by %s: ordinary speech is being skipped\n",
                    now / 1000.0, (unsigned long)((now - g_flag_since) / 1000), g_flag_cut);
        }
    }
    return 0;
}

/* the script contexts that have spoken (PlaySpeech / Subtitle): every command they run is logged too, so the branch
 * a script takes (a condition, a random roll) shows; per context, a line equal to its previous one (a wait that
 * repeats every frame) is not repeated */
#define kSpeechCtxs 32
static struct { uint32_t ctx; char last[400]; } g_sctx[kSpeechCtxs];

static int speech_ctx(uint32_t ctx, int add)
{
    for (int i = 0; i < kSpeechCtxs; i++)
        if (g_sctx[i].ctx == ctx) return i;
    if (!add) return -1;
    for (int i = 0; i < kSpeechCtxs; i++)
        if (!g_sctx[i].ctx) {
            g_sctx[i].ctx = ctx;
            g_sctx[i].last[0] = 0;
            return i;
        }
    return -1;
}

static void speech_command(const char *name, uint32_t entry, uint32_t ctx, uint32_t argv)
{
    static LONG started;
    char args[256], st[256], line[400];
    if (!InterlockedExchange(&started, 1)) {
        speech_state_text(st, sizeof st);
        fprintf(stderr, "[SPEECH] trace on | %s\n", st);
        CloseHandle(CreateThread(NULL, 0, speech_watch, NULL, 0, NULL));
    }
    if (!strcmp(name, "StartCutScene")) {  /* called every frame until the cutscene ends: remembered, not logged */
        format_args(entry, argv, args, sizeof args);
        snprintf(g_last_cut, sizeof g_last_cut, "StartCutScene(%s)", args);
        return;
    }
    int speech = !strcmp(name, "PlaySpeech") || !strcmp(name, "Subtitle") || !strcmp(name, "FadeOutSpeech") ||
                 !strcmp(name, "WaitForSpeech");
    int i = speech_ctx(ctx, !strcmp(name, "PlaySpeech") || !strcmp(name, "Subtitle"));
    if (i < 0 && !speech) return;
    format_args(entry, argv, args, sizeof args);
    if (speech) {
        speech_state_text(st, sizeof st);
        snprintf(line, sizeof line, "%s(%s) ctx %08X | %s%s", name, args, ctx, st,
                 !strcmp(name, "PlaySpeech") && MEM8(kCutsceneFlag) ? " | SKIPPED: cutscene flag set" : "");
    } else {
        snprintf(line, sizeof line, "  ctx %08X %s(%s)", ctx, name, args);  /* a speaking script's other commands */
    }
    if (i >= 0) {
        if (!strcmp(line, g_sctx[i].last)) return;
        memcpy(g_sctx[i].last, line, sizeof g_sctx[i].last);
    }
    fprintf(stderr, "[SPEECH] %9.2f s %s\n", GetTickCount() / 1000.0, line);
}

void hppoa_ScriptCommand(uint32_t entry, uint32_t ctx, uint32_t argv)
{
    static int lines;
    if (!crab_ptr(entry)) return;
    const char *name = (const char *)XBOX_PTR(entry);
    /* the difftest build sweeps each cutscene by itself (StartCutScene repeats for a few frames: once a minute) */
    if (hppoa_DiffCutscene && !strcmp(name, "StartCutScene")) {
        static DWORD last;
        DWORD now = GetTickCount();
        if (!last || now - last > 60000) { last = now; hppoa_DiffCutscene(); }
    }
    if (speech_trace_on()) speech_command(name, entry, ctx, argv);
    if (!script_trace_on() || lines > 400000) return;
    lines++;
    uint32_t harry = MEM32(0x3565B0);
    float ph[3] = {0, 0, 0};
    if (crab_ptr(harry)) crab_pos(harry, ph);
    char args[400];
    format_args(entry, argv, args, sizeof args);
    fprintf(stderr, "[SCRIPT] %9.2f s ctx %08X %.32s(%s) | harry %.0f %.0f %.0f\n", GetTickCount() / 1000.0, ctx, name,
            args, ph[0], ph[1], ph[2]);
}
