/* sound.h — the game's sounds.
 *
 * The original's sounds, played by an emulated Williams sound board from
 * the owner's own Robotron ROM set; without it, sounds made in code in the
 * same spirit. A file assets/sounds/<name>.wav (or .ogg) next to the
 * executable replaces either: see README.md for the names.
 *
 * With the original sounds the board's single voice and its priorities are
 * kept: a sound only cuts in on one of the same priority or lower, as on the
 * cabinet.
 */
#ifndef VT_SOUND_H
#define VT_SOUND_H

#include <vectorpie/vp.h>

typedef enum {
    SND_SHOT,           /* the player's laser */
    SND_GRUNT_DIE,      /* a grunt (and most robots) blown apart */
    SND_ELECTRODE_DIE,
    SND_HULK_HIT,       /* a shot pushing a hulk back */
    SND_RESCUE,         /* a human picked up */
    SND_HUMAN_DIE,      /* a human killed: the skull */
    SND_PROG_MAKE,      /* a brain turning a human into a prog */
    SND_BRAIN_DIE,
    SND_CRUISE,         /* a brain's cruise missile */
    SND_SPHEROID_DIE,
    SND_ENFORCER_BIRTH, /* a spheroid drops an enforcer */
    SND_ENFORCER_SHOT,  /* a spark */
    SND_ENFORCER_DIE,
    SND_QUARK_DIE,
    SND_TANK_BIRTH,
    SND_TANK_SHOT,
    SND_SHELL_BOUNCE,
    SND_TANK_DIE,
    SND_PLAYER_DIE,
    SND_WAVE_START,     /* the materialize sweep */
    SND_WAVE_END,
    SND_EXTRA_MAN,
    SND_COIN,
    SND_START,
    SND_GRUNT_STEP,     /* the grunts' march */
    SND_REPROGRAM,      /* a brain at work on a human */
    SND_SMALL_DIE,      /* a cruise missile shot */
    SND_SPARK_DIE,      /* a spark shot */
    SND_SHELL_DIE,      /* a shell shot */
    SND_PROG_DIE,
    SND_TRANSPORT,      /* a brain wave beams in */
    SND_START2,         /* a 2 player game starts */
    SND_HISCORE,        /* a high score: the tune (HSTUNE) */
    SND_HISCORE_TOP,    /* the best score of all: the air raid (ULTUNE) */
    SND_COUNT
} snd_id;

/* Loads the sounds: from the original's sound ROM when the owner's MAME set
 * is found (the Williams sound board emulated, see williams.h), else made
 * in code; a file assets/sounds/<name>.wav replaces either. */
void snd_init(vp_audio *a);
/* 1 when the sounds come from the original ROM. */
int  snd_original(void);
/* For tools: the sound's file name, its original command sequence (0 when
 * the original has none), and the sound ROM as snd_init finds it
 * (malloc'ed; NULL if none). */
#include "williams.h"
const char *snd_name(snd_id id);
int  snd_original_steps(snd_id id, const wms_step **steps);
unsigned char *snd_find_rom(size_t *size);
/* The original sound recorded from the ROM, as the board plays it (full
 * level; malloc'ed, NULL when the original has none). */
float *snd_record_original(const unsigned char *rom, size_t n, snd_id id, int rate, int *frames);

/* Once per 60 Hz step: the original's sound priorities run out. */
void snd_tick(void);
void snd_play(snd_id id);
void snd_play_ex(snd_id id, float volume, float pitch);
/* A looping sound: returns its voice (0 when silent). */
vp_voice snd_loop(snd_id id, float volume);
void snd_stop(vp_voice v);
void snd_stop_all(void);

#endif
