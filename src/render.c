/* render.c — camera, arena, effects, HUD. */
#include "render.h"
#include "font3d.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define W VP_DVG_COORD_MAX
#define PI 3.14159265f

static vp3d_view VIEW;
static vp_vec3 cam_eye, cam_target;
static int cam_ready;

/* ---- effects ------------------------------------------------------------- */

#define MAX_FRAGS  700
#define MAX_POPUPS 24
#define MAX_SKULLS 16

typedef struct {
    vp_vec3 p, v;
    float   ax, az;      /* half the segment, rotating */
    float   rot, vrot;
    float   life, max;
    rgb     c;
} frag;

typedef struct { float x, z, life; int value; } popup;
typedef struct { float x, z, life; } skull;

static frag   F[MAX_FRAGS];
static int    nF;
static popup  P[MAX_POPUPS];
static skull  K[MAX_SKULLS];
static float  flash_time;
static rgb    flash_color;
static unsigned fx_rng = 777;

static float frand(void)
{
    fx_rng = fx_rng * 1664525u + 1013904223u;
    return (float)(fx_rng >> 8) / 16777216.0f;
}

void render_init(void)
{
    vp3d_view_init(&VIEW, 60.0f);
    VIEW.near_z = 5.0f;
    VIEW.far_z = 2000.0f;
    cam_ready = 0;
    fx_clear();
}

void fx_clear(void)
{
    nF = 0;
    memset(P, 0, sizeof P);
    memset(K, 0, sizeof K);
    flash_time = 0;
}

/* The original blows a robot apart along the shot: slices fly out mostly
 * the way the shot was going. Here: short spinning segments, thrown along
 * (dirx, dirz) and a little up. */
void fx_explode(float x, float y, float z, rgb color, float dirx, float dirz, int pieces, float size)
{
    for (int i = 0; i < pieces; i++) {
        frag *f;
        if (nF < MAX_FRAGS) f = &F[nF++];
        else f = &F[(int)(frand() * MAX_FRAGS)];
        float along = (frand() * 2 - 1) * 2.2f;
        float side = (frand() * 2 - 1) * 0.7f;
        f->p = vp_v3(x + (frand() - 0.5f) * size * 0.5f, y + frand() * size, z + (frand() - 0.5f) * size * 0.5f);
        if (dirx == 0 && dirz == 0) {
            float a = frand() * 2 * PI;
            f->v = vp_v3(cosf(a) * 1.6f, 0.4f + frand() * 1.2f, sinf(a) * 1.6f);
        } else {
            f->v = vp_v3(dirx * along * 1.3f - dirz * side, 0.3f + frand() * 1.0f, dirz * along * 1.3f + dirx * side);
        }
        float len = size * (0.15f + frand() * 0.2f);
        float a = frand() * 2 * PI;
        f->ax = cosf(a) * len;
        f->az = sinf(a) * len;
        f->rot = 0;
        f->vrot = (frand() - 0.5f) * 0.5f;
        f->max = f->life = 30 + frand() * 25;
        f->c = color;
    }
}

void fx_popup(float x, float z, int value)
{
    int best = 0;
    for (int i = 0; i < MAX_POPUPS; i++) {
        if (P[i].life <= 0) { best = i; break; }
        if (P[i].life < P[best].life) best = i;
    }
    P[best].x = x;
    P[best].z = z;
    P[best].value = value;
    P[best].life = 90;
}

void fx_skull(float x, float z)
{
    int best = 0;
    for (int i = 0; i < MAX_SKULLS; i++) {
        if (K[i].life <= 0) { best = i; break; }
        if (K[i].life < K[best].life) best = i;
    }
    K[best].x = x;
    K[best].z = z;
    K[best].life = 90;
}

void fx_flash(rgb color, float frames)
{
    flash_color = color;
    flash_time = frames;
}

