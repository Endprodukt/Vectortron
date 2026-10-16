/* render.c — camera, arena, effects, HUD.
 *
 * The arena is drawn through one vp3d scene per frame: the walls,
 * every robot and person, the shots and the debris. Solid things hide what is
 * behind them; the scene sends only what can be seen.
 */
#include "render.h"
#include "font3d.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define W VP_DVG_COORD_MAX
#define PI 3.14159265f

static vp3d_view VIEW;

static vp3d_scene *SCENE;

/* the camera, smoothed: what it looks at, from which side, how far */
static struct {
    vp_vec3 target;
    float   yaw, pitch, dist;
    int     ready;
    float   clock;
    float   pvx, pvz, lastx, lastz;
} CAM;

/* ---- the stage's colors --------------------------------------------------- */

/* The original gives every wave its own wall color and its own laser flash
 * color, in a cycle of ten (GTWCOL, RRG23.ASM:373-401). The palette's RGB
 * values are not in the source; these follow its spirit. */
typedef struct { rgb wall, floor, laser; } stage;

static const stage STAGES[10] = {
    { {255,   0, 200}, {150, 130,  40}, {255, 255, 255} },
    { {  0, 200, 255}, { 40,  90, 170}, {255, 255,   0} },
    { {255,  70,   0}, {150,  60,  20}, {  0, 255, 255} },
    { {  0, 255,  90}, { 30, 130,  60}, {255,   0, 255} },
    { {255, 230,   0}, {130, 115,  20}, {255,  80,  80} },
    { {130,  80, 255}, { 80,  55, 150}, {120, 255, 120} },
    { {255,   0,  60}, {140,  20,  50}, {255, 255, 255} },
    { {  0, 255, 255}, { 20, 115, 125}, {255, 160,   0} },
    { {255, 140,   0}, {140,  85,  20}, {160, 160, 255} },
    { {255, 255, 255}, { 95,  95, 120}, {255,   0, 128} },
};

static int stage_index;

static const stage *ST(void) { return &STAGES[stage_index]; }

void render_set_wave(int wave) { stage_index = ((wave > 0 ? wave : 1) - 1) % 10; }

rgb render_laser_color(void) { return ST()->laser; }

/* ---- effects ------------------------------------------------------------- */

#define MAX_FRAGS  300      /* the beam's budget: new ones replace old ones */
#define MAX_RINGS  24
#define MAX_POPUPS 24
#define MAX_SKULLS 16

enum { FR_DEBRIS, FR_SPARK, FR_EMBER };

typedef struct {
    vp_vec3 p, v;
    float   ax, az;      /* debris: half the segment, rotating */
    float   rot, vrot;
    float   life, max;
    rgb     c;
    int     kind;
} frag;

/* a shock wave: a circle on the floor, growing and fading */
typedef struct { float x, y, z, r, vr, life, max; rgb c; } ring;

typedef struct { float x, z, life; int value; } popup;

/* an electrode being destroyed: the original's three shrinking images
 * (6, 3 and 2 frames) */
typedef struct { float x, z, life; int shape; rgb c; } shrink;
#define MAX_SHRINKS 16
static shrink SH[MAX_SHRINKS];
typedef struct { float x, z, life; } skull;

static frag   F[MAX_FRAGS];
static int    nF;
static ring   R[MAX_RINGS];
static popup  P[MAX_POPUPS];
static skull  K[MAX_SKULLS];
static float  flash_time;
static rgb    flash_color;
static float  shake;
static char   banner[32];
static rgb    banner_color;
static float  banner_time, banner_max;
static unsigned fx_rng = 777;

static int fx_level = 2;            /* 0 off, 1 light, 2 full */

void fx_set_level(int level) { fx_level = level; }

/* fewer particles at a lighter setting */
static int lvl(int n) { return fx_level >= 2 ? n : fx_level == 1 ? (n + 1) / 2 : 0; }

static float frand(void)
{
    fx_rng = fx_rng * 1664525u + 1013904223u;
    return (float)(fx_rng >> 8) / 16777216.0f;
}

void render_init(void)
{
    vp3d_view_init(&VIEW, 60.0f);
    VIEW.near_z = 4.0f;
    VIEW.far_z = 2000.0f;
    if (!SCENE) vp3d_scene_create(&SCENE);
    CAM.ready = 0;
    fx_clear();
}

void fx_clear(void)
{
    nF = 0;
    memset(R, 0, sizeof R);
    memset(SH, 0, sizeof SH);
    memset(P, 0, sizeof P);
    memset(K, 0, sizeof K);
    flash_time = 0;
    shake = 0;
    banner_time = 0;
}

static frag *new_frag(void)
{
    if (nF < MAX_FRAGS) return &F[nF++];
    return &F[(int)(frand() * MAX_FRAGS)];
}

/* Bright streaks thrown out fast, Tempest 2000 style: each is drawn along
 * its own velocity, white hot first, then its color, then gone. */
