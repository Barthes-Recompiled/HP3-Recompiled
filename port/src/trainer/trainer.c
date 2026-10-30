/*
 * hppoa_trainer.exe: the debug trainer. Runs beside hppoa.exe (start either
 * first) and talks to it through the shared
 * block in trainer_shm.h; the game carries the requests out on its own thread
 * (hle_trainer.c).
 *
 *   Noclip         Harry flies: left stick along the camera, RT up, LT down,
 *                  A held 4x as fast; walls, doors and floors do not stop him
 *   Infinite health
 *   Spells         any spell on any of the three buttons, learnt or not
 *   Collection     every wizard card (the title's way: a health bar each 20th), health bars up to 6, beans,
 *                  the big bean bag -- and taking them away again, or Undo: all back as before the first change
 *   Positions      three saved spots to jump back to
 *   Insta-kill     an enemy dies on the first hit (its CurrentHealth goes to 0)
 *   Levels         load any story level with the progress as it is
 *   Checkpoints    the progress (all a save keeps) as each level starts, in run\checkpoints; Restore puts it
 *                  back and loads that level
 *   Debug report   run\reports\<time>: guest memory, the progress block, logs (copied from here, so a hung
 *                  game still gives them), every thread's stack, the game's state and a frame capture (from
 *                  the game)
 *
 * Hotkeys (work while the game has focus): Ctrl+Shift+N noclip,
 * Ctrl+Shift+G infinite health, Ctrl+Shift+K insta-kill, Ctrl+Shift+1..3 save
 * spot, Ctrl+1..3 go, Ctrl+Shift+R debug report.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../trainer_shm.h"

enum {
    ID_TIMER = 1, ID_NOCLIP = 100, ID_SPEED, ID_GOD, ID_SLOT0, ID_SLOT1, ID_SLOT2, ID_APPLY, ID_READ,
    ID_SAVE0 = 120, ID_GO0 = 130, ID_STATUS = 140, ID_POS, ID_SPELLS, ID_SPOT0 = 150,
    ID_CARDS = 160, ID_BAR, ID_BARS_MAX, ID_BEANS, ID_BAG, ID_COLLECT,
    ID_CARDS_GIVEN_RM = 170, ID_CARDS_ALL_RM, ID_BAR_RM, ID_BEANS_VALUE, ID_BEANS_SET, ID_BAG_SMALL, ID_UNDO,
    ID_REPORT = 180, ID_REPORT_MEM, ID_REPORT_STATUS,
    ID_INSTAKILL = 190, ID_LEVEL_NOW, ID_LEVELS, ID_LOAD_LEVEL, ID_CHECKPOINTS, ID_RESTORE, ID_SNAP_NOW, ID_CP_STATUS,
    HK_NOCLIP = 1, HK_GOD, HK_SAVE0, HK_GO0 = 10, HK_REPORT = 20, HK_INSTAKILL,
    WM_REPORT_DONE = WM_APP + 1,
};

static const struct { const char *name; uint8_t id; } kSpells[] = {
    {"(none)", kSpellNone},       {"Flipendo", kSpellFlipendo}, {"Lumos", kSpellLumos},
    {"Diffindo", kSpellDiffindo}, {"Spongify", kSpellSpongify}, {"Incendio", kSpellIncendio},
    {"Alohomora", kSpellAlohomora},
};
#define NSPELLS (int)(sizeof(kSpells) / sizeof(kSpells[0]))

/* the story levels, ids 6..30 (Philosopher's Stone's; PoA's level list is not found yet) */
static const char *const kLevelFiles[] = {
    "day0", "night0", "day1", "day1lumos", "night1", "day2", "day2spongify", "night2", "night2troll", "day3",
    "day3diffindo", "night3", "night3library", "day4", "day4incendio", "night4", "night4ingredients", "day5",
    "day5quidditch", "night5", "night5devil", "night5keys", "night5chess", "night5voldemort", "night5avoldemort",
    "day6",
};
static const char *const kLevelNotes[] = {
    "", "", "", "Lumos challenge", "", "", "Spongify challenge", "", "", "",
    "Diffindo challenge (troll)", "", "Restricted Section", "", "Incendio challenge", "", "", "",
    "Quidditch", "", "Devil's Snare", "flying keys", "wizard chess", "", "", "",
};
#define NLEVELS (int)(sizeof(kLevelFiles) / sizeof(kLevelFiles[0]))