void fx_step(void)
{
    for (int i = 0; i < nF; ) {
        frag *f = &F[i];
        f->p = vp_v3_add(f->p, f->v);
        f->v.y -= 0.06f;
        if (f->p.y < 0) { f->p.y = 0; f->v.y = -f->v.y * 0.4f; f->v.x *= 0.7f; f->v.z *= 0.7f; }
        f->v.x *= 0.97f;
        f->v.z *= 0.97f;
        f->rot += f->vrot;
        if (--f->life <= 0) F[i] = F[--nF];
        else i++;
    }
    for (int i = 0; i < MAX_POPUPS; i++) if (P[i].life > 0) P[i].life--;
    for (int i = 0; i < MAX_SKULLS; i++) if (K[i].life > 0) K[i].life--;
    if (flash_time > 0) flash_time--;
}

/* ---- the camera ---------------------------------------------------------- */

static vp_vec3 lerp3(vp_vec3 a, vp_vec3 b, float k)
{
    return vp_v3(a.x + (b.x - a.x) * k, a.y + (b.y - a.y) * k, a.z + (b.z - a.z) * k);
}

void render_camera(const world *w, int mode, int snap)
{
    vp_vec3 eye, target;
    float px = w->px, pz = w->pz;
    switch (mode) {
    case 1: {   /* the whole arena, tilted, leaning toward the player */
        float tx = ARENA_CX + (px - ARENA_CX) * 0.22f;
        float tz = ARENA_CZ - 8 + (pz - ARENA_CZ) * 0.18f;
        target = vp_v3(tx, 0, tz);
        eye = vp_v3(tx, 245, tz - 175);
        VIEW.fov_deg = 60;
        break;
    }
    case 2:     /* the classic view, from (almost) straight above */
        target = vp_v3(ARENA_CX, 0, ARENA_CZ - 2);
        eye = vp_v3(ARENA_CX, 285, ARENA_CZ - 30);
        VIEW.fov_deg = 63;
        break;
    default: {  /* Robotron X: low behind the player, the far wall always in sight */
        float tx = px;
        if (tx < ARENA_X0 + 70) tx = ARENA_X0 + 70;
        if (tx > ARENA_X1 - 70) tx = ARENA_X1 - 70;
        float tz = ARENA_CZ + (pz - ARENA_CZ) * 0.55f + 10;
        target = vp_v3(tx, 0, tz);
        eye = vp_v3(tx, 120, tz - 150);
        VIEW.fov_deg = 66;
        break;
    }
    }
    if (snap || !cam_ready) {
        cam_eye = eye;
        cam_target = target;
        cam_ready = 1;
    } else {
        cam_eye = lerp3(cam_eye, eye, 0.06f);
        cam_target = lerp3(cam_target, target, 0.06f);
    }
    vp3d_look_at(&VIEW, cam_eye, cam_target, vp_v3(0, 1, 0));
    float dist = vp_v3_length(vp_v3_sub(cam_eye, cam_target));
    VIEW.fog_start = dist * 0.8f;
    VIEW.fog_end = dist * 2.2f;
    VIEW.fog_floor = 0.3f;
}

const vp3d_view *render_view(void) { return &VIEW; }

/* ---- the arena ----------------------------------------------------------- */

static rgb hsv(float h, float s, float v)
{
    h = fmodf(h, 1.0f);
    if (h < 0) h += 1;
    float r, g, b, f = h * 6;
    int i = (int)f;
    f -= i;
    float p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
    switch (i % 6) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
    }
    return mkrgb((int)(r * 255), (int)(g * 255), (int)(b * 255));
}

static void color(vp_dvg *d, rgb c) { vp_color(d, c.r, c.g, c.b); }

#define TILES_X 10
#define TILES_Z 8