void fx_sparks(float x, float y, float z, rgb color, int count, float speed)
{
    count = lvl(count);
    for (int i = 0; i < count; i++) {
        frag *f = new_frag();
        float a = frand() * 2 * PI, up = frand() * 0.9f - 0.2f;
        float s = speed * (0.4f + frand() * 0.8f);
        f->p = vp_v3(x, y, z);
        f->v = vp_v3(cosf(a) * s, up * s, sinf(a) * s);
        f->kind = FR_SPARK;
        f->max = f->life = 10 + frand() * 12;
        f->c = color;
    }
}

/* Sparks in a cone along (dx, dz): a muzzle flash, a shot hitting a wall. */
static void sparks_dir(float x, float y, float z, rgb color, int count, float speed, float dx, float dz,
                       float spread)
{
    count = lvl(count);
    for (int i = 0; i < count; i++) {
        frag *f = new_frag();
        float a = atan2f(dz, dx) + (frand() - 0.5f) * spread;
        float s = speed * (0.5f + frand() * 0.8f);
        f->p = vp_v3(x, y, z);
        f->v = vp_v3(cosf(a) * s, (frand() - 0.3f) * s * 0.5f, sinf(a) * s);
        f->kind = FR_SPARK;
        f->max = f->life = 6 + frand() * 10;
        f->c = color;
    }
}

void fx_ring(float x, float z, rgb color, float speed, float frames)
{
    if (fx_level == 0) return;
    int best = 0;
    for (int i = 0; i < MAX_RINGS; i++) {
        if (R[i].life <= 0) { best = i; break; }
        if (R[i].life < R[best].life) best = i;
    }
    ring *r = &R[best];
    r->x = x;
    r->y = 0.5f;
    r->z = z;
    r->r = 2;
    r->vr = speed;
    r->max = r->life = frames;
    r->c = color;
}

void fx_shake(float amount)
{
    if (fx_level == 0) return;
    if (amount > shake) shake = amount;
}

void fx_banner(const char *s, rgb color, float frames)
{
    snprintf(banner, sizeof banner, "%s", s);
    banner_color = color;
    banner_max = banner_time = frames;
}

/* The original blows a robot apart along the shot: slices fly out mostly
 * the way the shot was going. Here: spinning debris thrown along (dirx,
 * dirz), a spray of sparks, a shock wave on the floor and a jolt. */
void fx_explode(float x, float y, float z, rgb color, float dirx, float dirz, int pieces, float size)
{
    int debris = (pieces + 1) / 2;
    for (int i = 0; i < debris; i++) {
        frag *f = new_frag();
        float along = (frand() * 2 - 1) * 2.2f;
        float side = (frand() * 2 - 1) * 0.7f;
        f->p = vp_v3(x + (frand() - 0.5f) * size * 0.5f, y + frand() * size, z + (frand() - 0.5f) * size * 0.5f);
        if (dirx == 0 && dirz == 0) {
            float a = frand() * 2 * PI;
            f->v = vp_v3(cosf(a) * 1.6f, 0.4f + frand() * 1.2f, sinf(a) * 1.6f);
        } else {
            f->v = vp_v3(dirx * (1.0f + along) * 1.3f - dirz * side, 0.3f + frand() * 1.0f,
                         dirz * (1.0f + along) * 1.3f + dirx * side);
        }
        float len = size * (0.15f + frand() * 0.2f);
        float a = frand() * 2 * PI;
        f->ax = cosf(a) * len;
        f->az = sinf(a) * len;
        f->rot = 0;
        f->vrot = (frand() - 0.5f) * 0.5f;
        f->max = f->life = 30 + frand() * 25;
        f->c = color;
        f->kind = FR_DEBRIS;
    }
    fx_sparks(x, y + size * 0.5f, z, color, pieces / 2 + 1, 2.2f + size * 0.08f);
    fx_sparks(x, y + size * 0.5f, z, mkrgb(255, 255, 255), pieces / 6 + 1, 3.0f);
    if (size >= 14) fx_ring(x, z, color, 0.9f + size * 0.04f, 22);
    fx_shake(size > 14 ? 2.5f : 1.0f);
}

void fx_muzzle(float x, float z, float dx, float dz)
{
    sparks_dir(x + dx * 5, 7, z + dz * 5, ST()->laser, 2, 2.2f, dx, dz, 1.2f);
}

void fx_shot_trail(float x, float z, float dx, float dz)
{
    if (fx_level < 2) return;
    frag *f = new_frag();
    f->p = vp_v3(x - dx * 4 + (frand() - 0.5f) * 1.5f, 7 + (frand() - 0.5f) * 1.5f, z - dz * 4 + (frand() - 0.5f) * 1.5f);
    f->v = vp_v3(-dx * 0.6f + (frand() - 0.5f) * 0.5f, (frand() - 0.5f) * 0.4f, -dz * 0.6f + (frand() - 0.5f) * 0.5f);
    f->kind = FR_SPARK;
    f->max = f->life = 6 + frand() * 6;
    f->c = ST()->laser;
}

/* A shot hitting the wall: sparks back off it, and the wall flashes in the
 * wave's laser color (the original's laser-wall flash). */
