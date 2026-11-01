/* xboxrecomp: one copy of the game at a time. hppoa.exe holds a named mutex for its whole life; a second hppoa.exe
 * while one runs says so, brings the running game's window forward and starts nothing. A second
 * copy would share saves\, shadercache\ and hppoa_run.log with the first. */
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

#endif
