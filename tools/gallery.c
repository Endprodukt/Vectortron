/* gallery — every model of the game on one screen, turning.
 *   vt-gallery --windowed        (button 1: solid / wireframe parts as built)
 */
#include <vectorpie/vp.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/models.h"

#define W VP_DVG_COORD_MAX

static const struct { model_id m; const char *name; float y; } L[] = {
    { MDL_PLAYER, "PLAYER", 0 },   { MDL_GRUNT, "GRUNT", 0 },     { MDL_HULK, "HULK", 0 },
    { MDL_BRAIN, "BRAIN", 0 },     { MDL_PROG, "PROG", 0 },       { MDL_MOMMY, "MOMMY", 0 },
    { MDL_DADDY, "DADDY", 0 },     { MDL_MIKEY, "MIKEY", 0 },     { MDL_SPHEROID, "SPHEROID", 6 },
    { MDL_ENFORCER, "ENFORCER", 5 }, { MDL_SPARK, "SPARK", 5 },   { MDL_QUARK, "QUARK", 6 },
    { MDL_TANK, "TANK", 0 },       { MDL_SHELL, "SHELL", 3 },     { MDL_CRUISE, "CRUISE", 5 },
    { MDL_SKULL, "SKULL", 0 },     { MDL_ELECTRODE0, "ELECTRODE 1", 0 }, { MDL_ELECTRODE1, "ELECTRODE 2", 0 },
    { MDL_ELECTRODE2, "ELECTRODE 3", 0 }, { MDL_ELECTRODE3, "ELECTRODE 4", 0 },
    { MDL_ELECTRODE4, "ELECTRODE 5", 0 }, { MDL_ELECTRODE5, "ELECTRODE 6", 0 },
};
#define N ((int)(sizeof L / sizeof L[0]))
#define COLS 6
#define SPACING 28.0f

static int init(vp_game *g) { (void)g; models_init(); return VP_OK; }

/* Each model gets its own cell: one camera, the model moved across the
 * camera's plane so that it lands in the middle of its cell. */
static void draw(vp_game *g, vp_dvg *d)
{
    double t = vp_game_time(g);
    const char *fixed = getenv("VT_GALLERY_TIME");
    if (fixed) t = atof(fixed);
    const int rows = (N + COLS - 1) / COLS;
    const float cellw = (float)W / COLS, cellh = 3700.0f / rows;
    vp_text_font(VP_FONT_ASTEROIDS);
    for (int i = 0; i < N; i++) {
        float h = model_height(L[i].m);
        if (h < 8) h = 8;
        float c = L[i].y + h / 2;
        vp3d_view v;
        vp3d_view_init(&v, 30);
        vp_vec3 target = vp_v3(0, c, 0);
        /* far enough that the model is about a cell high */
        float far = h * 1.7f * rows * (4.0f / 3) / (2 * tanf(15 * 3.14159265f / 180));
        vp_vec3 eye = vp_v3_add(target, vp_v3_scale(vp_v3_normalize(vp_v3(0, 0.45f, -1)), far));
        vp3d_look_at(&v, eye, target, vp_v3(0, 1, 0));
        float sx = cellw * (i % COLS + 0.5f), sy = 3700.0f - cellh * (i / COLS + 0.5f) + 60;
        vp_vec3 fwd = vp_v3_normalize(vp_v3_sub(target, eye));
        vp_vec3 right = vp_v3_normalize(vp_v3_cross(vp_v3(0, 1, 0), fwd));
        vp_vec3 up = vp_v3_cross(fwd, right);
        float dist = vp_v3_length(vp_v3_sub(target, eye));
        float hw = dist * tanf(15 * 3.14159265f / 180), hh = hw / v.aspect;
        vp_vec3 move = vp_v3_add(vp_v3_scale(right, (sx - W / 2.0f) / (W / 2.0f) * hw),
                                 vp_v3_scale(up, (sy - W / 2.0f) / (W / 2.0f) * hh));
        model_pose p;
        memset(&p, 0, sizeof p);
        p.x = move.x;
        p.y = L[i].y + move.y;
        p.z = move.z;
        p.heading = (float)t * 0.8f + 3.14159f;
        p.walk = (float)t * 8;
        p.spin = (float)t * 2.5f;
        p.sxz = p.sy = 1;
        p.color = model_color(L[i].m);
        model_draw(d, &v, L[i].m, &p);
        vp_color(d, 150, 150, 150);
        vp_text(d, L[i].name, sx, sy - cellh * 0.42f, 55, VP_TEXT_CENTER);
    }
    vp_color(d, 255, 255, 0);
    vp_text(d, "VECTORTRON MODELS", W / 2.0f, 3900, 120, VP_TEXT_CENTER);
}

int main(int argc, char **argv)
{
    vp_game_desc desc = {0};
    desc.title = "Vectortron models";
    desc.init = init;
    desc.draw = draw;
    return vp_run(&desc, argc, argv);
}