void fx_wall_hit(float x, float z, float dx, float dz)
{
    sparks_dir(x, 7, z, ST()->laser, 4, 2.0f, -dx, -dz, 2.4f);
    fx_flash(ST()->laser, 4);
}

void fx_rescue(float x, float z)
{
    fx_ring(x, z, mkrgb(255, 255, 255), 1.4f, 26);
    fx_ring(x, z, mkrgb(80, 200, 255), 0.8f, 34);
    for (int i = 0; i < lvl(24); i++) {     /* a rising spiral of embers */
        frag *f = new_frag();
        float a = i * 0.52f;
        f->p = vp_v3(x + cosf(a) * 4, 1 + i * 0.4f, z + sinf(a) * 4);
        f->v = vp_v3(cosf(a + 1.6f) * 0.5f, 0.45f + frand() * 0.3f, sinf(a + 1.6f) * 0.5f);
        f->kind = FR_EMBER;
        f->max = f->life = 30 + frand() * 20;
        f->c = (i & 1) ? mkrgb(255, 255, 120) : mkrgb(120, 220, 255);
    }
}

void fx_player_dies(float x, float z)
{
    static const rgb C[4] = { {255,255,255}, {255,60,60}, {255,230,0}, {80,160,255} };
    for (int k = 0; k < 4; k++) fx_sparks(x, 7, z, C[k], 30, 2.0f + k * 0.6f);
    for (int k = 0; k < 3; k++) fx_ring(x, z, C[k + 1], 1.0f + k * 0.7f, 40 + k * 10);
    fx_shake(6);
    fx_flash(mkrgb(255, 255, 255), 30);
}

void fx_wave_start(float x, float z)
{
    for (int k = 0; k < 2; k++) fx_ring(x, z, ST()->wall, 2.2f + k * 0.8f, 60);
}

void fx_fireworks(void)
{
    for (int k = 0; k < 6; k++) {
        float x = ARENA_X0 + 30 + frand() * (ARENA_X1 - ARENA_X0 - 60);
        float z = ARENA_Z0 + 30 + frand() * (ARENA_Z1 - ARENA_Z0 - 60);
        rgb c = STAGES[(int)(frand() * 10)].wall;
        fx_sparks(x, 20 + frand() * 20, z, c, 26, 2.4f);
        fx_ring(x, z, c, 1.6f, 30);
    }
}

