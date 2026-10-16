/* render.c — camera, arena, effects, HUD.
 *
 * The arena is drawn through one vp3d scene per frame: the floor, the walls,
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
static int FLOOR;

void render_set_floor(int floor) { FLOOR = floor; }
static vp3d_scene *SCENE;

/* the camera, smoothed: what it looks at, from which side, how far */
static struct {
    vp_vec3 target;
    float   yaw, pitch, dist;
    int     ready;
    float   clock;
    float   pvx, pvz, lastx, lastz;
} CAM;

/* ---- effects ------------------------------------------------------------- */

#define MAX_FRAGS  600
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
    VIEW.near_z = 4.0f;
    VIEW.far_z = 2000.0f;
    if (!SCENE) vp3d_scene_create(&SCENE);
    CAM.ready = 0;
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
        /* the walls stop debris too */
        if (f->p.x < ARENA_X0 || f->p.x > ARENA_X1) { f->v.x = -f->v.x * 0.5f; f->p.x = f->p.x < ARENA_X0 ? ARENA_X0 : ARENA_X1; }
        if (f->p.z < ARENA_Z0 || f->p.z > ARENA_Z1) { f->v.z = -f->v.z * 0.5f; f->p.z = f->p.z < ARENA_Z0 ? ARENA_Z0 : ARENA_Z1; }
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
            yaw += k * 1.3f;
            pitch += k * 0.45f;
            dist *= 1 + k * 0.9f;
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
    float cp = cosf(CAM.pitch), sp = sinf(CAM.pitch);
    vp_vec3 eye = vp_v3(CAM.target.x - sinf(CAM.yaw) * cp * CAM.dist,
                        CAM.target.y + sp * CAM.dist,
                        CAM.target.z - cosf(CAM.yaw) * cp * CAM.dist);
    vp3d_look_at(&VIEW, eye, CAM.target, vp_v3(0, 1, 0));
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

#define TILES_X 10
#define TILES_Z 8
#define WALL_H  7.0f

static void arena_into_scene(vp_dvg *d, double t)
{
    const float x0 = ARENA_X0 - 4, x1 = ARENA_X1 + 4, z0 = ARENA_Z0 - 4, z1 = ARENA_Z1 + 4;

    /* the floor: tiles with a gap between them, as in Robotron X; walked in
     * a snake so the beam's jumps stay short */
    const float tw = (x1 - x0) / TILES_X, td = (z1 - z0) / TILES_Z, gap = 1.6f;
    vp_color(d, 150, 130, 40);
    if (FLOOR == 1) {
        for (int i = 1; i < TILES_X; i++) {
            float x = x0 + i * tw;
            if (i & 1) vp3d_scene_line(SCENE, vp_v3(x, 0, z0), vp_v3(x, 0, z1));
            else       vp3d_scene_line(SCENE, vp_v3(x, 0, z1), vp_v3(x, 0, z0));
        }
        for (int j = 1; j < TILES_Z; j++) {
            float z = z0 + j * td;
            if (j & 1) vp3d_scene_line(SCENE, vp_v3(x1, 0, z), vp_v3(x0, 0, z));
            else       vp3d_scene_line(SCENE, vp_v3(x0, 0, z), vp_v3(x1, 0, z));
        }
    }
    for (int j = 0; j < TILES_Z && FLOOR == 0; j++) {
        for (int k = 0; k < TILES_X; k++) {
            int i = (j & 1) ? TILES_X - 1 - k : k;
            float ax = x0 + i * tw + gap, bx = x0 + (i + 1) * tw - gap;
            float az = z0 + j * td + gap, bz = z0 + (j + 1) * td - gap;
            vp3d_scene_line(SCENE, vp_v3(ax, 0, az), vp_v3(bx, 0, az));
            vp3d_scene_line(SCENE, vp_v3(bx, 0, az), vp_v3(bx, 0, bz));
            vp3d_scene_line(SCENE, vp_v3(bx, 0, bz), vp_v3(ax, 0, bz));
            vp3d_scene_line(SCENE, vp_v3(ax, 0, bz), vp_v3(ax, 0, az));
        }
    }

    /* the border: a low magenta wall that flashes on events; solid, so the
     * near wall hides the feet of whoever stands behind it */
    rgb c = flash_time > 0 && ((int)flash_time / 3) % 2 ? flash_color
          : hsv(0.83f + 0.03f * sinf((float)t * 0.7f), 0.85f, 1.0f);
    color(d, c);
    vp_vec3 lo[4] = { vp_v3(x0, 0, z0), vp_v3(x1, 0, z0), vp_v3(x1, 0, z1), vp_v3(x0, 0, z1) };
    for (int i = 0; i < 4; i++) {
        vp_vec3 a = lo[i], b = lo[(i + 1) % 4];
        vp_vec3 q[4] = { a, b, vp_v3(b.x, WALL_H, b.z), vp_v3(a.x, WALL_H, a.z) };
        vp3d_scene_polygon(SCENE, q, 4, VP3D_ALL_EDGES);
    }
}

/* The score, the men left and the wave, standing on the far wall. The row
 * keeps to the part of the wall the camera is looking at. */
