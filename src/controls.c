/* controls.c — twin stick and single stick, read from the cabinet's actions. */
#include "controls.h"

#include <math.h>

static int held(vp_input *in, const char *a) { return vp_input_held(in, a); }

/* A stick from four digital directions and an analog pair. */
static void stick(vp_input *in, const char *up, const char *down, const char *left, const char *right,
                  const char *ax, const char *ay, float *x, float *z)
{
    float sx = 0, sz = 0;
    if (ax) sx = vp_input_axis(in, ax);
    if (ay) sz = -vp_input_axis(in, ay);            /* stick up is negative */
    if (held(in, left))  sx = -1;
    if (held(in, right)) sx = 1;
    if (held(in, down))  sz = -1;
    if (held(in, up))    sz = 1;
    *x = sx;
    *z = sz;
}

/* Snap to the nearest of the 8 directions, unit length. */
static void snap8(float *x, float *z)
{
    float a = atan2f(*z, *x);
    float k = roundf(a / (3.14159265f / 4));
    a = k * (3.14159265f / 4);
    *x = cosf(a);
    *z = sinf(a);
    if (fabsf(*x) < 1e-4f) *x = 0;
    if (fabsf(*z) < 1e-4f) *z = 0;
}

void controls_read(vp_input *in, controls *c, float *facing_x, float *facing_z)
{
    float mx, mz, mx2, mz2, fx, fz, fx2, fz2;

    stick(in, "p1_up", "p1_down", "p1_left", "p1_right", "p1_stickx", "p1_sticky", &mx, &mz);
    stick(in, "p1_doubleleft_up", "p1_doubleleft_down", "p1_doubleleft_left", "p1_doubleleft_right",
          NULL, NULL, &mx2, &mz2);
    if (mx2 != 0 || mz2 != 0) { mx = mx2; mz = mz2; }

    stick(in, "p1_doubleright_up", "p1_doubleright_down", "p1_doubleright_left", "p1_doubleright_right",
          NULL, NULL, &fx, &fz);
    stick(in, "p2_up", "p2_down", "p2_left", "p2_right", "p2_stickx", "p2_sticky", &fx2, &fz2);
    if (fx == 0 && fz == 0) { fx = fx2; fz = fz2; }

    float ml = sqrtf(mx * mx + mz * mz);
    if (ml > 1) { mx /= ml; mz /= ml; }
    c->mx = mx;
    c->mz = mz;

    int b1 = held(in, "p1_button1"), b2 = held(in, "p1_button2");
    /* single stick: facing follows movement unless strafing (button 2) */
    if (ml > 0.3f && !b2) {
        *facing_x = mx;
        *facing_z = mz;
        snap8(facing_x, facing_z);
    }

    if (fabsf(fx) > 0.35f || fabsf(fz) > 0.35f) {
        snap8(&fx, &fz);
        c->fx = fx;
        c->fz = fz;
        c->fire = 1;
    } else if (b1 || b2) {
        c->fx = *facing_x;
        c->fz = *facing_z;
        c->fire = 1;
    } else {
        c->fx = c->fz = 0;
        c->fire = 0;
    }
    c->camera = vp_input_pressed(in, "p1_button3");
}