static const char *level_file(uint32_t id)
{
    return id >= HPPOA_LEVEL_FIRST && id - HPPOA_LEVEL_FIRST < (uint32_t)NLEVELS ? kLevelFiles[id - HPPOA_LEVEL_FIRST]
                                                                              : "?";
}

static HppoaTrainerShm *g_t;
static HWND g_wnd, g_noclip, g_speed, g_god, g_slot[3], g_status, g_pos, g_spells, g_spot[3], g_collect, g_undo;
static HWND g_beans_value, g_report_mem, g_report_status, g_report_btn;
static HWND g_instakill, g_level_now, g_levels, g_checkpoints, g_cp_status;
static float g_spots[3][3];
static int g_spot_set[3];
static uint32_t g_last_frame, g_stale;
static char g_dir[MAX_PATH];  /* where this exe is: run\ */
static volatile LONG g_reporting;
static char g_report_msg[512];

static const char *spell_name(uint8_t id)
{
    for (int i = 0; i < NSPELLS; i++)
        if (kSpells[i].id == id) return kSpells[i].name;
    return "?";
}

static int attach(void)
{
    if (g_t) {
        /* the game restarted with a new block: let go of the old one */
        if (g_t->magic == HPPOA_TRAINER_MAGIC) return 1;
        UnmapViewOfFile(g_t);
        g_t = NULL;
    }
    HANDLE m = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, HPPOA_TRAINER_NAME);
    if (!m) return 0;
    HppoaTrainerShm *t = (HppoaTrainerShm *)MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(HppoaTrainerShm));
    CloseHandle(m);
    if (!t) return 0;
    if (t->magic != HPPOA_TRAINER_MAGIC || t->version != HPPOA_TRAINER_VERSION) {
        UnmapViewOfFile(t);
        return 0;
    }
    g_t = t;
    return 1;
}

