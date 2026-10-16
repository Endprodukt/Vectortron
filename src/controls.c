/* controls.c — twin stick and single stick, from the game's actions
 * (bind.h). Moving and firing go 8 ways only, as on the original's two
 * 8-way joysticks: an analog stick counts past half travel. */
#include "controls.h"
#include "bind.h"

static int dir(action neg, action pos)
{
    int n = act_held(neg), p = act_held(pos);
    return n == p ? 0 : p ? 1 : -1;     /* both: nothing, as the original */
}

void controls_read(vp_input *in, controls *c, float *facing_x, float *facing_z)
{
    (void)in;
    int mx = dir(ACT_LEFT, ACT_RIGHT), mz = dir(ACT_DOWN, ACT_UP);
    int fx = dir(ACT_FIRE_LEFT, ACT_FIRE_RIGHT), fz = dir(ACT_FIRE_DOWN, ACT_FIRE_UP);
    c->mx = (float)mx;
    c->mz = (float)mz;

    /* single stick: the facing follows the stick, unless strafing */
    int strafe = act_held(ACT_STRAFE);
    if ((mx || mz) && !strafe) {
        *facing_x = (float)mx;
        *facing_z = (float)mz;
    }
    if (fx || fz) {
        c->fx = (float)fx;
        c->fz = (float)fz;
        c->fire = 1;
    } else if (act_held(ACT_FIRE) || strafe) {
        c->fx = *facing_x;
        c->fz = *facing_z;
        c->fire = 1;
    } else {
        c->fx = c->fz = 0;
        c->fire = 0;
    }
    c->camera = act_pressed(ACT_CAMERA);
}
