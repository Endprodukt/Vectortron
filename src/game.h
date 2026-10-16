/* game.h — the rules: one player's world, wave by wave.
 *
 * Everything here follows the original's source (historicalsource/robotron):
 * the wave table, the speeds, the timers, the scores. The original moves
 * things in screen pixels (x) and lines (y, downwards) per 60 Hz frame; this
 * world uses the same units, stepped at 60 Hz, so the numbers carry over:
 *   x = the original's pixel column (2 per byte)
 *   z = 258 - the original's line: up the screen is +z, away from the camera
 */
#ifndef VT_GAME_H
#define VT_GAME_H

#include <stdint.h>
#include "controls.h"

/* The playfield (RRF.ASM:67-70): pixels 14..287, lines 24..234. */
#define ARENA_X0   14.0f
#define ARENA_X1   287.0f
#define ARENA_Z0   24.0f
#define ARENA_Z1   234.0f
#define ARENA_CX   ((ARENA_X0 + ARENA_X1) / 2)
#define ARENA_CZ   ((ARENA_Z0 + ARENA_Z1) / 2)
#define LINE_TO_Z(line) (258.0f - (line))

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
    uint8_t state;      /* a phase of its life (spheroid, quark, brain...) */
    uint8_t axis;       /* hulk, prog: the axis it moves on */
    uint8_t flash;      /* frames it shows white (hit) */
    int16_t target;     /* what it chases (an index), -1 the player */
    float   x, z;       /* its center */
    float   vx, vz;     /* velocity, per frame */
    float   ax, az;     /* acceleration, or a direction */
    float   tx, tz;     /* a point it heads for */
    int     timer;      /* frames to its next step */
    int     timer2;     /* its next decision, in steps */
    int     timer3;     /* shots, drops */
    int     count;      /* children left to make, steps of life left */
    int     alt;        /* alternating step sizes */
    float   walk;       /* walk cycle (radians) */
    float   heading;    /* where it faces (radians, 0 = +z) */
    float   spin;
    int     age;        /* frames since it appeared; negative: not yet materialized */
} ent;

#define MAX_ENTS   400
#define MAX_SHOTS  4        /* the original's laser limit */

typedef struct {
    float x, z, dx, dz;     /* dx, dz: -1, 0, 1 */
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
    int    lives;           /* men, counting the one in play */
    int    start_lives;
    int    extra_every, next_extra;
    int    difficulty;
    int    rescued;         /* humans rescued this life in this wave: the bonus grows */
    phase  ph;
    int    ph_time;         /* frames in this phase */
    int    mat_len;         /* frames the materialize takes */
    int    transporter;     /* a brain wave: the transporter effect */

    float  px, pz;          /* the player */
    float  face_x, face_z;  /* the single-stick facing */
    float  pwalk, pheading;
    int    fire_count;      /* frames the fire direction has been held */
    float  fire_dx, fire_dz;
    shot   shots[MAX_SHOTS];

    ent    e[MAX_ENTS];
    int    n;

    /* the difficulty rows for this wave, live (grunts speed up) */
    int    p[12];
    int    rmxspd_saved;
    int    spheroids_saved;
    int    grunt_clock;     /* the grunts' 4-frame tick */
    int    exec_clock;      /* the 15-frame game exec */
    int    exec_passes;
    int    scored;          /* scored since the last speed-up check */
    int    shells_fired;    /* the original's shell counter (and its bug) */
    int    hulk_rr;         /* hulks pick humans round-robin */

    unsigned rng;
    int    demo;            /* the attract mode is playing */
} world;

void world_new_game(world *w, int lives, int extra_man, int difficulty, unsigned seed);
void world_start_wave(world *w);
/* After a man is lost: the robots left are spread out again and the wave
 * materializes anew, as in the original. */
void world_resume(world *w);
/* One 60 Hz step. */
void world_step(world *w, const controls *c);
/* Robots still to kill this wave (hulks, electrodes, progs, humans don't count). */
int  world_robots_left(const world *w);
/* The original's electrode shape for this wave (an index into the models). */
int  wave_electrode_shape(int wave);

/* The automatic player of the attract mode. */
void demo_controls(const world *w, controls *c);

#endif