static void draw_arena(vp_dvg *d, double t)
{
    const float x0 = ARENA_X0 - 4, x1 = ARENA_X1 + 4, z0 = ARENA_Z0 - 4, z1 = ARENA_Z1 + 4, h = 7;

    /* the floor: tiles with a gap between them, as in Robotron X; each tile
     * one closed stroke, walked in a snake so the beam's jumps stay short */
    const float tw = (x1 - x0) / TILES_X, td = (z1 - z0) / TILES_Z, gap = 1.6f;
    vp_color(d, 150, 130, 40);
    for (int j = 0; j < TILES_Z; j++) {
        for (int k = 0; k < TILES_X; k++) {
            int i = (j & 1) ? TILES_X - 1 - k : k;
            float ax = x0 + i * tw + gap, bx = x0 + (i + 1) * tw - gap;
            float az = z0 + j * td + gap, bz = z0 + (j + 1) * td - gap;
            vp3d_line(d, &VIEW, vp_v3(ax, 0, az), vp_v3(bx, 0, az));
            vp3d_line(d, &VIEW, vp_v3(bx, 0, az), vp_v3(bx, 0, bz));
            vp3d_line(d, &VIEW, vp_v3(bx, 0, bz), vp_v3(ax, 0, bz));
            vp3d_line(d, &VIEW, vp_v3(ax, 0, bz), vp_v3(ax, 0, az));
        }
    }

    /* the border: a low magenta wall that flashes on events */
    rgb c = flash_time > 0 && ((int)flash_time / 3) % 2 ? flash_color
          : hsv(0.83f + 0.03f * sinf((float)t * 0.7f), 0.85f, 1.0f);
    color(d, c);
    vp_vec3 lo[4] = { vp_v3(x0, 0, z0), vp_v3(x1, 0, z0), vp_v3(x1, 0, z1), vp_v3(x0, 0, z1) };
    vp_vec3 hi[4] = { vp_v3(x0, h, z0), vp_v3(x1, h, z0), vp_v3(x1, h, z1), vp_v3(x0, h, z1) };
    for (int i = 0; i < 4; i++) vp3d_line(d, &VIEW, lo[i], lo[(i + 1) % 4]);
    color(d, rgb_scale(c, 0.6f));
    for (int i = 0; i < 4; i++) vp3d_line(d, &VIEW, hi[i], hi[(i + 1) % 4]);
    for (int i = 0; i < 4; i++) vp3d_line(d, &VIEW, lo[i], hi[i]);
}

/* The score, the men left and the wave, standing on the far wall. */
void render_wall_hud(vp_dvg *d, const world *w, int player, int players, double t)
{
    char buf[32];
    const float zb = ARENA_Z1 + 8, base = 10;
    vp_vec3 right = vp_v3(1, 0, 0), up = vp_v3(0, 1, 0);

    vp_color(d, 255, 40, 60);
    snprintf(buf, sizeof buf, players > 1 ? "SCORE%d" : "SCORE", player + 1);
    font3d_text(d, &VIEW, buf, vp_v3(ARENA_X0 + 4, base, zb), right, up, 16, FONT3D_LEFT);
    vp_color(d, 60, 120, 255);
    snprintf(buf, sizeof buf, "%d", w->score);
    font3d_text(d, &VIEW, buf, vp_v3(ARENA_X0 + 4 + 16 * 0.8f * (players > 1 ? 6.5f : 5.5f), base, zb),
                right, up, 16, FONT3D_LEFT);

    /* the men left: little figures standing on the wall */
    model_pose p;
    memset(&p, 0, sizeof p);
    p.sxz = p.sy = 1;
    p.color = model_color(MDL_PLAYER);
    p.walk = 0;
    int men = w->lives - 1;
    if (men > 8) men = 8;
    for (int i = 0; i < men; i++) {
        p.x = ARENA_X1 - 100 + i * 11.0f;
        p.z = zb;
        p.y = base;
        p.heading = 3.14159265f + 0.4f * sinf((float)t * 2 + i);
        model_draw(d, &VIEW, MDL_PLAYER, &p);
    }
    vp_color(d, 80, 220, 120);
    snprintf(buf, sizeof buf, "WAVE %d", w->wave);
    font3d_text(d, &VIEW, buf, vp_v3(ARENA_X1 - 4, base + 22, zb), right, up, 9, FONT3D_RIGHT);
}

/* ---- things -------------------------------------------------------------- */

static model_id model_of(const ent *e, int wave)
{
    switch (e->type) {
    case T_GRUNT:    return MDL_GRUNT;
    case T_HULK:     return MDL_HULK;
    case T_BRAIN:    return MDL_BRAIN;
    case T_PROG:     return MDL_PROG;
    case T_SPHEROID: return MDL_SPHEROID;
    case T_ENFORCER: return MDL_ENFORCER;
    case T_QUARK:    return MDL_QUARK;
    case T_TANK:     return MDL_TANK;
    case T_MOMMY:    return MDL_MOMMY;
    case T_DADDY:    return MDL_DADDY;
    case T_MIKEY:    return MDL_MIKEY;
    case T_SPARK:    return MDL_SPARK;
    case T_SHELL:    return MDL_SHELL;
    case T_CRUISE:   return MDL_CRUISE;
    case T_ELECTRODE:
    default:         return (model_id)(MDL_ELECTRODE0 + wave_electrode_shape(wave));
    }
}