void render_wall_hud(vp_dvg *d, const world *w, int player, int players, double t)
{
    char buf[32];
    const float zb = ARENA_Z1 + 8, base = 10, h = 14;
    const float row = 250;                      /* the whole row's width */
    vp_vec3 right = vp_v3(1, 0, 0), up = vp_v3(0, 1, 0);
    /* the point of the far wall in the middle of the screen */
    float mid = CAM.target.x, best = 1e9f;
    for (float x = ARENA_X0 - 60; x <= ARENA_X1 + 60; x += 4) {
        float sx, sy;
        if (vp3d_project(&VIEW, vp_v3(x, base, zb), &sx, &sy) && fabsf(sx - W / 2.0f) < best) {
            best = fabsf(sx - W / 2.0f);
            mid = x;
        }
    }
    float x0 = mid - row / 2;
    if (x0 < ARENA_X0 - 30) x0 = ARENA_X0 - 30;
    if (x0 > ARENA_X1 + 30 - row) x0 = ARENA_X1 + 30 - row;

    vp_color(d, 255, 40, 60);
    snprintf(buf, sizeof buf, players > 1 ? "SCORE%d" : "SCORE", player + 1);
    font3d_text(d, &VIEW, buf, vp_v3(x0, base, zb), right, up, h, FONT3D_LEFT);
    float sx = x0 + h * 0.8f * ((float)strlen(buf) + 0.6f);
    vp_color(d, 60, 120, 255);
    snprintf(buf, sizeof buf, "%d", w->score);
    font3d_text(d, &VIEW, buf, vp_v3(sx, base, zb), right, up, h, FONT3D_LEFT);

    /* the men left: little figures standing on the wall */
    model_pose p;
    memset(&p, 0, sizeof p);
    p.sxz = p.sy = 1;
    p.dim = 1;
    p.color = model_color(MDL_PLAYER);
    int men = w->lives - 1;
    if (men > 7) men = 7;           /* the original shows 7 at most */
    for (int i = 0; i < men; i++) {
        p.x = x0 + row - 6 - i * 11.0f;
        p.z = zb;
        p.y = base;
        p.heading = PI + 0.4f * sinf((float)t * 2 + i);
        model_draw(d, &VIEW, NULL, MDL_PLAYER, &p);
    }
    vp_color(d, 80, 220, 120);
    snprintf(buf, sizeof buf, "WAVE %d", w->wave);
    font3d_text(d, &VIEW, buf, vp_v3(x0 + row, base + 20, zb), right, up, 8, FONT3D_RIGHT);
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

static rgb color_of(const ent *e, int wave, double t)
{
    switch (e->type) {
    case T_SPHEROID: return hsv((float)t * 0.8f, 0.8f, 1);
    case T_QUARK:    return hsv((float)t * 1.3f + e->x * 0.01f, 0.7f, 1);
    case T_SPARK:    return ((int)(t * 20) & 1) ? mkrgb(255, 255, 255) : mkrgb(255, 80, 255);
    case T_CRUISE:   return hsv((float)t * 2, 0.6f, 1);
    case T_ELECTRODE: {
        /* a color of its own every wave, as the original's post colors */
        static const rgb ec[10] = { {255,140,0}, {0,200,255}, {255,60,255}, {255,255,40}, {60,255,60},
                                    {255,60,60}, {120,120,255}, {255,255,255}, {0,255,200}, {255,160,200} };
        return ec[((wave > 0 ? wave : 1) - 1) % 10];
    }
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
            /* materializing: tall and thin, settling into shape */
            if (w->transporter) {
                pose.sxz = pose.sy = k;
                pose.flat_color = (int)(t * 30) & 1;
            } else {
                pose.sxz = 0.15f + 0.85f * k;
                pose.sy = 1 + 5 * (1 - k);
                pose.flat_color = 1;
            }
            pose.dim = 0.4f + 0.6f * k;
        } else {
            pose.sxz = pose.sy = 1;
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
            pose.sy = 1 + 4 * (1 - pu);
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

    /* the player's shots: bright streaks */
    vp_color(d, 255, 255, 255);
    for (int i = 0; i < MAX_SHOTS; i++) {
        const shot *s = &w->shots[i];
        if (!s->live) continue;
        vp3d_scene_line(SCENE, vp_v3(s->x - s->dx * 5, 7, s->z - s->dz * 5),
                        vp_v3(s->x + s->dx * 3, 7, s->z + s->dz * 3));
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

    /* debris */
    rgb last = mkrgb(1, 2, 3);
    for (int i = 0; i < nF; i++) {
        const frag *f = &F[i];
        rgb c = rgb_scale(f->c, f->life / f->max * 1.4f);
        if (c.r != last.r || c.g != last.g || c.b != last.b) { color(d, c); last = c; }
        float cr = cosf(f->rot), sr = sinf(f->rot);
        float ax = f->ax * cr - f->az * sr, az = f->ax * sr + f->az * cr;
        vp3d_scene_line(SCENE, vp_v3(f->p.x - ax, f->p.y, f->p.z - az), vp_v3(f->p.x + ax, f->p.y + 1, f->p.z + az));
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
