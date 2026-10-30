/*
 * The debug trainer's shared block: hppoa.exe creates the named mapping
 * "Local\hppoa_trainer" at boot; hppoa_trainer.exe (src/trainer) opens it, shows
 * the status the game writes and leaves requests the game carries out on its
 * own thread (hle_trainer.c). Nothing here changes the game unless the trainer
 * asks for it.
 */
#ifndef HPPOA_TRAINER_SHM_H
#define HPPOA_TRAINER_SHM_H

#include <stdint.h>

#define HPPOA_TRAINER_NAME    "Local\\hppoa_trainer"
#define HPPOA_TRAINER_MAGIC   0x52545048u  /* "HPTR" */
#define HPPOA_TRAINER_VERSION 5u

/* The values below are still Philosopher's Stone's: the trainer exe's pages use them, the PoA game side
 * (hle_trainer.c) ignores everything but input injection and the report until PoA's state is found. */

/* spell numbers (0 = none) */
enum { kSpellNone = 0, kSpellFlipendo = 1, kSpellLumos = 2, kSpellDiffindo = 5, kSpellSpongify = 8,
       kSpellIncendio = 10, kSpellAlohomora = 12 };

/* the progress block (= the save file after its 24 byte header): its size; where PoA keeps it is not found yet */
#define HPPOA_PROGRESS_SIZE 0x4AE4u

/* levels: an id = 6 + its index in the title's name table (6 day0.lvl .. 30 day6.lvl were the story) */
#define HPPOA_LEVEL_FIRST 6u
#define HPPOA_LEVEL_LAST  30u

typedef struct HppoaTrainerShm {
    uint32_t magic, version;
    uint32_t pid;                  /* hppoa.exe */
    uint32_t pad1;
    uint64_t mem_offset;           /* host address = guest address + mem_offset */

    /* game -> trainer, every frame Harry is updated */
    volatile uint32_t frame;       /* counts Harry's updates: stops while paused or loading */
    volatile uint32_t player;      /* guest pointer of Harry's entity, 0 when there is none */
    volatile float pos[3];         /* world position, y up */
    volatile float health, health_max;
    volatile uint8_t slots[3];     /* spells on the three buttons */
    volatile uint8_t noclip_on;    /* the game is flying Harry */
    volatile uint16_t cards;       /* wizard cards held (of 101) */
    volatile int16_t beans, beans_max;
    volatile uint16_t cards_given; /* of them, given by the trainer */
    volatile uint8_t undo_ready;   /* the values from before the first trainer change are kept */
    volatile uint8_t pad2;

    /* trainer -> game */
    volatile uint32_t noclip;      /* 1: Harry's update is skipped, the left stick flies him */
    volatile float fly_speed;      /* units a second (0: default) */
    volatile uint32_t god;         /* 1: health kept at its maximum */
    volatile uint32_t slots_seq;   /* bumped to put req_slots on the buttons */
    volatile uint8_t req_slots[3], pad0;
    volatile uint32_t tp_seq;      /* bumped to move Harry to tp_pos */
    volatile float tp_pos[3];
    volatile uint32_t cards_seq;   /* bumped: every missing wizard card, the title's way (a bar each 20th) */
    volatile uint32_t bar_seq;     /* bumped: one more health bar (100, up to 600) */
    volatile uint32_t bars_max_seq;/* bumped: health bars up to the most */
    volatile uint32_t beans_seq;   /* bumped: the bean bag filled */
    volatile uint32_t bag_seq;     /* bumped: the big bean bag (150) */

    /* taking things away again */
    volatile uint32_t cards_given_remove_seq;  /* the cards the trainer gave */
    volatile uint32_t cards_all_remove_seq;    /* every card */
    volatile uint32_t bar_remove_seq;          /* one health bar less (not below one) */
    volatile uint32_t beans_set_seq;           /* beans = beans_value (up to the bag) */
    volatile int16_t beans_value, pad3;
    volatile uint32_t bag_small_seq;           /* the small bean bag (50) */
    volatile uint32_t undo_seq;                /* everything back as before the first trainer change */

    /* the debug report: the trainer bumps report_seq with report_dir set; the game's own thread (not the title's,
     * so a hung game still answers) writes its stacks and state there, asks for a frame capture and sets
     * report_done = report_seq */
    volatile uint32_t report_seq, report_done;
    char report_dir[260];

    /* version 4 */
    volatile uint32_t instakill;   /* 1: an enemy whose CurrentHealth drops goes to 0 (dies on the first hit) */
    volatile uint32_t level;       /* game -> trainer: the level in play */
    volatile uint32_t warp_seq;    /* bumped: load warp_level (the title's StartLevel way) */
    volatile uint32_t warp_level;
    /* checkpoints: the progress block (everything a save keeps) as a level starts, or when asked (snap_now_seq);
     * the trainer files them in run\checkpoints. Restore puts restore_block back and loads restore_level. */
    volatile uint32_t snap_seq, snap_level, snap_now_seq;
    volatile uint8_t snap_block[HPPOA_PROGRESS_SIZE];
    volatile uint32_t restore_seq, restore_level;
    volatile uint8_t restore_block[HPPOA_PROGRESS_SIZE];

    /* version 5: trainer / tools -> game: pad input added to port 0 (hle_input.c), for driving the game without a
     * person and without OS key presses (tests): buttons (XINPUT wButtons), the 8 analog buttons (A B X Y Black
     * White LT RT, 0-255) and the left / right stick, held for inject_ms from when inject_seq is bumped (as HP:CoS) */
    volatile uint32_t inject_seq, inject_ms;
    volatile uint16_t inject_buttons, pad5;
    volatile uint8_t inject_analog[8];
    volatile int16_t inject_stick[4];
} HppoaTrainerShm;

#endif