void fx_electrode_shrink(float x, float z, int shape, rgb c)
{
    for (int i = 0; i < MAX_SHRINKS; i++) {
        if (SH[i].life > 0) continue;
        SH[i].x = x;
        SH[i].z = z;
        SH[i].shape = shape;
        SH[i].c = c;
        SH[i].life = 11;
        break;
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
    P[best].life = 60;          /* the original shows it 60 frames */
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
    K[best].life = 90;          /* 90 frames, as the original */
    fx_ring(x, z, mkrgb(255, 40, 40), 0.7f, 30);
}

void fx_flash(rgb color, float frames)
{
    flash_color = color;
    flash_time = frames;
}

static void keep_inside(frag *f)
{
    if (f->p.x < ARENA_X0 - 3 || f->p.x > ARENA_X1 + 3) {
        f->v.x = -f->v.x * 0.5f;
        f->p.x = f->p.x < ARENA_X0 ? ARENA_X0 - 3 : ARENA_X1 + 3;
    }
    if (f->p.z < ARENA_Z0 - 3 || f->p.z > ARENA_Z1 + 3) {
        f->v.z = -f->v.z * 0.5f;
        f->p.z = f->p.z < ARENA_Z0 ? ARENA_Z0 - 3 : ARENA_Z1 + 3;
    }
}

void fx_step(void)
{
    for (int i = 0; i < nF; ) {
        frag *f = &F[i];
        f->p = vp_v3_add(f->p, f->v);
        switch (f->kind) {
        case FR_DEBRIS:
            f->v.y -= 0.06f;
            if (f->p.y < 0) { f->p.y = 0; f->v.y = -f->v.y * 0.4f; f->v.x *= 0.7f; f->v.z *= 0.7f; }
            f->v.x *= 0.97f;
            f->v.z *= 0.97f;
            f->rot += f->vrot;
            break;
        case FR_SPARK:
            f->v.y -= 0.03f;
            if (f->p.y < 0) { f->p.y = 0; f->v.y = -f->v.y * 0.5f; }
            f->v = vp_v3_scale(f->v, 0.93f);
            break;
        default:        /* embers rise and slow */
            f->v = vp_v3_scale(f->v, 0.98f);
            break;
        }
        keep_inside(f);
        if (--f->life <= 0) F[i] = F[--nF];
        else i++;
    }
    for (int i = 0; i < MAX_RINGS; i++) {
        if (R[i].life <= 0) continue;
        R[i].r += R[i].vr;
        R[i].life--;
    }
    for (int i = 0; i < MAX_POPUPS; i++) if (P[i].life > 0) P[i].life--;
    for (int i = 0; i < MAX_SKULLS; i++) if (K[i].life > 0) K[i].life--;
    for (int i = 0; i < MAX_SHRINKS; i++) if (SH[i].life > 0) SH[i].life--;
    if (flash_time > 0) flash_time--;
    if (banner_time > 0) banner_time--;
    shake *= 0.88f;
    if (shake < 0.05f) shake = 0;
}

/* ---- the camera ---------------------------------------------------------- */

static float approach(float cur, float goal, float k) { return cur + (goal - cur) * k; }

static float angle_approach(float cur, float goal, float k)
{
    float d = goal - cur;
    while (d > PI) d -= 2 * PI;
    while (d < -PI) d += 2 * PI;
    return cur + d * k;
}

static int is_threat(int t) { return t != T_NONE && t != T_MOMMY && t != T_DADDY && t != T_MIKEY && t != T_ELECTRODE; }

/* Robotron X's camera never stands still: it leans toward the player and
 * the way they run, swings round as they cross the arena, backs off when
 * the robots crowd in, swoops down as a wave materializes and closes in
 * when the player dies. */
void render_camera(const world *w, int mode, int snap)
{
    float px = w->px, pz = w->pz;
    CAM.clock += 1;
    if (!CAM.ready || snap) { CAM.lastx = px; CAM.lastz = pz; CAM.pvx = CAM.pvz = 0; }
    CAM.pvx = approach(CAM.pvx, px - CAM.lastx, 0.05f);
    CAM.pvz = approach(CAM.pvz, pz - CAM.lastz, 0.05f);
    CAM.lastx = px;
    CAM.lastz = pz;

    float hw = (ARENA_X1 - ARENA_X0) / 2, hd = (ARENA_Z1 - ARENA_Z0) / 2;
    float ux = (px - ARENA_CX) / hw, uz = (pz - ARENA_CZ) / hd;   /* -1..1 across the arena */
    vp_vec3 target;
    float yaw, pitch, dist, fov;

    if (mode == 2) {            /* the classic view, from (almost) straight above */
        target = vp_v3(ARENA_CX, 0, ARENA_CZ - 2);
        yaw = 0;
        pitch = 1.40f;
        dist = 292;
        fov = 63;
    } else if (mode == 1) {     /* the whole arena, tilted, swaying a little */
        target = vp_v3(ARENA_CX + ux * 30, 0, ARENA_CZ - 6 + uz * 20);
        yaw = ux * 0.18f + 0.04f * sinf(CAM.clock * 0.004f);
        pitch = 0.95f;
        dist = 300;
        fov = 60;
    } else {                    /* Robotron X */
        int crowd = 0;
        for (int i = 0; i < w->n; i++) {
            const ent *e = &w->e[i];
            if (!is_threat(e->type)) continue;
            float dx = e->x - px, dz = e->z - pz;
            if (dx * dx + dz * dz < 70 * 70) crowd++;
        }
        target = vp_v3(px * 0.6f + ARENA_CX * 0.4f + CAM.pvx * 25,
                       0,
                       pz * 0.45f + ARENA_CZ * 0.55f + CAM.pvz * 20 + 14);
        yaw = -ux * 0.30f + 0.06f * sinf(CAM.clock * 0.006f);    /* swing round to look across the arena */
        pitch = 0.84f - uz * 0.08f;
        dist = 185 + (crowd > 12 ? 12 : crowd) * 5;
        fov = 66;

        if (w->ph == PH_MATERIALIZE && w->mat_len > 0) {
            /* swoop in from high and round to the side */
            float u = (float)w->ph_time / w->mat_len, k = (1 - u) * (1 - u);
            yaw += k * 0.8f;
            pitch += k * 0.3f;
            dist *= 1 + k * 0.7f;
        } else if (w->ph == PH_DYING) {
            /* close in on the player and circle */
            float k = w->ph_time / 150.0f;
            target = vp_v3(px, 4, pz);
            yaw += k * 2.0f;
            pitch = 0.78f;
            dist = 150 - 70 * (k < 1 ? k : 1);
        } else if (w->ph == PH_CLEARED) {
            pitch += w->ph_time * 0.006f;
            dist += w->ph_time * 1.2f;
        }
    }
    if (pitch > 1.5f) pitch = 1.5f;

    if (!CAM.ready || snap) {
        CAM.target = target;
        CAM.yaw = yaw;
        CAM.pitch = pitch;
        CAM.dist = dist;
        CAM.ready = 1;
    } else {
        float k = (w->ph == PH_DYING) ? 0.07f : 0.045f;
        CAM.target.x = approach(CAM.target.x, target.x, k);
        CAM.target.y = approach(CAM.target.y, target.y, k);
        CAM.target.z = approach(CAM.target.z, target.z, k);
        CAM.yaw = angle_approach(CAM.yaw, yaw, k * 0.7f);
        CAM.pitch = approach(CAM.pitch, pitch, k);
        CAM.dist = approach(CAM.dist, dist, k * 0.8f);
    }
    VIEW.fov_deg = fov;
    vp_vec3 look = CAM.target;
    if (shake > 0) {
        look.x += (frand() - 0.5f) * shake * 2;
        look.y += (frand() - 0.5f) * shake;
        look.z += (frand() - 0.5f) * shake * 2;
    }
    float cp = cosf(CAM.pitch), sp = sinf(CAM.pitch);
    vp_vec3 eye = vp_v3(look.x - sinf(CAM.yaw) * cp * CAM.dist,
                        look.y + sp * CAM.dist,
                        look.z - cosf(CAM.yaw) * cp * CAM.dist);
    vp3d_look_at(&VIEW, eye, look, vp_v3(0, 1, 0));
    VIEW.fog_start = CAM.dist * 0.75f;
    VIEW.fog_end = CAM.dist * 2.3f;
    VIEW.fog_floor = 0.3f;
}

const vp3d_view *render_view(void) { return &VIEW; }

vp3d_scene *render_scene(void) { return SCENE; }

/* ---- colors ------------------------------------------------------------ */

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

/* ---- the arena ----------------------------------------------------------- */

#define WALL_H  7.0f

static void arena_into_scene(vp_dvg *d, double t)
{
    const float x0 = ARENA_X0 - 4, x1 = ARENA_X1 + 4, z0 = ARENA_Z0 - 4, z1 = ARENA_Z1 + 4;

    /* no floor: a vector monitor has only so much beam time a frame, and a
     * full wave takes it all (the tiles of Robotron X cost the most) */

    /* the border: a low magenta wall that flashes on events; solid, so the
     * near wall hides the feet of whoever stands behind it */
    rgb c = flash_time > 0 && ((int)flash_time / 3) % 2 == 0 ? flash_color
          : rgb_scale(ST()->wall, 0.8f + 0.2f * sinf((float)t * 3.0f));
    color(d, c);
    vp_vec3 lo[4] = { vp_v3(x0, 0, z0), vp_v3(x1, 0, z0), vp_v3(x1, 0, z1), vp_v3(x0, 0, z1) };
    for (int i = 0; i < 4; i++) {
        vp_vec3 a = lo[i], b = lo[(i + 1) % 4];
        vp_vec3 q[4] = { a, b, vp_v3(b.x, WALL_H, b.z), vp_v3(a.x, WALL_H, a.z) };
        vp3d_scene_polygon(SCENE, q, 4, VP3D_ALL_EDGES);
    }
}

/* The score, the men left and the wave, standing on the far wall: fixed to
 * it, the score at its left end, the wave and the men (a number) at its
 * right. With two players both scores stand there, 1UP and 2UP, the one
 * playing blinking. */
void render_wall_hud(vp_dvg *d, const world *w1, const world *w2, int players, int current, double t)
{
    char buf[32];
    const world *w = current ? w2 : w1;
    const float zb = ARENA_Z1 + 8, base = 10;
    const float h = 14;                         /* letter height */
    const float x0 = ARENA_X0, row = ARENA_X1 - ARENA_X0;
    vp_vec3 right = vp_v3(1, 0, 0), up = vp_v3(0, 1, 0);

    if (players < 2) {
        /* the score alone, as the original shows it (a word costs 40 lines) */
        vp_color(d, 60, 120, 255);
        snprintf(buf, sizeof buf, "%d", w->score);
        font3d_text(d, &VIEW, buf, vp_v3(x0, base, zb), right, up, h, FONT3D_LEFT);
    } else {
        const world *ws[2] = { w1, w2 };
        for (int p = 0; p < 2; p++) {
            float px = p == 0 ? x0 : x0 + row * 0.42f;
            int blink = p == current && ((int)(t * 3) & 1);
            rgb c = p == 0 ? mkrgb(255, 40, 60) : mkrgb(60, 140, 255);
            if (p != current) c = rgb_scale(c, 0.45f);
            if (!blink) {
                color(d, c);
                font3d_text(d, &VIEW, p == 0 ? "1UP" : "2UP", vp_v3(px, base + h * 1.2f, zb), right, up, h * 0.7f, FONT3D_LEFT);
            }
            color(d, p == current ? mkrgb(255, 255, 255) : mkrgb(120, 120, 140));
            snprintf(buf, sizeof buf, "%d", ws[p]->score);
            font3d_text(d, &VIEW, buf, vp_v3(px, base, zb), right, up, h, FONT3D_LEFT);
        }
    }

    /* the men left, as a number */
    int men = w->lives - 1;
    if (men < 0) men = 0;
    color(d, model_color(MDL_PLAYER));
    snprintf(buf, sizeof buf, "MEN %d", men);
    font3d_text(d, &VIEW, buf, vp_v3(x0 + row, base, zb), right, up, h * 0.8f, FONT3D_RIGHT);
    vp_color(d, 80, 220, 120);
    snprintf(buf, sizeof buf, "WAVE %d", w->wave);
    font3d_text(d, &VIEW, buf, vp_v3(x0 + row, base + h * (players > 1 ? 1.7f : 1.4f), zb), right, up, h * 0.57f, FONT3D_RIGHT);
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
    case T_SPHEROID: return 8;
    case T_ENFORCER: return 6;
    case T_QUARK:    return 8;
    case T_SPARK:    return 5;
    case T_SHELL:    return 3;
    case T_CRUISE:   return 6;
    default:         return 0;
    }
}

