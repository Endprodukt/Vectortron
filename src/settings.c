/* settings.c — save/settings.txt, "key = value" lines. */
#include "settings.h"

#include <vectorpie/vp.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

settings SET = { 0, 3, 25000, 0, 5, -1 };

static const char *path(char *buf, size_t size)
{
    snprintf(buf, size, "%s/settings.txt", vp_save_dir());
    return buf;
}

void settings_load(void)
{
    char p[1024], line[256];
    FILE *f = fopen(path(p, sizeof p), "r");
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        char key[64];
        int value;
        char *c = strpbrk(line, ";#");
        if (c) *c = 0;
        if (sscanf(line, " %63[a-z_] = %d", key, &value) != 2) continue;
        if (!strcmp(key, "free_play")) SET.free_play = value != 0;
        else if (!strcmp(key, "lives") && value >= 1 && value <= 9) SET.lives = value;
        else if (!strcmp(key, "extra_man") && value >= 0) SET.extra_man = value;
        else if (!strcmp(key, "camera") && value >= 0 && value <= 2) SET.camera = value;
        else if (!strcmp(key, "difficulty") && value >= 0 && value <= 10) SET.difficulty = value;
        else if (!strcmp(key, "floor") && value >= -1 && value <= 2) SET.floor = value;
    }
    fclose(f);
}

void settings_save(void)
{
    char p[1024], tmp[1100];
    path(p, sizeof p);
    snprintf(tmp, sizeof tmp, "%s.new", p);
    FILE *f = fopen(tmp, "w");
    if (!f) return;
    fprintf(f, "; Vectortron settings\n");
    fprintf(f, "free_play = %d   ; 1: START needs no coin\n", SET.free_play);
    fprintf(f, "lives     = %d   ; men at the start of a game\n", SET.lives);
    fprintf(f, "extra_man = %d   ; an extra man every this many points (0: none)\n", SET.extra_man);
    fprintf(f, "camera    = %d   ; 0 Robotron X, 1 arena, 2 classic overhead (button 3 in the game)\n", SET.camera);
    fprintf(f, "floor     = %d   ; -1 auto, 0 tiles (Robotron X), 1 grid (lighter on the beam), 2 none\n", SET.floor);
    fprintf(f, "difficulty = %d  ; the original's master difficulty, 0..10 (5: as the wave table)\n", SET.difficulty);
    fclose(f);
    remove(p);
    rename(tmp, p);
}
