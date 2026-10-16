/* sound.h — the game's sounds.
 *
 * Every sound is made in code at start-up, in the spirit of the Williams
 * sound board (square and saw sweeps, noise, warbles). A file
 * assets/sounds/<name>.wav (or .ogg) next to the executable replaces the
 * made one: see README.md for the names.
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
    SND_COUNT
} snd_id;

void snd_init(vp_audio *a);
void snd_play(snd_id id);
void snd_play_ex(snd_id id, float volume, float pitch);
/* A looping sound: returns its voice (0 when silent). */
vp_voice snd_loop(snd_id id, float volume);
void snd_stop(vp_voice v);
void snd_stop_all(void);

#endif