static float hover_of(int type)
{
    switch (type) {
    case T_SPHEROID: return 7;
    case T_ENFORCER: return 6;
    case T_QUARK:    return 7;
    case T_SPARK:    return 5;
    case T_SHELL:    return 3;
    case T_CRUISE:   return 6;
    default:         return 0;
    }
}

static rgb color_of(const ent *e, int wave, double t)
{
    switch (e->type) {
    case T_GRUNT:    return hsv(0.0f + 0.04f * sinf((float)t * 6 + e->x), 1, 1);
    case T_SPHEROID: return hsv((float)t * 0.8f, 0.8f, 1);
    case T_QUARK:    return hsv((float)t * 1.3f + e->x * 0.01f, 0.7f, 1);
    case T_SPARK:    return ((int)(t * 20) & 1) ? mkrgb(255, 255, 255) : mkrgb(255, 80, 255);
    case T_CRUISE:   return hsv((float)t * 2, 0.6f, 1);
    case T_ELECTRODE: {
        static const rgb ec[8] = { {255,140,0}, {0,200,255}, {255,60,255}, {255,255,40},
                                   {60,255,60}, {255,60,60}, {120,120,255}, {255,255,255} };
        return ec[(wave - 1) % 8];
    }
    default:         return model_color(model_of(e, wave));
    }
}

static int is_human(int type) { return type == T_MOMMY || type == T_DADDY || type == T_MIKEY; }

