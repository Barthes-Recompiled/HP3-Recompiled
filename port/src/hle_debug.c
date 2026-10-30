/**
 * The developer debug menu (hppoa.toml debug_menu, off by default; F10 > Debug).
 *
 * The front end's APT script (poa_startup_xbox.big, poa_fe.big) asks the game whether the menu is allowed through
 * FSCommand "DebugMenu", whose handler sub_00020310 (registered by the static constructor sub_00256C50 in its
 * FSCommand table) sets the APT variable DebugMenu through the set-variable call at [0x38A2F8]. The shipped game
 * always answers "false" (0x2C8718); with "true" (0x2C8730, a separate string in .rdata) the script's key handler
 * opens POA_DEBUGMENU.swf. The menu's code is all still in the game: its DM_* handlers pick a chapter, a room and a
 * start point into 0x376B44 / 0x376B48 / 0x376B4C, which the level loader reads (docs/re-notes.md, Debug menu).
 *
 * Off, the body is the generated one. Built into the recompile with --exclude-manual naming this file.
 */

#include <stdint.h>
#include <stdio.h>

#define RECOMP_GENERATED_CODE
#include "recomp/gen/recomp_types.h"

extern int app_DebugMenu(void); /* hppoa.toml debug_menu (scda/app/app.cpp) */
extern void sub_00020310_gen(void);

/* FSCommand "DebugMenu": APT set-variable("DebugMenu", "false" or "true") */
void sub_00020310(void)
{
    if (!app_DebugMenu()) {
        sub_00020310_gen();
        return;
    }
    static int logged;
    if (!logged) {
        logged = 1;
        fprintf(stderr, "[DEBUG] front end asked for the debug menu: allowed\n");
    }
    { uint32_t _icall_esp = g_esp;
    PUSH32(esp, 0x2C8730);  /* "true" */
    PUSH32(esp, 0x2C9360);  /* "DebugMenu" */
    { uint32_t _icall_target = MEM32(0x38A2F8); PUSH32(esp, 0x00020320u); RECOMP_ICALL_SAFE_AT_CC(_icall_target, _icall_esp, 0x0002031Au); }
    }
    esp = esp + 8;
    esp += 4;
}
