/* xboxrecomp: one copy of the game at a time. hppoa.exe holds a named mutex for its whole life; a second hppoa.exe,
 * or the launcher while one runs, says so, brings the running game's window forward and starts nothing. A second
 * copy would share saves\, shadercache\ and hppoa_run.log with the first (and the launcher would truncate its log). */
#ifndef HPPOA_SINGLE_INSTANCE_H
#define HPPOA_SINGLE_INSTANCE_H

#include <wchar.h>
#include <windows.h>

#define HPPOA_RUNNING_MUTEX L"Local\\hppoa_game_running"
#define HPPOA_GAME_WINDOW_CLASS "XboxRecompFramebuffer" /* fb_present.c */

/* the message, then the running game's window to the front (after the box: a fullscreen game would cover it) */
static inline void hppoa_ReportAlreadyRunning(void)
{
    MessageBoxW(NULL, L"Harry Potter and the Prisoner of Azkaban is already running.",
                L"Harry Potter and the Prisoner of Azkaban", MB_OK | MB_ICONINFORMATION | MB_TOPMOST | MB_SETFOREGROUND);
    HWND w = FindWindowA(HPPOA_GAME_WINDOW_CLASS, NULL);
    if (w) {
        if (IsIconic(w)) ShowWindow(w, SW_RESTORE);
        SetForegroundWindow(w);
    }
}

/* the game: takes the mutex for the rest of the process; 0 when another copy already has it */
static inline int hppoa_ClaimSingleInstance(void)
{
    HANDLE m = CreateMutexW(NULL, FALSE, HPPOA_RUNNING_MUTEX);
    return !(m && GetLastError() == ERROR_ALREADY_EXISTS);
}

/* whether a game holds the mutex now */
static inline int hppoa_GameRunning(void)
{
    HANDLE m = OpenMutexW(SYNCHRONIZE, FALSE, HPPOA_RUNNING_MUTEX);
    if (!m) return 0;
    CloseHandle(m);
    return 1;
}

/* xboxrecomp: hppoa.exe runs only from hppoa_launcher.exe, which sets this in the game's environment (with its
 * RECOMP_* settings, which a bare start would lack); test scripts set it themselves */
#define HPPOA_LAUNCHER_ENV L"HPPOA_LAUNCHER"

static inline int hppoa_StartedByLauncher(void)
{
    return GetEnvironmentVariableW(HPPOA_LAUNCHER_ENV, NULL, 0) != 0;
}

/* hppoa.exe started on its own: says to use the launcher and offers to open it (beside this exe) */
static inline void hppoa_OfferLauncher(void)
{
    const wchar_t *title = L"Harry Potter and the Prisoner of Azkaban";
    int r = MessageBoxW(NULL, L"Please start the game with hppoa_launcher.exe.\n\nOpen the launcher now?", title,
                        MB_YESNO | MB_ICONINFORMATION | MB_TOPMOST | MB_SETFOREGROUND);
    if (r != IDYES) return;
    wchar_t dir[MAX_PATH], exe[MAX_PATH + 32];
    DWORD n = GetModuleFileNameW(NULL, dir, MAX_PATH);
    wchar_t *slash = n && n < MAX_PATH ? wcsrchr(dir, L'\\') : NULL;
    if (!slash) return;
    slash[1] = 0;
    lstrcpyW(exe, dir);
    lstrcatW(exe, L"hppoa_launcher.exe");
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof si);
    si.cb = sizeof si;
    if (CreateProcessW(exe, NULL, NULL, NULL, FALSE, 0, NULL, dir, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    } else {
        MessageBoxW(NULL, L"hppoa_launcher.exe was not found beside hppoa.exe.", title, MB_OK | MB_ICONWARNING | MB_TOPMOST);
    }
}

#endif
