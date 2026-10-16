/* demo.c — the attract mode's player: keeps away from danger, picks up
 * humans, shoots at the nearest robot. Not good, just lively. */
#include "game.h"

#include <math.h>

static int is_threat(int t)
{
    return t != T_NONE && t != T_MOMMY && t != T_DADDY && t != T_MIKEY;
}

static int is_target(int t)
{
    return is_threat(t) && t != T_HULK;
}

void demo_controls(const world *w, controls *c)
{
    float ax = 0, az = 0;
    float best = 1e9f, tx = 0, tz = 0;
    float human = 1e9f, hx = 0, hz = 0;

    for (int i = 0; i < w->n; i++) {
        const ent *e = &w->e[i];
        float dx = e->x - w->px, dz = e->z - w->pz;
        float d2 = dx * dx + dz * dz + 1;
        if (is_threat(e->type)) {
            float r = e->type == T_ELECTRODE ? 18.0f : e->type == T_HULK ? 40.0f : 55.0f;
            if (d2 < r * r) {
                ax -= dx / d2 * 60;
                az -= dz / d2 * 60;
            }
            if (is_target(e->type) && d2 < best) { best = d2; tx = dx; tz = dz; }
        } else if (e->type != T_NONE && d2 < human) {
            human = d2; hx = dx; hz = dz;
        }
    }
    if (human < 1e9f) {
        float l = sqrtf(human);
        ax += hx / l * 0.8f;
        az += hz / l * 0.8f;
    }
    /* keep off the walls */
    if (w->px < ARENA_X0 + 25) ax += 1.5f;
    if (w->px > ARENA_X1 - 25) ax -= 1.5f;
    if (w->pz < ARENA_Z0 + 25) az += 1.5f;
    if (w->pz > ARENA_Z1 - 25) az -= 1.5f;

    float l = sqrtf(ax * ax + az * az);
    c->mx = l > 0.2f ? ax / l : 0;
    c->mz = l > 0.2f ? az / l : 0;
    c->camera = 0;
    if (best < 1e9f) {
        float a = atan2f(tz, tx);
        a = roundf(a / (3.14159265f / 4)) * (3.14159265f / 4);
        c->fx = cosf(a);
        c->fz = sinf(a);
        c->fire = 1;
    } else {
        c->fire = 0;
        c->fx = c->fz = 0;
    }
}
