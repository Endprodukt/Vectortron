/* screen.c — scaling and moving the whole picture (see screen.h). */
#include "screen.h"

#include <vectorpie/vp.h>

void __real_vp_moveto(vp_dvg *d, int x, int y);
void __real_vp_lineto(vp_dvg *d, int x, int y);
void __wrap_vp_moveto(vp_dvg *d, int x, int y);
void __wrap_vp_lineto(vp_dvg *d, int x, int y);

static float SX = 1, SY = 1;
static int OX, OY;

void screen_set(int size_x, int size_y, int pos_x, int pos_y)
{
    SX = size_x / 100.0f;
    SY = size_y / 100.0f;
    OX = pos_x;
    OY = pos_y;
}

#define MID (VP_DVG_COORD_MAX / 2.0f)

static int tx(int x) { return (int)(MID + (x - MID) * SX + OX + 0.5f); }
static int ty(int y) { return (int)(MID + (y - MID) * SY + OY + 0.5f); }

void __wrap_vp_moveto(vp_dvg *d, int x, int y) { __real_vp_moveto(d, tx(x), ty(y)); }
void __wrap_vp_lineto(vp_dvg *d, int x, int y) { __real_vp_lineto(d, tx(x), ty(y)); }