static rgb electrode_color(int wave)
{
    /* a color of its own every wave, as the original's post colors */
    static const rgb ec[10] = { {255,140,0}, {0,200,255}, {255,60,255}, {255,255,40}, {60,255,60},
                                {255,60,60}, {120,120,255}, {255,255,255}, {0,255,200}, {255,160,200} };
    return ec[((wave > 0 ? wave : 1) - 1) % 10];
}

rgb render_electrode_color(int wave) { return electrode_color(wave); }

static rgb color_of(const ent *e, int wave, double t)
{
    switch (e->type) {
    case T_SPHEROID: return ((int)(t * 12 + e->x) & 1) ? mkrgb(255, 30, 20) : mkrgb(255, 110, 40);
    case T_SPARK:    return ((int)(t * 20) & 1) ? mkrgb(255, 40, 40) : mkrgb(255, 200, 80);
    case T_CRUISE:   return hsv((float)t * 2, 0.6f, 1);
    case T_ELECTRODE: return electrode_color(wave);
    default:         return model_color(model_of(e, wave));
    }
}

static int is_human(int type) { return type == T_MOMMY || type == T_DADDY || type == T_MIKEY; }

/* The appear effect: 0 not there yet, 1 complete. */
static float appear(const world *w, const ent *e)
{
    if (w->ph == PH_MATERIALIZE) {
        if (e->age < 0) return 0;
        float k = e->age / 30.0f;
        return k > 1 ? 1 : k;
    }
    if (e->type == T_ENFORCER && e->age < 40) return 0.15f + 0.85f * e->age / 40.0f;
    if (e->type == T_TANK && e->age < 48) return 0.15f + 0.85f * e->age / 48.0f;
    return 1;
}

