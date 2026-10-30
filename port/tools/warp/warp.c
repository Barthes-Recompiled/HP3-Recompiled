/* hppoa_warp.exe <level>: asks the running game to load a level through the trainer's shared block (as the trainer's
 * Load level button does). Level 24 = day5's Quidditch match. Run it in the game's Wine prefix. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../src/trainer_shm.h"

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: hppoa_warp <level %u..%u>\n", HPPOA_LEVEL_FIRST, HPPOA_LEVEL_LAST); return 2; }
    unsigned level = (unsigned)strtoul(argv[1], NULL, 0);
    HANDLE m = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, "Local\\hppoa_trainer");
    if (!m) { fprintf(stderr, "game not running (no Local\\hppoa_trainer)\n"); return 1; }
    HppoaTrainerShm *s = MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(HppoaTrainerShm));
    if (!s || s->version < 4) { fprintf(stderr, "shared block missing or too old\n"); return 1; }
    printf("in level %u, Harry %08X; loading level %u\n", s->level, s->player, level);
    s->warp_level = level;
    MemoryBarrier();
    s->warp_seq++;
    return 0;
}