static void push_settings(void)
{
    if (!attach()) return;
    char buf[32];
    GetWindowTextA(g_speed, buf, sizeof buf);
    g_t->fly_speed = (float)atof(buf);
    g_t->noclip = SendMessageA(g_noclip, BM_GETCHECK, 0, 0) == BST_CHECKED;
    g_t->god = SendMessageA(g_god, BM_GETCHECK, 0, 0) == BST_CHECKED;
    g_t->instakill = SendMessageA(g_instakill, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

static void apply_spells(void)
{
    if (!attach()) return;
    for (int i = 0; i < 3; i++) {
        int sel = (int)SendMessageA(g_slot[i], CB_GETCURSEL, 0, 0);
        g_t->req_slots[i] = sel >= 0 ? kSpells[sel].id : 0;
    }
    g_t->slots_seq++;
}

static void read_spells(void)
{
    if (!attach() || !g_t->player) return;
    for (int i = 0; i < 3; i++)
        for (int k = 0; k < NSPELLS; k++)
            if (kSpells[k].id == g_t->slots[i]) SendMessageA(g_slot[i], CB_SETCURSEL, k, 0);
}

static void show_spot(int i)
{
    char b[96];
    if (g_spot_set[i]) snprintf(b, sizeof b, "%.0f  %.0f  %.0f", g_spots[i][0], g_spots[i][1], g_spots[i][2]);
    else snprintf(b, sizeof b, "(empty)");
    SetWindowTextA(g_spot[i], b);
}

static void save_spot(int i)
{
    if (!attach() || !g_t->player) return;
    for (int k = 0; k < 3; k++) g_spots[i][k] = g_t->pos[k];
    g_spot_set[i] = 1;
    show_spot(i);
}

static void go_spot(int i)
{
    if (!attach() || !g_spot_set[i]) return;
    for (int k = 0; k < 3; k++) g_t->tp_pos[k] = g_spots[i][k];
    g_t->tp_seq++;
}

static void toggle(HWND box)
{
    SendMessageA(box, BM_SETCHECK, SendMessageA(box, BM_GETCHECK, 0, 0) == BST_CHECKED ? BST_UNCHECKED : BST_CHECKED, 0);
    push_settings();
}

/* ── levels and checkpoints ───────────────────────────────────────────────── */

typedef struct {
    uint32_t magic, version, level, size;
} CheckpointHeader;
#define CHECKPOINT_MAGIC 0x4B435048u  /* "HPCK" */

static uint8_t g_last_block[HPPOA_PROGRESS_SIZE];
static int g_have_last;
static uint32_t g_snap_seen;

static void load_level(void)
{
    if (!attach()) return;
    int sel = (int)SendMessageA(g_levels, CB_GETCURSEL, 0, 0);
    if (sel < 0) return;
    g_t->warp_level = HPPOA_LEVEL_FIRST + (uint32_t)sel;
    g_t->warp_seq++;
}

/* run\checkpoints, newest first */
static void list_checkpoints(void)
{
    char pat[MAX_PATH];
    snprintf(pat, sizeof pat, "%s\\checkpoints\\*.bin", g_dir);
    SendMessageA(g_checkpoints, LB_RESETCONTENT, 0, 0);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        /* names start with the time, so the reverse order is the newest first */
        char name[MAX_PATH];
        snprintf(name, sizeof name, "%s", fd.cFileName);
        char *dot = strrchr(name, '.');
        if (dot) *dot = 0;
        SendMessageA(g_checkpoints, LB_INSERTSTRING, 0, (LPARAM)name);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

/* a checkpoint the game handed over: filed unless it is the same as the last one */
static void take_snapshot(void)
{
    if (!g_t || g_t->snap_seq == g_snap_seen) return;
    g_snap_seen = g_t->snap_seq;
    uint32_t level = g_t->snap_level;
    static uint8_t block[HPPOA_PROGRESS_SIZE];
    for (uint32_t i = 0; i < HPPOA_PROGRESS_SIZE; i++) block[i] = g_t->snap_block[i];
    if (g_have_last && !memcmp(block, g_last_block, sizeof block)) return;
    memcpy(g_last_block, block, sizeof block);
    g_have_last = 1;
    SYSTEMTIME st;
    GetLocalTime(&st);
    char path[MAX_PATH];
    snprintf(path, sizeof path, "%s\\checkpoints", g_dir);
    CreateDirectoryA(path, NULL);
    snprintf(path, sizeof path, "%s\\checkpoints\\%04u%02u%02u_%02u%02u%02u_%s.bin", g_dir, st.wYear, st.wMonth,
             st.wDay, st.wHour, st.wMinute, st.wSecond, level_file(level));
    FILE *f = fopen(path, "wb");
    if (!f) return;
    CheckpointHeader hd = {CHECKPOINT_MAGIC, 1, level, HPPOA_PROGRESS_SIZE};
    fwrite(&hd, sizeof hd, 1, f);
    fwrite(block, 1, sizeof block, f);
    fclose(f);
    list_checkpoints();
}

static void restore_checkpoint(void)
{
    if (!attach()) return;
    int sel = (int)SendMessageA(g_checkpoints, LB_GETCURSEL, 0, 0);
    if (sel < 0) {
        SetWindowTextA(g_cp_status, "Pick a checkpoint first");
        return;
    }
    char name[MAX_PATH], path[MAX_PATH];
    SendMessageA(g_checkpoints, LB_GETTEXT, sel, (LPARAM)name);
    snprintf(path, sizeof path, "%s\\checkpoints\\%s.bin", g_dir, name);
    FILE *f = fopen(path, "rb");
    CheckpointHeader hd;
    static uint8_t block[HPPOA_PROGRESS_SIZE];
    int ok = f && fread(&hd, sizeof hd, 1, f) == 1 && hd.magic == CHECKPOINT_MAGIC && hd.size == HPPOA_PROGRESS_SIZE &&
             fread(block, 1, sizeof block, f) == sizeof block && hd.level >= HPPOA_LEVEL_FIRST &&
             hd.level <= HPPOA_LEVEL_LAST;
    if (f) fclose(f);
    if (!ok) {
        SetWindowTextA(g_cp_status, "That file is not a checkpoint");
        return;
    }
    for (uint32_t i = 0; i < HPPOA_PROGRESS_SIZE; i++) g_t->restore_block[i] = block[i];
    g_t->restore_level = hd.level;
    g_t->restore_seq++;
    char b[MAX_PATH + 32];
    snprintf(b, sizeof b, "Restoring %s", name);
    SetWindowTextA(g_cp_status, b);
}

/* ── debug report ─────────────────────────────────────────────────────────── */

static void report_line(FILE *f, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
}

/* guest memory through ReadProcessMemory, page by page (holes read as zero) */
static size_t dump_guest(HANDLE h, uint64_t off, uint32_t va, uint32_t size, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    static unsigned char page[65536];
    size_t got = 0;
    for (uint32_t o = 0; o < size; o += sizeof page) {
        uint32_t n = size - o < sizeof page ? size - o : sizeof page;
        SIZE_T r = 0;
        memset(page, 0, n);
        if (!ReadProcessMemory(h, (void *)(uintptr_t)(va + o + off), page, n, &r)) {
            /* a hole inside: smaller pieces */
            for (uint32_t k = 0; k < n; k += 4096) {
                SIZE_T rr = 0;
                if (ReadProcessMemory(h, (void *)(uintptr_t)(va + o + k + off), page + k, 4096, &rr)) r += rr;
            }
        }
        got += r;
        fwrite(page, 1, n, f);
    }
    fclose(f);
    return got;
}

static int copy_in(const char *from_rel, const char *dir, const char *name)
{
    char from[MAX_PATH], to[MAX_PATH];
    snprintf(from, sizeof from, "%s\\%s", g_dir, from_rel);
    snprintf(to, sizeof to, "%s\\%s", dir, name);
    /* the logs are still being written: read them shared */
    HANDLE in = CreateFileA(from, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                            OPEN_EXISTING, 0, NULL);
    if (in == INVALID_HANDLE_VALUE) return 0;
    FILE *out = fopen(to, "wb");
    if (!out) { CloseHandle(in); return 0; }
    static char buf[65536];
    DWORD r;
    while (ReadFile(in, buf, sizeof buf, &r, NULL) && r) fwrite(buf, 1, r, out);
    fclose(out);
    CloseHandle(in);
    return 1;
}

/* the newest folder in run\captures made since `since` (a capture the game wrote for the report) */
static int newest_capture(FILETIME since, char *name, size_t len)
{
    char pat[MAX_PATH];
    snprintf(pat, sizeof pat, "%s\\captures\\*", g_dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    FILETIME best = since;
    int found = 0;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == '.') continue;
        if (CompareFileTime(&fd.ftCreationTime, &best) >= 0) {
            best = fd.ftCreationTime;
            snprintf(name, len, "%s", fd.cFileName);
            found = 1;
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return found;
}

static DWORD WINAPI report_worker(LPVOID arg)
{
    int with_memory = (int)(INT_PTR)arg;
    SYSTEMTIME st;
    GetLocalTime(&st);
    char stamp[32], dir[MAX_PATH], path[MAX_PATH];
    snprintf(stamp, sizeof stamp, "%04u%02u%02u_%02u%02u%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
             st.wSecond);
    snprintf(path, sizeof path, "%s\\reports", g_dir);
    CreateDirectoryA(path, NULL);
    snprintf(dir, sizeof dir, "%s\\reports\\%s", g_dir, stamp);
    CreateDirectoryA(dir, NULL);
    FILETIME t0;
    GetSystemTimeAsFileTime(&t0);

    snprintf(path, sizeof path, "%s\\report.txt", dir);
    FILE *f = fopen(path, "w");
    if (!f) {
        snprintf(g_report_msg, sizeof g_report_msg, "Could not create %s", dir);
        PostMessageA(g_wnd, WM_REPORT_DONE, 0, 0);
        return 0;
    }
    report_line(f, "hppoa debug report %s", stamp);
    int game = attach();
    uint32_t asked = 0;
    if (game) {
        HppoaTrainerShm *t = g_t;
        report_line(f, "game pid %u, guest -> host offset %+lld", t->pid, (long long)t->mem_offset);
        report_line(f, "Harry's updates %u, player %08X, position %.2f %.2f %.2f%s", t->frame, t->player, t->pos[0],
                    t->pos[1], t->pos[2], t->noclip_on ? ", noclip" : "");
        report_line(f, "health %.1f / %.1f, cards %u (trainer gave %u), beans %d / %d, buttons %s %s %s", t->health,
                    t->health_max, t->cards, t->cards_given, t->beans, t->beans_max, spell_name(t->slots[0]),
                    spell_name(t->slots[1]), spell_name(t->slots[2]));
        report_line(f, "trainer settings: noclip %u god %u fly speed %.0f, undo kept %u", t->noclip, t->god,
                    t->fly_speed, t->undo_ready);
        /* the game's part, on its own thread */
        snprintf((char *)t->report_dir, sizeof t->report_dir, "%s", dir);
        asked = ++t->report_seq;
    } else {
        report_line(f, "game: no shared block (hppoa.exe not running, or an older build)");
    }

    if (game && with_memory) {
        HANDLE h = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, g_t->pid);
        if (h) {
            uint64_t off = g_t->mem_offset;
            snprintf(path, sizeof path, "%s\\guest_00000000.bin", dir);
            size_t b = dump_guest(h, off, 0x00000000u, 0x04000000u, path);
            snprintf(path, sizeof path, "%s\\guest_80000000.bin", dir);
            size_t c = dump_guest(h, off, 0x80000000u, 0x04000000u, path);
            report_line(f, "memory: guest 0x00000000 %zu MB, guest 0x80000000 %zu MB read", b >> 20, c >> 20);
            CloseHandle(h);
        } else {
            report_line(f, "memory: could not open the game (error %lu)", GetLastError());
        }
    }

    report_line(f, "logs: last_run.log %s, hppoa_run.log %s, xbox_kernel.log %s, hppoa.toml %s",
                copy_in("..\\logs\\last_run.log", dir, "last_run.log") ? "copied" : "missing",
                copy_in("hppoa_run.log", dir, "hppoa_run.log") ? "copied" : "missing",
                copy_in("xbox_kernel.log", dir, "xbox_kernel.log") ? "copied" : "missing",
                copy_in("hppoa.toml", dir, "hppoa.toml") ? "copied" : "missing");

    if (game) {
        int answered = 0;
        for (int i = 0; i < 100 && !answered; i++, Sleep(50)) answered = g_t->report_done == asked;
        report_line(f, "game's part (stacks.txt, game_state.txt): %s",
                    answered ? "written" : "NO ANSWER in 5 s - the game's helper thread is stuck too");
        /* the capture comes with the next frame the game presents */
        char cap[MAX_PATH] = "";
        for (int i = 0; i < 40 && !newest_capture(t0, cap, sizeof cap); i++) Sleep(100);
        if (cap[0]) {
            Sleep(1500);  /* its textures and shaders are still being written */
            char rel[MAX_PATH];
            snprintf(rel, sizeof rel, "captures\\%s\\final.bmp", cap);
            copy_in(rel, dir, "screen.bmp");
            snprintf(rel, sizeof rel, "captures\\%s\\frame.txt", cap);
            copy_in(rel, dir, "frame.txt");
            report_line(f, "frame capture: run\\captures\\%s (screen.bmp, frame.txt copied)", cap);
        } else {
            report_line(f, "frame capture: none (the game presented no frame: paused in a load or hung)");
        }
    }
    fclose(f);
    snprintf(g_report_msg, sizeof g_report_msg, "Saved: run\\reports\\%s - tell Claude this name", stamp);
    PostMessageA(g_wnd, WM_REPORT_DONE, 0, 0);
    return 0;
}

static void start_report(void)
{
    if (InterlockedExchange(&g_reporting, 1)) return;
    EnableWindow(g_report_btn, FALSE);
    SetWindowTextA(g_report_status, "Collecting...");
    int mem = SendMessageA(g_report_mem, BM_GETCHECK, 0, 0) == BST_CHECKED;
    CloseHandle(CreateThread(NULL, 0, report_worker, (LPVOID)(INT_PTR)mem, 0, NULL));
}

/* ── window ───────────────────────────────────────────────────────────────── */

static void refresh(void)
{
    char b[256];
    if (!attach()) {
        SetWindowTextA(g_status, "Waiting for hppoa.exe...");
        return;
    }
    if (g_t->frame != g_last_frame) g_last_frame = g_t->frame, g_stale = 0;
    else g_stale++;
    if (!g_t->player) snprintf(b, sizeof b, "Connected - Harry not in play yet");
    else snprintf(b, sizeof b, "Connected - %s%s", g_stale > 5 ? "paused / loading" : "in play",
                  g_t->noclip_on ? " - NOCLIP" : "");
    SetWindowTextA(g_status, b);
    snprintf(b, sizeof b, "Position %.0f  %.0f  %.0f     Health %.0f / %.0f", g_t->pos[0], g_t->pos[1], g_t->pos[2],
             g_t->health, g_t->health_max);
    SetWindowTextA(g_pos, b);
    snprintf(b, sizeof b, "On the buttons now: %s, %s, %s", spell_name(g_t->slots[0]), spell_name(g_t->slots[1]),
             spell_name(g_t->slots[2]));
    SetWindowTextA(g_spells, b);
    snprintf(b, sizeof b, "Wizard cards %u / 101 (%u from trainer)   Health bars %.0f / 6   Beans %d / %d",
             g_t->cards, g_t->cards_given, g_t->health_max / 100.0f, g_t->beans, g_t->beans_max);
    SetWindowTextA(g_collect, b);
    EnableWindow(g_undo, g_t->undo_ready);
    uint32_t lv = g_t->level;
    snprintf(b, sizeof b, "Level now: %u %s", lv, level_file(lv));
    SetWindowTextA(g_level_now, b);
    take_snapshot();
    /* the game restarted: send the settings again */
    if (g_t->noclip != (SendMessageA(g_noclip, BM_GETCHECK, 0, 0) == BST_CHECKED) ||
        g_t->god != (SendMessageA(g_god, BM_GETCHECK, 0, 0) == BST_CHECKED) ||
        g_t->instakill != (SendMessageA(g_instakill, BM_GETCHECK, 0, 0) == BST_CHECKED))
        push_settings();
}

static HWND add(const char *cls, const char *text, DWORD style, int x, int y, int w, int h, int id)
{
    HWND c = CreateWindowExA(0, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, g_wnd,
                             (HMENU)(INT_PTR)id, GetModuleHandleA(NULL), NULL);
    SendMessageA(c, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    return c;
}

static int build(void)
{
    int y = 10;
    g_status = add("STATIC", "", 0, 10, y, 420, 18, ID_STATUS);
    y += 20;
    g_pos = add("STATIC", "", 0, 10, y, 420, 18, ID_POS);
    y += 28;
    add("BUTTON", "Movement", BS_GROUPBOX, 5, y, 430, 70, 0);
    g_noclip = add("BUTTON", "Noclip / fly  (Ctrl+Shift+N)", BS_AUTOCHECKBOX, 15, y + 20, 220, 20, ID_NOCLIP);
    add("STATIC", "Speed", 0, 250, y + 22, 40, 18, 0);
    g_speed = add("EDIT", "600", WS_BORDER | ES_NUMBER, 295, y + 20, 60, 20, ID_SPEED);
    add("STATIC", "Left stick: move   RT/LT: up/down   A: fast", 0, 15, y + 45, 400, 18, 0);
    y += 78;
    g_god = add("BUTTON", "Infinite health  (Ctrl+Shift+G)", BS_AUTOCHECKBOX, 15, y, 300, 20, ID_GOD);
    y += 28;

    add("BUTTON", "Collection", BS_GROUPBOX, 5, y, 430, 196, 0);
    g_collect = add("STATIC", "", 0, 15, y + 20, 415, 18, ID_COLLECT);
    add("BUTTON", "Give all cards", BS_PUSHBUTTON, 15, y + 42, 130, 24, ID_CARDS);
    add("BUTTON", "+1 health bar", BS_PUSHBUTTON, 155, y + 42, 130, 24, ID_BAR);
    add("BUTTON", "Max health bars", BS_PUSHBUTTON, 295, y + 42, 130, 24, ID_BARS_MAX);
    add("BUTTON", "Fill beans", BS_PUSHBUTTON, 15, y + 70, 130, 24, ID_BEANS);
    add("BUTTON", "Big bean bag (150)", BS_PUSHBUTTON, 155, y + 70, 130, 24, ID_BAG);
    add("STATIC", "Take away:", 0, 15, y + 104, 100, 18, 0);
    add("BUTTON", "Trainer's cards", BS_PUSHBUTTON, 15, y + 122, 130, 24, ID_CARDS_GIVEN_RM);
    add("BUTTON", "All cards", BS_PUSHBUTTON, 155, y + 122, 130, 24, ID_CARDS_ALL_RM);
    add("BUTTON", "-1 health bar", BS_PUSHBUTTON, 295, y + 122, 130, 24, ID_BAR_RM);
    g_beans_value = add("EDIT", "0", WS_BORDER | ES_NUMBER, 15, y + 152, 50, 22, ID_BEANS_VALUE);
    add("BUTTON", "Set beans", BS_PUSHBUTTON, 70, y + 151, 75, 24, ID_BEANS_SET);
    add("BUTTON", "Small bean bag (50)", BS_PUSHBUTTON, 155, y + 151, 130, 24, ID_BAG_SMALL);
    g_undo = add("BUTTON", "Undo all changes", BS_PUSHBUTTON, 295, y + 151, 130, 24, ID_UNDO);
    y += 204;

    add("BUTTON", "Spells on the buttons (learnt or not)", BS_GROUPBOX, 5, y, 430, 100, 0);
    for (int i = 0; i < 3; i++) {
        char l[16];
        snprintf(l, sizeof l, "Button %d", i + 1);
        add("STATIC", l, 0, 15 + i * 140, y + 20, 120, 18, 0);
        g_slot[i] = add("COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL, 15 + i * 140, y + 38, 130, 200, ID_SLOT0 + i);
        for (int k = 0; k < NSPELLS; k++) SendMessageA(g_slot[i], CB_ADDSTRING, 0, (LPARAM)kSpells[k].name);
        SendMessageA(g_slot[i], CB_SETCURSEL, 0, 0);
    }
    add("BUTTON", "Apply", BS_PUSHBUTTON, 15, y + 68, 80, 24, ID_APPLY);
    add("BUTTON", "Read from game", BS_PUSHBUTTON, 105, y + 68, 110, 24, ID_READ);
    y += 108;
    g_spells = add("STATIC", "", 0, 10, y, 420, 18, 0);
    y += 26;

    add("BUTTON", "Saved positions", BS_GROUPBOX, 5, y, 430, 108, 0);
    for (int i = 0; i < 3; i++) {
        char l[32];
        int yy = y + 20 + i * 28;
        snprintf(l, sizeof l, "Save %d", i + 1);
        add("BUTTON", l, BS_PUSHBUTTON, 15, yy, 60, 24, ID_SAVE0 + i);
        snprintf(l, sizeof l, "Go %d", i + 1);
        add("BUTTON", l, BS_PUSHBUTTON, 80, yy, 60, 24, ID_GO0 + i);
        g_spot[i] = add("STATIC", "", 0, 150, yy + 4, 270, 18, ID_SPOT0 + i);
        show_spot(i);
    }
    y += 112;
    add("STATIC", "Ctrl+Shift+1..3 save, Ctrl+1..3 go (also from the game)", 0, 10, y, 420, 18, 0);
    y += 26;

    add("BUTTON", "Problem? Debug report for Claude", BS_GROUPBOX, 5, y, 430, 92, 0);
    g_report_btn = add("BUTTON", "Collect debug info  (Ctrl+Shift+R)", BS_PUSHBUTTON, 15, y + 20, 230, 26, ID_REPORT);
    g_report_mem = add("BUTTON", "With memory (128 MB)", BS_AUTOCHECKBOX, 255, y + 23, 170, 20, ID_REPORT_MEM);
    SendMessageA(g_report_mem, BM_SETCHECK, BST_CHECKED, 0);
    g_report_status = add("STATIC", "Stacks, game state, a frame capture, logs and memory into run\\reports",
                          0, 15, y + 54, 415, 32, ID_REPORT_STATUS);
    y += 100;

    /* the second column */
    int x = 445, ry = 58;
    add("BUTTON", "Combat", BS_GROUPBOX, x, ry, 430, 50, 0);
    g_instakill = add("BUTTON", "Insta-kill: enemies die on the first hit  (Ctrl+Shift+K)", BS_AUTOCHECKBOX, x + 10,
                      ry + 20, 400, 20, ID_INSTAKILL);
    ry += 58;
    add("BUTTON", "Levels (loads with your progress as it is now)", BS_GROUPBOX, x, ry, 430, 86, 0);
    g_level_now = add("STATIC", "", 0, x + 10, ry + 20, 410, 18, ID_LEVEL_NOW);
    g_levels = add("COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL, x + 10, ry + 46, 300, 400, ID_LEVELS);
    for (int i = 0; i < NLEVELS; i++) {
        char l[96];
        snprintf(l, sizeof l, "%2d  %s%s%s", (int)HPPOA_LEVEL_FIRST + i, kLevelFiles[i], kLevelNotes[i][0] ? " - " : "",
                 kLevelNotes[i]);
        SendMessageA(g_levels, CB_ADDSTRING, 0, (LPARAM)l);
    }
    SendMessageA(g_levels, CB_SETCURSEL, 11, 0);  /* night3, before the Restricted Section */
    add("BUTTON", "Load level", BS_PUSHBUTTON, x + 320, ry + 45, 100, 24, ID_LOAD_LEVEL);
    ry += 94;
    add("BUTTON", "Checkpoints (kept as each level starts)", BS_GROUPBOX, x, ry, 430, 420, 0);
    g_checkpoints = add("LISTBOX", "", WS_BORDER | WS_VSCROLL | LBS_NOTIFY, x + 10, ry + 20, 410, 320, ID_CHECKPOINTS);
    add("BUTTON", "Restore selected", BS_PUSHBUTTON, x + 10, ry + 344, 130, 26, ID_RESTORE);
    add("BUTTON", "Checkpoint now", BS_PUSHBUTTON, x + 150, ry + 344, 130, 26, ID_SNAP_NOW);
    g_cp_status = add("STATIC", "Restore puts the progress back (health, cards, story) and loads its level",
                      0, x + 10, ry + 378, 410, 34, ID_CP_STATUS);
    list_checkpoints();
    return y;
}

static LRESULT CALLBACK proc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        g_wnd = w;
        build();
        RegisterHotKey(w, HK_NOCLIP, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'N');
        RegisterHotKey(w, HK_GOD, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'G');
        RegisterHotKey(w, HK_REPORT, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'R');
        RegisterHotKey(w, HK_INSTAKILL, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'K');
        for (int i = 0; i < 3; i++) {
            RegisterHotKey(w, HK_SAVE0 + i, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, '1' + i);
            RegisterHotKey(w, HK_GO0 + i, MOD_CONTROL | MOD_NOREPEAT, '1' + i);
        }
        SetTimer(w, ID_TIMER, 100, NULL);
        return 0;
    case WM_TIMER:
        refresh();
        return 0;
    case WM_REPORT_DONE:
        SetWindowTextA(g_report_status, g_report_msg);
        EnableWindow(g_report_btn, TRUE);
        InterlockedExchange(&g_reporting, 0);
        return 0;
    case WM_HOTKEY:
        if (wp == HK_NOCLIP) toggle(g_noclip);
        else if (wp == HK_GOD) toggle(g_god);
        else if (wp == HK_REPORT) start_report();
        else if (wp == HK_INSTAKILL) toggle(g_instakill);
        else if (wp >= HK_SAVE0 && wp < HK_SAVE0 + 3) save_spot((int)wp - HK_SAVE0);
        else if (wp >= HK_GO0 && wp < HK_GO0 + 3) go_spot((int)wp - HK_GO0);
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(wp), code = HIWORD(wp);
        if (id == ID_NOCLIP || id == ID_GOD || id == ID_INSTAKILL) push_settings();
        else if (id == ID_LOAD_LEVEL) load_level();
        else if (id == ID_RESTORE || (id == ID_CHECKPOINTS && code == LBN_DBLCLK)) restore_checkpoint();
        else if (id == ID_SPEED && code == EN_CHANGE) push_settings();
        else if (id == ID_APPLY) apply_spells();
        else if (id == ID_READ) read_spells();
        else if (id == ID_REPORT) start_report();
        else if (id >= ID_SAVE0 && id < ID_SAVE0 + 3) save_spot(id - ID_SAVE0);
        else if (id >= ID_GO0 && id < ID_GO0 + 3) go_spot(id - ID_GO0);
        else if (!attach()) return 0;
        else if (id == ID_CARDS) g_t->cards_seq++;
        else if (id == ID_BAR) g_t->bar_seq++;
        else if (id == ID_BARS_MAX) g_t->bars_max_seq++;
        else if (id == ID_BEANS) g_t->beans_seq++;
        else if (id == ID_BAG) g_t->bag_seq++;
        else if (id == ID_CARDS_GIVEN_RM) g_t->cards_given_remove_seq++;
        else if (id == ID_CARDS_ALL_RM) g_t->cards_all_remove_seq++;
        else if (id == ID_BAR_RM) g_t->bar_remove_seq++;
        else if (id == ID_BAG_SMALL) g_t->bag_small_seq++;
        else if (id == ID_UNDO) g_t->undo_seq++;
        else if (id == ID_SNAP_NOW) g_t->snap_now_seq++;
        else if (id == ID_BEANS_SET) {
            char buf[16];
            GetWindowTextA(g_beans_value, buf, sizeof buf);
            g_t->beans_value = (int16_t)atoi(buf);
            g_t->beans_set_seq++;
        }
        return 0;
    }
    case WM_DESTROY:
        /* leave the game as it was */
        if (g_t) g_t->noclip = 0, g_t->god = 0, g_t->instakill = 0;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(w, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    (void)prev, (void)cmd;
    GetModuleFileNameA(NULL, g_dir, sizeof g_dir);
    char *slash = strrchr(g_dir, '\\');
    if (slash) *slash = 0;
    InitCommonControls();
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "hppoa_trainer";
    RegisterClassA(&wc);
    RECT r = {0, 0, 885, 892};
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&r, style, FALSE);
    HWND w = CreateWindowExA(WS_EX_TOPMOST, "hppoa_trainer", "HP:PS debug trainer", style, CW_USEDEFAULT,
                             CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, inst, NULL);
    ShowWindow(w, show);
    MSG m;
    while (GetMessageA(&m, NULL, 0, 0) > 0) {
        if (IsDialogMessageA(w, &m)) continue;
        TranslateMessage(&m);
        DispatchMessageA(&m);
    }
    return 0;
}