void render_world(vp_dvg *d, const world *w, double t)
{
    draw_arena(d, t);

    /* the materialize: everything builds itself out of tall thin lines */
    float u = 1;
    if (w->ph == PH_MATERIALIZE) {
        u = w->ph_time / 70.0f;
        if (u > 1) u = 1;
    }

    model_pose pose;
    memset(&pose, 0, sizeof pose);
    for (int i = 0; i < w->n; i++) {
        const ent *e = &w->e[i];
        if (e->type == T_NONE) continue;
        model_id m = model_of(e, w->wave);
        pose.x = e->x;
        pose.z = e->z;
        pose.y = hover_of(e->type);
        pose.heading = e->heading;
        pose.walk = e->walk;
        pose.spin = e->spin;
        pose.color = color_of(e, w->wave, t);
        pose.flat_color = 0;
        float eu = u;
        /* things born during play (enforcers, tanks) grow in too */
        if (e->age < 20 && w->ph != PH_MATERIALIZE &&
            (e->type == T_ENFORCER || e->type == T_TANK || e->type == T_PROG)) eu = e->age / 20.0f;
        if (eu < 1) {
            /* each thing arrives at its own moment */
            float k = eu * 1.6f - (float)((i * 37) % 23) / 23.0f * 0.6f;
            if (k < 0) continue;
            if (k > 1) k = 1;
            pose.sxz = 0.15f + 0.85f * k;
            pose.sy = 1 + 5 * (1 - k);
            pose.flat_color = 1;
            pose.color = rgb_scale(pose.color, 0.4f + 0.6f * k);
        } else {
            pose.sxz = pose.sy = 1;
        }
        if (e->type == T_SPHEROID) {
            float pulse = 0.75f + 0.3f * sinf((float)t * 9 + e->x);
            pose.sxz *= pulse;
            pose.sy *= pulse;
        }
        if (e->type == T_PROG) {
            /* the prog's trail: dim copies behind it */
            model_pose ghost = pose;
            float sp = sqrtf(e->vx * e->vx + e->vz * e->vz);
            if (sp > 0.01f) {
                for (int k = 3; k >= 1; k--) {
                    ghost.x = e->x - e->vx / sp * 5.0f * k;
                    ghost.z = e->z - e->vz / sp * 5.0f * k;
                    ghost.color = rgb_scale(hsv(0.8f + 0.1f * k + (float)t, 0.8f, 1), 0.7f - 0.15f * k);
                    ghost.flat_color = 1;
                    model_draw(d, &VIEW, m, &ghost);
                }
            }
        }
        if (e->type == T_CRUISE) {
            vp_color(d, 140, 60, 255);
            vp3d_line(d, &VIEW, vp_v3(e->x, pose.y, e->z),
                      vp_v3(e->x - e->vx * 10, pose.y, e->z - e->vz * 10));
        }
        if (is_human(e->type) && e->state == 1) {
            /* a human a brain has caught: flashing, about to turn */
            pose.flat_color = 1;
            pose.color = ((int)(t * 15) & 1) ? mkrgb(255, 255, 255) : mkrgb(200, 60, 255);
        }
        model_draw(d, &VIEW, m, &pose);
    }

    /* the player */
    int show = 1;
    if (w->ph == PH_DYING) show = ((int)(w->ph_time / 4) & 1) == 0 && w->ph_time < 90;
    if (w->ph == PH_OVER || w->ph == PH_CLEARED) show = w->ph != PH_OVER;
    if (show) {
        pose.x = w->px;
        pose.z = w->pz;
        pose.y = 0;
        pose.heading = w->pheading;
        pose.walk = w->pwalk;
        pose.spin = 0;
        pose.sxz = pose.sy = 1;
        pose.flat_color = 0;
        pose.color = model_color(MDL_PLAYER);
        if (w->ph == PH_MATERIALIZE && u < 1) {
            pose.sxz = 0.2f + 0.8f * u;
            pose.sy = 1 + 4 * (1 - u);
            pose.flat_color = 1;
        }
        if (w->ph == PH_DYING) {
            pose.flat_color = 1;
            pose.color = hsv(w->ph_time * 0.05f, 1, 1);
            float k = w->ph_time / 90.0f;
            pose.sxz = 1 + k * 1.5f;
            pose.sy = 1 - k * 0.8f;
        }
        model_draw(d, &VIEW, MDL_PLAYER, &pose);
    }

    /* the player's shots: bright streaks */
    vp_color(d, 255, 255, 255);
    for (int i = 0; i < MAX_SHOTS; i++) {
        const shot *s = &w->shots[i];
        if (!s->live) continue;
        vp3d_line(d, &VIEW, vp_v3(s->x - s->dx * 1.6f, 7, s->z - s->dz * 1.6f),
                  vp_v3(s->x + s->dx * 1.6f, 7, s->z + s->dz * 1.6f));
    }

    /* skulls where humans died */
    for (int i = 0; i < MAX_SKULLS; i++) {
        if (K[i].life <= 0) continue;
        pose.x = K[i].x;
        pose.z = K[i].z;
        pose.y = 0;
        pose.heading = 0;
        pose.sxz = pose.sy = 1;
        pose.flat_color = 1;
        pose.color = rgb_scale(mkrgb(255, 255, 255), K[i].life / 40.0f);
        model_draw(d, &VIEW, MDL_SKULL, &pose);
    }

    /* fragments */
    rgb last = mkrgb(1, 2, 3);
    for (int i = 0; i < nF; i++) {
        const frag *f = &F[i];
        rgb c = rgb_scale(f->c, f->life / f->max * 1.4f);
        if (c.r != last.r || c.g != last.g || c.b != last.b) { color(d, c); last = c; }
        float cr = cosf(f->rot), sr = sinf(f->rot);
        float ax = f->ax * cr - f->az * sr, az = f->ax * sr + f->az * cr;
        vp3d_line(d, &VIEW, vp_v3(f->p.x - ax, f->p.y, f->p.z - az), vp_v3(f->p.x + ax, f->p.y + 1, f->p.z + az));
    }

    /* score pop-ups (the rescue bonus) */
    vp_text_font(VP_FONT_ASTEROIDS);
    for (int i = 0; i < MAX_POPUPS; i++) {
        float sx, sy;
        if (P[i].life <= 0) continue;
        if (!vp3d_project(&VIEW, vp_v3(P[i].x, 16 + (90 - P[i].life) * 0.08f, P[i].z), &sx, &sy)) continue;
        char buf[16];
        snprintf(buf, sizeof buf, "%d", P[i].value);
        rgb c = hsv((float)t * 1.5f, 0.6f, 1);
        text(d, rgb_scale(c, P[i].life / 30.0f), buf, sx, sy, 70, VP_TEXT_CENTER);
    }
}