void render_world(vp_dvg *d, const world *w, double t)
{
    vp3d_scene_begin(SCENE, d, &VIEW);
    arena_into_scene(d, t);

    model_pose pose;
    memset(&pose, 0, sizeof pose);
    for (int i = 0; i < w->n; i++) {
        const ent *e = &w->e[i];
        if (e->type == T_NONE) continue;
        model_id m = model_of(e, w->wave);
        float k = appear(w, e);
        if (k <= 0) {
            if (w->transporter && w->ph == PH_MATERIALIZE && e->age > -40) {
                /* the transporter: a glitter where something is about to arrive */
                color(d, hsv((float)t * 3 + i * 0.1f, 0.5f, 1));
                float h = frand() * 14;
                vp3d_scene_line(SCENE, vp_v3(e->x + (frand() - 0.5f) * 8, h, e->z),
                                vp_v3(e->x + (frand() - 0.5f) * 8, h + 2, e->z));
            }
            continue;
        }
        pose.x = e->x;
        pose.z = e->z;
        pose.y = hover_of(e->type);
        pose.heading = e->heading;
        pose.walk = e->walk;
        pose.spin = e->spin;
        pose.color = color_of(e, w->wave, t);
        pose.flat_color = 0;
        pose.dim = 1;
        if (k < 1) {
            /* materializing: tall and thin, settling into shape (not too
             * tall: the stretched lines cost the beam right when the arena
             * is fullest) */
            if (w->transporter) {
                pose.sxz = pose.sy = k;
                pose.flat_color = (int)(t * 30) & 1;
            } else {
                pose.sxz = 0.15f + 0.85f * k;
                pose.sy = 1 + 2.5f * (1 - k);
                pose.flat_color = 1;
            }
            pose.dim = 0.4f + 0.6f * k;
        } else {
            pose.sxz = pose.sy = 1;
        }
        if (e->type == T_ELECTRODE) pose.heading = CAM.yaw;      /* flat, like the sprites */
        if (e->type == T_QUARK) {           /* the original's square grows and shrinks */
            float pulse = 0.65f + 0.35f * fabsf(sinf((float)t * 4 + e->z));
            pose.sxz *= pulse;
            pose.sy *= pulse;
        }
        if (e->type == T_SPHEROID) {
            float pulse = 0.75f + 0.3f * sinf((float)t * 9 + e->x);
            pose.sxz *= pulse;
            pose.sy *= pulse;
        }
        if (e->type == T_PROG) {
            /* the prog's trail: dim copies behind it, wireframe */
            model_pose ghost = pose;
            float sp = sqrtf(e->vx * e->vx + e->vz * e->vz);
            if (sp > 0.01f) {
                for (int g = 3; g >= 1; g--) {
                    ghost.x = e->x - e->vx / sp * 5.0f * g;
                    ghost.z = e->z - e->vz / sp * 5.0f * g;
                    ghost.color = rgb_scale(hsv(0.8f + 0.1f * g + (float)t, 0.8f, 1), 0.7f - 0.15f * g);
                    ghost.flat_color = 1;
                    model_draw(d, &VIEW, SCENE, m, &ghost);
                }
            }
        }
        if (e->type == T_CRUISE) {
            vp_color(d, 140, 60, 255);
            vp3d_scene_line(SCENE, vp_v3(e->x, pose.y, e->z), vp_v3(e->x - e->vx * 8, pose.y, e->z - e->vz * 8));
        }
        if (e->flash > 0) {             /* just hit: white */
            pose.flat_color = 1;
            pose.color = mkrgb(255, 255, 255);
        }
        if (is_human(e->type) && e->state == 1) {
            /* a human a brain has caught: flashing, shaking */
            pose.flat_color = 1;
            pose.color = ((int)(t * 15) & 1) ? mkrgb(255, 255, 255) : mkrgb(200, 60, 255);
            pose.y = 1.5f * sinf(e->spin);
        }
        model_draw(d, &VIEW, SCENE, m, &pose);
    }

    /* the player */
    int show = w->ph != PH_OVER && w->ph != PH_RESTART;
    float pu = 1;
    if (w->ph == PH_MATERIALIZE) {
        float start = (float)w->mat_len - 60;
        pu = (w->ph_time - start) / 60.0f;
        if (pu <= 0) show = 0;
        if (pu > 1) pu = 1;
    }
    if (w->ph == PH_DYING && w->ph_time > 110) show = 0;
    if (show) {
        pose.x = w->px;
        pose.z = w->pz;
        pose.y = 0;
        pose.heading = w->pheading;
        pose.walk = w->pwalk;
        pose.spin = 0;
        pose.sxz = pose.sy = 1;
        pose.flat_color = 0;
        pose.dim = 1;
        pose.color = model_color(MDL_PLAYER);
        if (pu < 1) {
            pose.sxz = 0.2f + 0.8f * pu;
            pose.sy = 1 + 2.5f * (1 - pu);
            pose.flat_color = 1;
            pose.color = hsv((float)t * 2, 0.5f, 1);
        }
        if (w->ph == PH_DYING) {
            /* flashing (10 x 8 frames), then the colors cycle */
            pose.flat_color = 1;
            pose.color = w->ph_time < 80 ? (((w->ph_time / 4) & 1) ? mkrgb(255, 255, 255) : mkrgb(255, 40, 40))
                                         : hsv(w->ph_time * 0.05f, 1, 1);
            if (w->ph_time >= 80) {
                float k = (w->ph_time - 80) / 30.0f;
                pose.sxz = 1 + k * 1.5f;
                pose.sy = 1 - k * 0.8f;
            }
        }
        model_draw(d, &VIEW, SCENE, MDL_PLAYER, &pose);
    }

    /* the player's shots: one line each in the wave's laser color, as the
     * original's laser (the beam's time goes to the robots) */
    color(d, ST()->laser);
    for (int i = 0; i < MAX_SHOTS; i++) {
        const shot *s = &w->shots[i];
        if (!s->live) continue;
        float l = sqrtf(s->dx * s->dx + s->dz * s->dz);
        float ux = s->dx / l, uz = s->dz / l;
        const float y = 7;
        vp3d_scene_line(SCENE, vp_v3(s->x - ux * 14, y, s->z - uz * 14), vp_v3(s->x + ux * 5, y, s->z + uz * 5));
    }

    /* shock waves on the floor */
    for (int i = 0; i < MAX_RINGS; i++) {
        const ring *r = &R[i];
        if (r->life <= 0) continue;
        float k = r->life / r->max;
        color(d, rgb_scale(k > 0.8f ? mkrgb(255, 255, 255) : r->c, k * 1.3f));
        const int n = r->r > 25 ? 14 : 10;
        vp_vec3 prev = vp_v3(r->x + r->r, r->y, r->z);
        for (int j = 1; j <= n; j++) {
            float a = 2 * PI * j / n;
            vp_vec3 p = vp_v3(r->x + cosf(a) * r->r, r->y, r->z + sinf(a) * r->r);
            vp3d_scene_line(SCENE, prev, p);
            prev = p;
        }
    }

    /* electrodes shrinking away */
    for (int i = 0; i < MAX_SHRINKS; i++) {
        if (SH[i].life <= 0) continue;
        float k = SH[i].life > 5 ? 0.75f : SH[i].life > 2 ? 0.45f : 0.2f;
        memset(&pose, 0, sizeof pose);
        pose.x = SH[i].x;
        pose.z = SH[i].z;
        pose.heading = CAM.yaw;
        pose.sxz = pose.sy = k;
        pose.flat_color = 1;
        pose.dim = 1;
        pose.color = SH[i].c;
        model_draw(d, &VIEW, SCENE, (model_id)(MDL_ELECTRODE0 + SH[i].shape), &pose);
    }

    /* skulls where humans died */
    for (int i = 0; i < MAX_SKULLS; i++) {
        if (K[i].life <= 0) continue;
        pose.x = K[i].x;
        pose.z = K[i].z;
        pose.y = 0;
        pose.heading = CAM.yaw;
        pose.sxz = pose.sy = 1;
        pose.flat_color = 1;
        pose.dim = 1;
        pose.color = rgb_scale(mkrgb(255, 255, 255), K[i].life / 40.0f);
        model_draw(d, &VIEW, SCENE, MDL_SKULL, &pose);
    }

    /* debris, sparks and embers */
    rgb last = mkrgb(1, 2, 3);
    for (int i = 0; i < nF; i++) {
        const frag *f = &F[i];
        float k = f->life / f->max;
        rgb c;
        if (f->kind == FR_DEBRIS) c = rgb_scale(f->c, k * 1.4f);
        else if (k > 0.75f) c = mkrgb(255, 255, 255);               /* white hot */
        else c = rgb_scale(f->c, k * 1.6f);
        if (c.r != last.r || c.g != last.g || c.b != last.b) { color(d, c); last = c; }
        if (f->kind == FR_DEBRIS) {
            float cr = cosf(f->rot), sr = sinf(f->rot);
            float ax = f->ax * cr - f->az * sr, az = f->ax * sr + f->az * cr;
            vp3d_scene_line(SCENE, vp_v3(f->p.x - ax, f->p.y, f->p.z - az), vp_v3(f->p.x + ax, f->p.y + 1, f->p.z + az));
        } else if (f->kind == FR_SPARK) {
            /* a streak along its flight */
            vp3d_scene_line(SCENE, f->p, vp_v3_sub(f->p, vp_v3_scale(f->v, 2.5f)));
        } else {
            vp3d_scene_line(SCENE, vp_v3(f->p.x, f->p.y - 0.7f, f->p.z), vp_v3(f->p.x, f->p.y + 0.7f, f->p.z));
        }
    }

    vp3d_scene_end(SCENE);

    /* score pop-ups (the rescue bonus, 1000 for a spheroid or quark): on top */
    vp_text_font(VP_FONT_ASTEROIDS);
    for (int i = 0; i < MAX_POPUPS; i++) {
        float sx, sy;
        if (P[i].life <= 0) continue;
        if (!vp3d_project(&VIEW, vp_v3(P[i].x, 16 + (60 - P[i].life) * 0.1f, P[i].z), &sx, &sy)) continue;
        char buf[16];
        snprintf(buf, sizeof buf, "%d", P[i].value);
        text(d, rgb_scale(hsv((float)t * 1.5f, 0.6f, 1), P[i].life / 20.0f), buf, sx, sy, 70, VP_TEXT_CENTER);
    }

    /* a banner (WAVE CLEARED, EXTRA MAN): zooms out of the middle with a
     * trail of echoes, Tempest 2000 style */
    if (banner_time > 0) {
        float u = 1 - banner_time / banner_max;
        float grow = u < 0.25f ? u / 0.25f : 1 + (u - 0.25f) * 0.25f;
        float fade = u > 0.8f ? (1 - u) / 0.2f : 1;
        /* the echoes cost big lines right when the arena is fullest (a wave
         * starting): two at FULL, one at LIGHT, none at OFF */
        for (int k = lvl(2); k >= 0; k--) {
            float h = 260 * grow * (1 - k * 0.12f);
            rgb c = k ? rgb_scale(hsv((float)t * 2 + k * 0.15f, 0.9f, 1), 0.5f * fade)
                      : rgb_scale(banner_color, fade);
            text(d, c, banner, W / 2.0f, 2350 - h / 2 + k * 40, h, VP_TEXT_CENTER);
        }
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
    (void)t;
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
        if (men > 7) men = 7;
        for (int i = 0; i < men; i++) {
            float ix = p == 0 ? x + 20 + i * 70.0f : x - 20 - i * 70.0f;
            draw_man_icon(d, ix, 3780, 40);
        }
    }
    snprintf(buf, sizeof buf, "HI %d", hiscore);
    text(d, mkrgb(200, 200, 255), buf, W / 2.0f, 3900, 90, VP_TEXT_CENTER);
    snprintf(buf, sizeof buf, "%d WAVE", ws[current]->wave);
    text(d, mkrgb(80, 160, 255), buf, W / 2.0f, 80, 90, VP_TEXT_CENTER);
}

void render_wave_tunnel(vp_dvg *d, float u, double t)
{
    /* nested rectangles rushing out from the middle, in cycling colors */
    for (int i = 0; i < 10; i++) {
        float k = u * 1.6f - i * 0.07f;
        if (k <= 0 || k > 1.2f) continue;
        float hw = k * 2300, hh = k * 1800;
        color(d, hsv((float)t * 0.5f + i * 0.1f, 1, 1));
        int x0 = (int)(W / 2 - hw), x1 = (int)(W / 2 + hw), y0 = (int)(W / 2 - hh), y1 = (int)(W / 2 + hh);
        vp_moveto(d, x0, y0);
        vp_lineto(d, x1, y0);
        vp_lineto(d, x1, y1);
        vp_lineto(d, x0, y1);
        vp_lineto(d, x0, y0);
    }
}
