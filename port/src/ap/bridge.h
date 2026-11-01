/* The Archipelago bridge's C interface (bridge.cpp), ported from HP2 (hp2_recomp ap/bridge).
 *
 * Off unless RECOMP_AP_SERVER and RECOMP_AP_SLOT are set (the password, if any, is read from
 * <exe dir>\saves\archipelago\connection.toml, password = "...", so it never passes through the command line or a
 * log). Everything here runs on the game thread: the network thread (client.cpp) only queues packets. */
#ifndef HPPOA_AP_BRIDGE_H
#define HPPOA_AP_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/* 1 if RECOMP_AP_SERVER and RECOMP_AP_SLOT are set (read once): the rest does nothing otherwise */
int hppoa_ap_enabled(void);
/* once a frame: connect, drain packets, (re)send checks */
void hppoa_ap_frame(void);
/* a location's catalog key happened (unknown keys and groups the slot left out are ignored); safe to repeat */
void hppoa_ap_location(const char *key);
/* 1 if the catalog has that location key and the slot's groups include it */
int hppoa_ap_known(const char *key);
/* the goal is done */
void hppoa_ap_goal(void);
/* the next received item to give in the game: its catalog name, kind ("chapter", "area", "spell", "token",
 * "upgrade", "filler") and value (a spell's id, else 0); returns 0 when there is none. hppoa_ap_item_given() after
 * giving it moves the received index on and saves it */
int hppoa_ap_next_item(const char **name, const char **kind, int *value);
void hppoa_ap_item_given(void);
/* 1 while a slot is bound (connected this run, or its session loaded) */
int hppoa_ap_active(void);
/* a whole-number (or true/false) option of the slot's slot_data, def without a bound slot or that key */
int hppoa_ap_option(const char *key, int def);
/* 1 if the slot owns that item (by catalog name; the catalog's start items count) */
int hppoa_ap_has(const char *item_name);
/* the travel page's list: chapters in the game's chapter-table order (index = chapter), then the areas. name is the
 * display name, region the catalog region, chapter the chapter index (-1 for an area reachable from any chapter),
 * unlocked 1 when the slot owns the pass (and, for an area, its chapter's). Returns 0 past the end. */
int hppoa_ap_travel_entry(int i, const char **name, const char **region, int *chapter, int *unlocked);

#ifdef __cplusplus
}
#endif

#endif