/* ---- text, HUD, the tunnel ----------------------------------------------- */

void text(vp_dvg *d, rgb c, const char *s, float x, float y, float h, int align)
{
    color(d, c);
    vp_text(d, s, x, y, h, align);
}

static void draw_man_icon(vp_dvg *d, float x, float y, float s)
{
    /* a tiny stick figure for the men left */
    vp_moveto(d, (int)(x - s * 0.5f), (int)y);
    vp_lineto(d, (int)x, (int)(y + s * 0.9f));
    vp_lineto(d, (int)(x + s * 0.5f), (int)y);
    vp_moveto(d, (int)x, (int)(y + s * 0.9f));
    vp_lineto(d, (int)x, (int)(y + s * 1.7f));
    vp_moveto(d, (int)(x - s * 0.5f), (int)(y + s * 1.4f));
    vp_lineto(d, (int)(x + s * 0.5f), (int)(y + s * 1.4f));
    vp_moveto(d, (int)(x - s * 0.25f), (int)(y + s * 1.7f));
    vp_lineto(d, (int)(x + s * 0.25f), (int)(y + s * 1.7f));
    vp_lineto(d, (int)(x + s * 0.25f), (int)(y + s * 2.2f));
    vp_lineto(d, (int)(x - s * 0.25f), (int)(y + s * 2.2f));
    vp_lineto(d, (int)(x - s * 0.25f), (int)(y + s * 1.7f));
}

void render_hud(vp_dvg *d, const world *w1, const world *w2, int players, int current, int hiscore,
                double t)
{
    char buf[32];
    vp_text_font(VP_FONT_ASTEROIDS);
    const world *ws[2] = { w1, w2 };
    for (int p = 0; p < players; p++) {
        const world *w = ws[p];
        float x = p == 0 ? 120.0f : W - 120.0f;
        int align = p == 0 ? VP_TEXT_LEFT : VP_TEXT_RIGHT;
        rgb c = (p == current) ? mkrgb(255, 40, 40) : mkrgb(140, 30, 30);
        snprintf(buf, sizeof buf, "%d", w->score);
        text(d, c, buf, x, 3900, 110, align);
        vp_color(d, 255, 255, 255);
        int men = w->lives - (p == current ? 1 : 0);
        if (men < 0) men = 0;
        if (men > 10) men = 10;
        for (int i = 0; i < men; i++) {
            float ix = p == 0 ? x + 20 + i * 70.0f : x - 20 - i * 70.0f;
            draw_man_icon(d, ix, 3780, 40);
        }
    }
    snprintf(buf, sizeof buf, "HI %d", hiscore);
    text(d, mkrgb(200, 200, 255), buf, W / 2.0f, 3900, 90, VP_TEXT_CENTER);
    const world *w = ws[current];
    snprintf(buf, sizeof buf, "%d WAVE", w->wave);
    text(d, mkrgb(80, 160, 255), buf, W / 2.0f, 80, 90, VP_TEXT_CENTER);
    (void)t;
}

void render_wave_tunnel(vp_dvg *d, float u, double t)
{
    /* nested rectangles rushing out from the middle, in cycling colors */
    for (int i = 0; i < 10; i++) {
        float k = u * 1.6f - i * 0.07f;
        if (k <= 0 || k > 1.2f) continue;
        float hw = k * 2300, hh = k * 1800;
        rgb c = hsv((float)t * 0.5f + i * 0.1f, 1, 1);
        color(d, c);
        int x0 = (int)(W / 2 - hw), x1 = (int)(W / 2 + hw), y0 = (int)(W / 2 - hh), y1 = (int)(W / 2 + hh);
        vp_moveto(d, x0, y0);
        vp_lineto(d, x1, y0);
        vp_lineto(d, x1, y1);
        vp_lineto(d, x0, y1);
        vp_lineto(d, x0, y0);
    }
}
