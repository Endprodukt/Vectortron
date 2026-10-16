/* settings.h — the owner's choices, kept in save/settings.txt.
 *
 * Set from the service menu (hold START 1 + START 2 in the attract mode, or
 * the service button); the file can be edited by hand too: "key = value"
 * lines, unknown keys ignored, a missing key keeps its default.
 */
#ifndef VT_SETTINGS_H
#define VT_SETTINGS_H

typedef struct {
    /* game */
    int free_play;      /* 1: START needs no coin */
    int lives;          /* men at the start of a game, 1..9 (the original: 3) */
    int extra_man;      /* an extra man every this many points, 0: none */
    int difficulty;     /* the original's MASTER DIFFICULTY, 0..10 (5: the table as is) */
    int two_players;    /* 1: START 2 starts a game for two, taking turns */
    int attract_sound;  /* 1: the demo game plays its sounds */
    /* display */
    int size_x, size_y; /* picture size, percent of the board (50..120) */
    int pos_x, pos_y;   /* picture offset, board units (-600..600) */
    int camera;         /* 0 Robotron X, 1 arena, 2 classic overhead */
    int effects;        /* 0 off, 1 light, 2 full */
    /* sound */
    int volume;         /* 0..10 */
} settings;

extern settings SET;

void settings_load(void);
void settings_save(void);

#endif
