/* render.h — the camera, the arena, effects, the HUD and the screens between. */
#ifndef VT_RENDER_H
#define VT_RENDER_H

#include <vectorpie/vp.h>
#include "game.h"
#include "models.h"

#define CAMERA_MODES 3

void render_init(void);

/* Effects, started by the rules. Positions in world units. */
void fx_explode(float x, float y, float z, rgb color, float dirx, float dirz, int pieces, float size);
void fx_popup(float x, float z, int value);
void fx_skull(float x, float z);
void fx_flash(rgb color, float frames);   /* the border flashes */
void fx_clear(void);
void fx_step(void);                       /* one 60 Hz step */

/* The camera follows the player smoothly; `snap` jumps there at once. */
void render_camera(const world *w, int mode, int snap);
const vp3d_view *render_view(void);

/* The arena and everything in it. `t` in seconds, for the color cycles. */
void render_world(vp_dvg *d, const world *w, double t);

/* The Robotron X score: the score, the men and the wave on the far wall. */
void render_wall_hud(vp_dvg *d, const world *w, int player, int players, double t);

/* The flat score line (the classic overhead camera): both players' scores and men, the wave. */
void render_hud(vp_dvg *d, const world *w1, const world *w2, int players, int current, int hiscore,
                double t);

/* The original's between-waves effect: rectangles rushing out from the
 * middle, u from 0 to 1. */
void render_wave_tunnel(vp_dvg *d, float u, double t);

/* Text helpers in board units. */
void text(vp_dvg *d, rgb c, const char *s, float x, float y, float h, int align);

#endif
