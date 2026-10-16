/* settings.c — save/settings.txt, "key = value" lines. The control
 * bindings are kept in the same file (bind_*). */
#include "settings.h"
#include "bind.h"

#include <vectorpie/vp.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

settings SET = {
    .free_play = 0, .lives = 3, .extra_man = 25000, .difficulty = 5, .two_players = 1, .attract_sound = 1,
    .size_x = 100, .size_y = 100, .pos_x = 0, .pos_y = 0, .camera = 0, .effects = 2,
    .volume = 8,
};

typedef struct { const char *key; int *value; int min, max; const char *note; } field;

static const field FIELDS[] = {
    { "free_play",     &SET.free_play,     0, 1,       "1: START needs no coin" },
    { "lives",         &SET.lives,         1, 9,       "men at the start of a game" },
    { "extra_man",     &SET.extra_man,     0, 1000000, "an extra man every this many points (0: none)" },
    { "difficulty",    &SET.difficulty,    0, 10,      "the original's master difficulty (5: as the wave table)" },
    { "two_players",   &SET.two_players,   0, 1,       "1: START 2 starts a game for two, taking turns" },
    { "attract_sound", &SET.attract_sound, 0, 1,       "1: the demo game makes its sounds" },
    { "size_x",        &SET.size_x,        50, 120,    "picture width, percent" },
    { "size_y",        &SET.size_y,        50, 120,    "picture height, percent" },
    { "pos_x",         &SET.pos_x,         -600, 600,  "picture offset right, board units" },
    { "pos_y",         &SET.pos_y,         -600, 600,  "picture offset up, board units" },
    { "camera",        &SET.camera,        0, 2,       "0 Robotron X, 1 arena, 2 classic overhead" },
    { "effects",       &SET.effects,       0, 2,       "0 off, 1 light, 2 full" },
    { "volume",        &SET.volume,        0, 10,      "0..10" },
};
#define NFIELDS (int)(sizeof FIELDS / sizeof FIELDS[0])

static const char *path(char *buf, size_t size)
{
    snprintf(buf, size, "%s/settings.txt", vp_save_dir());
    return buf;
}

void settings_load(void)
{
    char p[1024], line[512];
    FILE *f = fopen(path(p, sizeof p), "r");
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        char key[64], value[400];
        char *c = strpbrk(line, ";#");
        if (c) *c = 0;
        if (sscanf(line, " %63[a-z_0-9] = %399[^\r\n]", key, value) != 2) continue;
        char *end = value + strlen(value);
        while (end > value && (end[-1] == ' ' || end[-1] == '\t')) *--end = 0;
        if (bind_parse(key, value)) continue;
        for (int i = 0; i < NFIELDS; i++) {
            if (strcmp(key, FIELDS[i].key)) continue;
            int v = atoi(value);
            if (v >= FIELDS[i].min && v <= FIELDS[i].max) *FIELDS[i].value = v;
        }
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
    fprintf(f, "; Vectortron settings (the service menu writes this file)\n");
    for (int i = 0; i < NFIELDS; i++)
        fprintf(f, "%-14s = %-8d ; %s\n", FIELDS[i].key, *FIELDS[i].value, FIELDS[i].note);
    fprintf(f, "; controls: none = the cabinet's advmame.rc\n");
    bind_save(f);
    fclose(f);
    remove(p);
    rename(tmp, p);
}
