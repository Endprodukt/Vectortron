/* settings.h — the owner's choices, kept in save/settings.txt.
 *
 *   free_play   = 0        ; 1: START needs no coin
 *   lives       = 3        ; men at the start of a game (the original's default)
 *   extra_man   = 25000    ; an extra man every this many points (0: none)
 *   camera      = 0        ; 0 arena, 1 follow, 2 classic overhead
 *
 * A missing file or line keeps the default. The camera chosen in the game
 * (button 3) is saved back here.
 */
#ifndef VT_SETTINGS_H
#define VT_SETTINGS_H

typedef struct {
    int free_play;
    int lives;
    int extra_man;
    int camera;
} settings;

extern settings SET;

void settings_load(void);
void settings_save(void);

#endif
