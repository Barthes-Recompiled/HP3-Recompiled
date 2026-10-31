/**
 * The game is paused: the pause menu's PC Settings page (scda/app/menu.cpp) opens on Back only then.
 *
 * PoA's pause menu is an APT movie (POA_IG_OPT), so the port can't add a row to it as HP1/HP2's hle_frontend.c
 * does. Its ActionScript calls the FSCommands PauseGame (sub_00021C90) when it opens and UnpauseGame
 * (sub_00021D00) when it closes; they are reached only through the FSCommand table (84 {name, handler} pairs at
 * 0x30EF28, filled by 0x00256C50 through sub_001AE6C0) and bracket the whole paused session, its Save Game /
 * Options / Quit Game screens included. Both bodies run unchanged; the flag is set after them.
 *
 * Built into the recompile with --exclude-manual port/src/hle_pause.c (regen.sh).
 */
#include <windows.h>

static volatile LONG g_paused;
extern void sub_00021C90_gen(void);
extern void sub_00021D00_gen(void);

int hppoa_GamePaused(void) { return InterlockedCompareExchange(&g_paused, 0, 0) != 0; }

/* FSCommand "PauseGame" */
void sub_00021C90(void)
{
    sub_00021C90_gen();
    InterlockedExchange(&g_paused, 1);
}

/* FSCommand "UnpauseGame" */
void sub_00021D00(void)
{
    sub_00021D00_gen();
    InterlockedExchange(&g_paused, 0);
}
