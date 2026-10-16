/* game.h — the rules: one player's world, wave by wave.
 *
 * Everything here follows the original's source (historicalsource/robotron):
 * the wave table, the speeds, the timers, the scores. The original moves
 * things in screen pixels per 60 Hz frame; this world uses the same units
 * (x right, z up the screen), stepped at 60 Hz, so the numbers carry over.
 */
#ifndef VT_GAME_H
#define VT_GAME_H

#include <stdint.h>
#include "controls.h"

/* The playfield, inside the border, in the original's pixels. */
#define ARENA_X0   8.0f
#define ARENA_X1   292.0f
#define ARENA_Z0   18.0f
#define ARENA_Z1   236.0f
#define ARENA_CX   ((ARENA_X0 + ARENA_X1) / 2)
#define ARENA_CZ   ((ARENA_Z0 + ARENA_Z1) / 2)

typedef enum {
    T_NONE,
    T_GRUNT,
    T_HULK,
    T_BRAIN,
    T_PROG,
    T_SPHEROID,
    T_ENFORCER,
    T_QUARK,
    T_TANK,
    T_ELECTRODE,
    T_MOMMY,
    T_DADDY,
    T_MIKEY,
    T_SPARK,        /* an enforcer's shot */
    T_SHELL,        /* a tank's shot */
    T_CRUISE,       /* a brain's cruise missile */
    T_COUNT
} etype;

typedef struct {
    uint8_t type;
    uint8_t state;
    int16_t target;     /* what it chases (an index), -1 none */
    float   x, z;
    float   vx, vz;
    float   timer;      /* the next decision, in frames */
    float   timer2;
    float   walk;       /* walk cycle (radians) */
    float   heading;    /* where it faces (radians) */
    float   spin;
    float   speed;
    int     count;      /* children left to make (spheroids, quarks), bounces (shells) */
    float   age;        /* frames since it appeared */
} ent;

#define MAX_ENTS   320
#define MAX_SHOTS  8

typedef struct {
    float x, z, dx, dz;
    int   live;
} shot;

typedef enum {
    PH_MATERIALIZE,     /* the wave builds itself */
    PH_PLAY,
    PH_DYING,           /* the player was hit */
    PH_CLEARED,         /* every robot that counts is gone */
    PH_RESTART,         /* a man lost, men left: waiting for world_resume */
    PH_OVER             /* no men left */
} phase;

typedef struct {
    int    wave;            /* 1.. */
    int    score;
    int    lives;
    int    next_extra;
    int    rescued;         /* humans rescued this wave: the bonus grows */
    phase  ph;
    float  ph_time;         /* frames in this phase */

    float  px, pz;          /* the player */
    float  face_x, face_z;  /* the single-stick facing */
    float  pwalk, pheading;
    int    moving;
    float  fire_timer;
    shot   shots[MAX_SHOTS];

    ent    e[MAX_ENTS];
    int    n;

    float  grunt_speedup;   /* the grunts' wait shrinks as the wave goes on */
    float  wave_time;
    unsigned rng;
    int    demo;            /* the attract mode is playing */
} world;

void world_new_game(world *w, int lives, int extra_man, unsigned seed);
void world_start_wave(world *w);
/* After a man is lost: the robots left are spread out again and the wave
 * materializes anew, as in the original. */
void world_resume(world *w);
/* One 60 Hz step. */
void world_step(world *w, const controls *c);
/* Robots still to kill this wave (hulks, electrodes, humans don't count). */
int  world_robots_left(const world *w);
/* The original's electrode shape for this wave. */
int  wave_electrode_shape(int wave);

/* The automatic player of the attract mode. */
void demo_controls(const world *w, controls *c);

#endif
