/* models.h — the 3D shapes of everything in the arena, built in code.
 *
 * World units are the original game's screen pixels: a grunt is about 13
 * high, the arena about 290 x 225. The floor is y = 0; x right, z away from
 * the default camera (the original's screen "up").
 */
#ifndef VT_MODELS_H
#define VT_MODELS_H

#include <vectorpie/vp.h>

typedef enum {
    MDL_PLAYER,
    MDL_GRUNT,
    MDL_HULK,
    MDL_BRAIN,
    MDL_PROG,
    MDL_MOMMY,
    MDL_DADDY,
    MDL_MIKEY,
    MDL_SPHEROID,
    MDL_ENFORCER,
    MDL_SPARK,
    MDL_QUARK,
    MDL_TANK,
    MDL_SHELL,
    MDL_CRUISE,
    MDL_SKULL,
    MDL_ELECTRODE0,     /* electrodes change shape from wave to wave */
    MDL_ELECTRODE1,
    MDL_ELECTRODE2,
    MDL_ELECTRODE3,
    MDL_ELECTRODE4,
    MDL_ELECTRODE5,
    MDL_COUNT
} model_id;

#define ELECTRODE_SHAPES 6

typedef struct { unsigned char r, g, b; } rgb;

static inline rgb mkrgb(int r, int g, int b) { rgb c = { (unsigned char)r, (unsigned char)g, (unsigned char)b }; return c; }
rgb rgb_scale(rgb c, float k);

/* How to draw one: where, which way it faces, how far into its walk cycle,
 * and how stretched (the materialize effect stretches things tall and thin). */
typedef struct {
    float x, y, z;
    float heading;      /* radians about y; 0 faces +z */
    float walk;         /* walk cycle phase in radians; legs and arms swing */
    float spin;         /* free rotation for spinning things */
    float sxz, sy;      /* scale across and up; 1, 1 normally */
    rgb   color;        /* the model's main color; parts may have their own */
    int   flat_color;   /* 1: every part in `color` (flashes, dying) */
} model_pose;

void models_init(void);
void models_free(void);

void model_draw(vp_dvg *d, const vp3d_view *v, model_id id, const model_pose *p);

/* The model's main color, as in the original. */
rgb  model_color(model_id id);
/* About how tall the model is (for score pop-ups, fragments). */
float model_height(model_id id);

#endif
