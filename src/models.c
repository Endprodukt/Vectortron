/* models.c — the shapes: chunky robots and people in the Robotron X manner,
 * built from boxes and prisms.
 *
 * Every part is a closed solid, so that drawn through a scene each model
 * hides what is behind it (its own far limbs, the floor, the robots behind).
 * Legs and arms are separate meshes swung about the hip and the shoulder.
 * Parts marked DETAIL (visors, belts, hair...) are left out far from the
 * camera: the beam pays for every line.
 */
#include "models.h"

#include <math.h>
#include <string.h>

#define PI 3.14159265f

typedef enum { A_NONE, A_LEG_L, A_LEG_R, A_ARM_L, A_ARM_R, A_SPIN, A_SPIN_ALL, A_BOB } anim;

typedef struct {
    vp3d_mesh *mesh;
    anim       a;
    vp_vec3    pivot;
    rgb        color;
    int        own_color;       /* else the pose's color */
    int        detail;          /* left out far away */
    vp3d_mode  mode;
} part;

#define MAX_PARTS 14

typedef struct {
    part  parts[MAX_PARTS];
    int   n;
    float height;
    rgb   color;
} model;

static model M[MDL_COUNT];
static float lod_distance = 190.0f;

rgb rgb_scale(rgb c, float k)
{
    if (k < 0) k = 0;
    if (k > 1) k = 1;
    return mkrgb((int)(c.r * k), (int)(c.g * k), (int)(c.b * k));
}

void models_set_lod(float distance) { lod_distance = distance; }

/* ---- builders ------------------------------------------------------------ */

static vp3d_mesh *mesh_new(void)
{
    vp3d_mesh *m = NULL;
    vp3d_mesh_create(&m);
    return m;
}

static vp3d_mesh *mesh_done(vp3d_mesh *m, float crease)
{
    vp3d_mesh_finish(m);
    vp3d_mesh_set_crease(m, crease);
    return m;
}

/* An axis-aligned box centered on (cx, cy, cz), full sizes sx, sy, sz. */
static void add_box(vp3d_mesh *m, float cx, float cy, float cz, float sx, float sy, float sz)
{
    float x0 = cx - sx / 2, x1 = cx + sx / 2;
    float y0 = cy - sy / 2, y1 = cy + sy / 2;
    float z0 = cz - sz / 2, z1 = cz + sz / 2;
    int b = vp3d_mesh_vertex(m, x0, y0, z0);
    vp3d_mesh_vertex(m, x1, y0, z0);
    vp3d_mesh_vertex(m, x1, y1, z0);
    vp3d_mesh_vertex(m, x0, y1, z0);
    vp3d_mesh_vertex(m, x0, y0, z1);
    vp3d_mesh_vertex(m, x1, y0, z1);
    vp3d_mesh_vertex(m, x1, y1, z1);
    vp3d_mesh_vertex(m, x0, y1, z1);
    static const int f[6][4] = { {0,3,2,1}, {4,5,6,7}, {0,1,5,4}, {2,3,7,6}, {1,2,6,5}, {0,4,7,3} };
    for (int i = 0; i < 6; i++) {
        int q[4] = { b + f[i][0], b + f[i][1], b + f[i][2], b + f[i][3] };
        vp3d_mesh_face(m, q, 4);
    }
}

/* A box narrower at the top: a tapered torso (top sizes tx, tz). */
static void add_taper(vp3d_mesh *m, float cx, float cy, float cz, float sx, float sy, float sz, float tx, float tz)
{
    float y0 = cy - sy / 2, y1 = cy + sy / 2;
    int b = vp3d_mesh_vertex(m, cx - sx / 2, y0, cz - sz / 2);
    vp3d_mesh_vertex(m, cx + sx / 2, y0, cz - sz / 2);
    vp3d_mesh_vertex(m, cx + tx / 2, y1, cz - tz / 2);
    vp3d_mesh_vertex(m, cx - tx / 2, y1, cz - tz / 2);
    vp3d_mesh_vertex(m, cx - sx / 2, y0, cz + sz / 2);
    vp3d_mesh_vertex(m, cx + sx / 2, y0, cz + sz / 2);
    vp3d_mesh_vertex(m, cx + tx / 2, y1, cz + tz / 2);
    vp3d_mesh_vertex(m, cx - tx / 2, y1, cz + tz / 2);
    static const int f[6][4] = { {0,3,2,1}, {4,5,6,7}, {0,1,5,4}, {2,3,7,6}, {1,2,6,5}, {0,4,7,3} };
    for (int i = 0; i < 6; i++) {
        int q[4] = { b + f[i][0], b + f[i][1], b + f[i][2], b + f[i][3] };
        vp3d_mesh_face(m, q, 4);
    }
}

/* A prism or frustum standing on y: n sides, bottom radius r0 at y0, top
 * radius r1 at y1 (r1 = 0: a pyramid / cone). */
static void add_frustum(vp3d_mesh *m, int n, float cx, float cz, float y0, float r0, float y1, float r1,
                        float turn)
{
    int base = vp3d_mesh_vertex_count(m);
    for (int i = 0; i < n; i++) {
        float a = turn + 2 * PI * i / n;
        vp3d_mesh_vertex(m, cx + r0 * sinf(a), y0, cz + r0 * cosf(a));
    }
    int top = vp3d_mesh_vertex_count(m);
    if (r1 > 0) {
        for (int i = 0; i < n; i++) {
            float a = turn + 2 * PI * i / n;
            vp3d_mesh_vertex(m, cx + r1 * sinf(a), y1, cz + r1 * cosf(a));
        }
    } else {
        vp3d_mesh_vertex(m, cx, y1, cz);
    }
    int idx[16];
    for (int i = 0; i < n; i++) idx[i] = base + n - 1 - i;
    vp3d_mesh_face(m, idx, n);
    if (r1 > 0) {
        for (int i = 0; i < n; i++) idx[i] = top + i;
        vp3d_mesh_face(m, idx, n);
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            int q[4] = { base + i, base + j, top + j, top + i };
            vp3d_mesh_face(m, q, 4);
        }
    } else {
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            int t[3] = { base + i, base + j, top };
            vp3d_mesh_face(m, t, 3);
        }
    }
}

/* An octahedron (a diamond), radii rx, ry, rz. */
static void add_octa(vp3d_mesh *m, float cx, float cy, float cz, float rx, float ry, float rz)
{
    int b = vp3d_mesh_vertex(m, cx + rx, cy, cz);
    vp3d_mesh_vertex(m, cx, cy, cz + rz);
    vp3d_mesh_vertex(m, cx - rx, cy, cz);
    vp3d_mesh_vertex(m, cx, cy, cz - rz);
    vp3d_mesh_vertex(m, cx, cy + ry, cz);
    vp3d_mesh_vertex(m, cx, cy - ry, cz);
    for (int i = 0; i < 4; i++) {
        int j = (i + 1) % 4;
        int up[3] = { b + i, b + 4, b + j };
        int dn[3] = { b + j, b + 5, b + i };
        vp3d_mesh_face(m, up, 3);
        vp3d_mesh_face(m, dn, 3);
    }
}

static void add_line(vp3d_mesh *m, float x0, float y0, float z0, float x1, float y1, float z1)
{
    int a = vp3d_mesh_vertex(m, x0, y0, z0);
    int b = vp3d_mesh_vertex(m, x1, y1, z1);
    vp3d_mesh_line(m, a, b);
}

static void add_poly(vp3d_mesh *m, const float (*pts)[3], int n, int closed)
{
    int first = -1, prev = -1;
    for (int i = 0; i < n; i++) {
        int v = vp3d_mesh_vertex(m, pts[i][0], pts[i][1], pts[i][2]);
        if (prev >= 0) vp3d_mesh_line(m, prev, v);
        else first = v;
        prev = v;
    }
    if (closed && n > 2) vp3d_mesh_line(m, prev, first);
}

static void add_circle(vp3d_mesh *m, int n, float r, int axis)
{
    float pts[32][3];
    for (int i = 0; i < n; i++) {
        float a = 2 * PI * i / n, c = r * cosf(a), s = r * sinf(a);
        if (axis == 0)      { pts[i][0] = 0; pts[i][1] = c; pts[i][2] = s; }
        else if (axis == 1) { pts[i][0] = c; pts[i][1] = 0; pts[i][2] = s; }
        else                { pts[i][0] = c; pts[i][1] = s; pts[i][2] = 0; }
    }
    add_poly(m, (const float (*)[3])pts, n, 1);
}

static part *add_part(model *md, vp3d_mesh *mesh, anim a, vp_vec3 pivot, vp3d_mode mode)
{
    part *p = &md->parts[md->n++];
    memset(p, 0, sizeof *p);
    p->mesh = mesh;
    p->a = a;
    p->pivot = pivot;
    p->mode = mode;
    return p;
}

static part *solid(model *md, vp3d_mesh *mesh) { return add_part(md, mesh_done(mesh, 30), A_NONE, vp_v3(0, 0, 0), VP3D_SOLID); }

static part *colored(part *p, rgb c)
{
    p->color = c;
    p->own_color = 1;
    return p;
}

static part *detail(part *p)
{
    p->detail = 1;
    return p;
}

/* A leg: thigh-and-shin box and a foot, swinging about the hip. */
static part *leg(model *md, float x, float hip, float w, float d, float foot_len, int left)
{
    vp3d_mesh *m = mesh_new();
    add_box(m, x, hip / 2 + 0.4f, 0, w, hip - 0.8f, d);
    add_box(m, x, 0.4f, foot_len * 0.25f, w * 1.15f, 0.8f, foot_len);
    return add_part(md, mesh_done(m, 30), left ? A_LEG_L : A_LEG_R, vp_v3(0, hip, 0), VP3D_SOLID);
}

/* An arm with a hand, swinging about the shoulder. */
static part *arm(model *md, float x, float shoulder, float len, float w, float hand, int left)
{
    vp3d_mesh *m = mesh_new();
    add_box(m, x, shoulder - len / 2, 0, w, len, w);
    if (hand > 0) add_box(m, x, shoulder - len - hand / 2 + 0.3f, 0.2f, hand, hand, hand);
    return add_part(md, mesh_done(m, 30), left ? A_ARM_L : A_ARM_R, vp_v3(0, shoulder, 0), VP3D_SOLID);
}

/* ---- the models ---------------------------------------------------------- */

static void build_player(model *md)
{
    /* the original's hero: a big red-orange head, a slim white body and
     * legs, magenta arms (the Robotron player sprite) */
    rgb orange = mkrgb(255, 110, 20), red = mkrgb(255, 30, 20), magenta = mkrgb(255, 40, 255);
    leg(md, -0.8f, 5.6f, 1.2f, 1.3f, 2.2f, 1);
    leg(md, 0.8f, 5.6f, 1.2f, 1.3f, 2.2f, 0);
    vp3d_mesh *t = mesh_new();
    add_taper(t, 0, 8.0f, 0, 2.4f, 4.8f, 1.8f, 2.8f, 2.0f);   /* the white body */
    solid(md, t);
    vp3d_mesh *h = mesh_new();
    add_box(h, 0, 12.0f, 0, 3.6f, 2.8f, 3.4f);                 /* the big head */
    colored(solid(md, h), orange);
    vp3d_mesh *hair = mesh_new();
    add_box(hair, 0, 13.7f, -0.2f, 4.6f, 1.0f, 4.0f);          /* the red mop on top */
    add_box(hair, -2.1f, 12.4f, -0.2f, 0.7f, 2.0f, 3.6f);      /* and at the sides */
    add_box(hair, 2.1f, 12.4f, -0.2f, 0.7f, 2.0f, 3.6f);
    colored(solid(md, hair), red);
    vp3d_mesh *eyes = mesh_new();                              /* the dark eyes */
    add_box(eyes, -0.8f, 12.3f, 1.75f, 0.7f, 0.7f, 0.2f);
    add_box(eyes, 0.8f, 12.3f, 1.75f, 0.7f, 0.7f, 0.2f);
    detail(colored(solid(md, eyes), mkrgb(90, 0, 0)));
    colored(arm(md, -2.0f, 10.0f, 4.0f, 1.0f, 1.2f, 1), magenta);
    colored(arm(md, 2.0f, 10.0f, 4.0f, 1.0f, 1.2f, 0), magenta);
    md->height = 14.5f;
    md->color = mkrgb(255, 255, 255);
}

static void build_grunt(model *md)
{
    rgb yellow = mkrgb(255, 220, 0), green = mkrgb(40, 230, 120);
    colored(leg(md, -1.6f, 5.2f, 2.0f, 2.2f, 3.2f, 1), green);
    colored(leg(md, 1.6f, 5.2f, 2.0f, 2.2f, 3.2f, 0), green);
    vp3d_mesh *b = mesh_new();
    add_box(b, 0, 5.7f, 0, 4.8f, 1.2f, 3.0f);                  /* hips */
    add_taper(b, 0, 8.6f, 0, 5.4f, 4.6f, 3.6f, 7.0f, 4.2f);    /* torso */
    solid(md, b);
    vp3d_mesh *h = mesh_new();
    add_box(h, 0, 12.2f, 0, 3.6f, 2.6f, 3.2f);                 /* head */
    solid(md, h);
    vp3d_mesh *visor = mesh_new();
    add_box(visor, 0, 12.4f, 1.7f, 2.8f, 0.8f, 0.5f);
    add_box(visor, 0, 8.8f, 2.2f, 3.6f, 2.2f, 0.6f);           /* chest plate */
    detail(colored(solid(md, visor), yellow));
    vp3d_mesh *ant = mesh_new();                               /* antenna */
    add_line(ant, 0, 13.5f, 0, 0, 15.0f, -0.6f);
    detail(add_part(md, mesh_done(ant, 40), A_NONE, vp_v3(0, 0, 0), VP3D_WIREFRAME));
    colored(arm(md, -4.3f, 10.3f, 4.4f, 1.6f, 2.2f, 1), green);
    colored(arm(md, 4.3f, 10.3f, 4.4f, 1.6f, 2.2f, 0), green);
    md->height = 15;
    md->color = mkrgb(255, 40, 40);
}

static void build_hulk(model *md)
{
    rgb dark = mkrgb(20, 160, 40), red = mkrgb(255, 60, 40);
    colored(leg(md, -2.2f, 6.0f, 2.6f, 3.0f, 4.0f, 1), dark);
    colored(leg(md, 2.2f, 6.0f, 2.6f, 3.0f, 4.0f, 0), dark);
    vp3d_mesh *b = mesh_new();
    add_box(b, 0, 6.6f, 0, 6.2f, 1.6f, 4.4f);                  /* belt */
    add_taper(b, 0, 10.8f, 0, 7.0f, 6.8f, 5.0f, 10.4f, 6.0f);  /* the chest */
    solid(md, b);
    vp3d_mesh *pads = mesh_new();
    add_box(pads, -5.8f, 14.0f, 0, 3.0f, 1.8f, 4.0f);          /* shoulder pads */
    add_box(pads, 5.8f, 14.0f, 0, 3.0f, 1.8f, 4.0f);
    detail(colored(solid(md, pads), dark));
    vp3d_mesh *h = mesh_new();
    add_box(h, 0, 15.6f, 0.6f, 3.6f, 2.6f, 3.4f);              /* the small head */
    solid(md, h);
    vp3d_mesh *visor = mesh_new();
    add_box(visor, 0, 15.8f, 2.4f, 3.0f, 0.8f, 0.5f);
    detail(colored(solid(md, visor), red));
    colored(arm(md, -6.4f, 13.4f, 6.0f, 2.4f, 3.0f, 1), mkrgb(40, 255, 40));
    colored(arm(md, 6.4f, 13.4f, 6.0f, 2.4f, 3.0f, 0), mkrgb(40, 255, 40));
    md->height = 17.5f;
    md->color = mkrgb(40, 255, 40);
}

static void build_brain(model *md)
{
    rgb blue = mkrgb(80, 120, 255), pink = mkrgb(255, 120, 255);
    colored(leg(md, -0.9f, 5.6f, 1.3f, 1.4f, 2.2f, 1), blue);
    colored(leg(md, 0.9f, 5.6f, 1.3f, 1.4f, 2.2f, 0), blue);
    vp3d_mesh *b = mesh_new();
    add_taper(b, 0, 7.8f, 0, 3.0f, 4.4f, 2.0f, 3.8f, 2.4f);
    colored(solid(md, b), blue);
    vp3d_mesh *face = mesh_new();
    add_box(face, 0, 10.8f, 0.4f, 3.0f, 2.2f, 2.4f);
    colored(solid(md, face), mkrgb(255, 190, 150));
    vp3d_mesh *br = mesh_new();                                /* the big brain */
    add_frustum(br, 8, 0, -0.4f, 11.8f, 4.0f, 14.6f, 4.6f, PI / 8);
    add_frustum(br, 8, 0, -0.4f, 14.6f, 4.6f, 16.6f, 2.6f, PI / 8);
    solid(md, br);
    vp3d_mesh *folds = mesh_new();                             /* its folds */
    add_line(folds, 0, 16.7f, -2.6f, 0, 16.7f, 2.2f);
    add_line(folds, -2.2f, 16.0f, -3.0f, -2.6f, 14.8f, 3.0f);
    add_line(folds, 2.2f, 16.0f, -3.0f, 2.6f, 14.8f, 3.0f);
    detail(colored(add_part(md, mesh_done(folds, 40), A_NONE, vp_v3(0, 0, 0), VP3D_WIREFRAME), pink));
    colored(arm(md, -2.4f, 9.6f, 3.8f, 1.0f, 1.0f, 1), blue);
    colored(arm(md, 2.4f, 9.6f, 3.8f, 1.0f, 1.0f, 0), blue);
    md->height = 17;
    md->color = mkrgb(200, 60, 255);
}

static void build_prog(model *md)
{
    /* a human, scrambled: tall, thin, all one color */
    leg(md, -0.9f, 5.6f, 1.2f, 1.2f, 2.0f, 1);
    leg(md, 0.9f, 5.6f, 1.2f, 1.2f, 2.0f, 0);
    vp3d_mesh *m = mesh_new();
    add_octa(m, 0, 9.4f, 0, 2.4f, 4.0f, 1.6f);
    add_box(m, 0, 14.2f, 0, 2.2f, 2.2f, 2.2f);
    solid(md, m);
    md->height = 15;
    md->color = mkrgb(255, 80, 255);
}

static void build_mommy(model *md)
{
    rgb skin = mkrgb(255, 190, 150), hair = mkrgb(255, 230, 0);
    colored(leg(md, -0.8f, 3.4f, 1.0f, 1.0f, 1.8f, 1), skin);
    colored(leg(md, 0.8f, 3.4f, 1.0f, 1.0f, 1.8f, 0), skin);
    vp3d_mesh *d = mesh_new();
    add_frustum(d, 6, 0, 0, 3.0f, 3.4f, 7.4f, 1.6f, 0);        /* the dress */
    add_taper(d, 0, 8.6f, 0, 2.8f, 2.4f, 1.8f, 3.2f, 2.0f);    /* bodice */
    solid(md, d);
    vp3d_mesh *h = mesh_new();
    add_box(h, 0, 11.2f, 0, 2.4f, 2.6f, 2.4f);
    colored(solid(md, h), skin);
    vp3d_mesh *hr = mesh_new();
    add_box(hr, 0, 12.6f, -0.3f, 2.8f, 0.8f, 2.8f);
    add_box(hr, 0, 10.4f, -1.6f, 2.6f, 3.6f, 0.8f);            /* long hair behind */
    detail(colored(solid(md, hr), hair));
    colored(arm(md, -2.2f, 9.6f, 3.6f, 0.9f, 0.9f, 1), skin);
    colored(arm(md, 2.2f, 9.6f, 3.6f, 0.9f, 0.9f, 0), skin);
    md->height = 13.5f;
    md->color = mkrgb(255, 90, 160);
}

static void build_daddy(model *md)
{
    rgb skin = mkrgb(255, 190, 150), hat = mkrgb(255, 220, 140);
    leg(md, -1.1f, 5.4f, 1.5f, 1.6f, 2.4f, 1);
    leg(md, 1.1f, 5.4f, 1.5f, 1.6f, 2.4f, 0);
    vp3d_mesh *s = mesh_new();
    add_taper(s, 0, 8.0f, 0, 3.8f, 5.2f, 2.4f, 4.8f, 2.6f);    /* the suit */
    solid(md, s);
    vp3d_mesh *tie = mesh_new();
    add_box(tie, 0, 8.6f, 1.35f, 0.7f, 3.4f, 0.3f);
    detail(colored(solid(md, tie), mkrgb(255, 40, 40)));
    vp3d_mesh *h = mesh_new();
    add_box(h, 0, 11.8f, 0, 2.6f, 2.6f, 2.6f);
    colored(solid(md, h), skin);
    vp3d_mesh *ht = mesh_new();
    add_box(ht, 0, 13.4f, 0, 4.0f, 0.6f, 4.0f);                /* brim */
    add_box(ht, 0, 14.5f, 0, 2.6f, 1.6f, 2.6f);                /* crown */
    detail(colored(solid(md, ht), hat));
    arm(md, -3.0f, 10.2f, 4.4f, 1.2f, 1.1f, 1);
    arm(md, 3.0f, 10.2f, 4.4f, 1.2f, 1.1f, 0);
    md->height = 15.5f;
    md->color = mkrgb(70, 110, 255);
}

static void build_mikey(model *md)
{
    rgb skin = mkrgb(255, 190, 150), green = mkrgb(60, 255, 60);
    colored(leg(md, -0.7f, 3.4f, 1.0f, 1.0f, 1.6f, 1), skin);
    colored(leg(md, 0.7f, 3.4f, 1.0f, 1.0f, 1.6f, 0), skin);
    vp3d_mesh *b = mesh_new();
    add_box(b, 0, 3.8f, 0, 2.6f, 1.2f, 1.6f);                  /* shorts */
    add_box(b, 0, 5.6f, 0, 2.8f, 2.6f, 1.8f);                  /* shirt */
    solid(md, b);
    vp3d_mesh *h = mesh_new();
    add_box(h, 0, 8.0f, 0, 2.2f, 2.2f, 2.2f);
    colored(solid(md, h), skin);
    vp3d_mesh *cap = mesh_new();
    add_box(cap, 0, 9.4f, -0.1f, 2.4f, 0.7f, 2.4f);
    add_box(cap, 0, 9.2f, 1.6f, 2.0f, 0.3f, 1.2f);             /* the peak */
    detail(colored(solid(md, cap), green));
    colored(arm(md, -1.9f, 6.8f, 2.8f, 0.8f, 0.8f, 1), skin);
    colored(arm(md, 1.9f, 6.8f, 2.8f, 0.8f, 0.8f, 0), skin);
    md->height = 10;
    md->color = mkrgb(255, 60, 60);
}

static void build_spheroid(model *md)
{
    /* three rings turning round a glowing core */
    vp3d_mesh *m = mesh_new();
    add_circle(m, 14, 5.4f, 0);
    add_circle(m, 14, 5.4f, 1);
    add_circle(m, 14, 5.4f, 2);
    add_part(md, mesh_done(m, 40), A_SPIN_ALL, vp_v3(0, 0, 0), VP3D_WIREFRAME);
    vp3d_mesh *c = mesh_new();
    add_octa(c, 0, 0, 0, 2.0f, 2.0f, 2.0f);
    colored(add_part(md, mesh_done(c, 30), A_SPIN, vp_v3(0, 0, 0), VP3D_SOLID), mkrgb(255, 255, 120));
    md->height = 11;
    md->color = mkrgb(255, 80, 20);
}

static void build_enforcer(model *md)
{
    vp3d_mesh *m = mesh_new();
    add_frustum(m, 8, 0, 0, -1.2f, 2.0f, 0, 4.6f, PI / 8);     /* the saucer's underside */
    add_frustum(m, 8, 0, 0, 0, 4.6f, 1.0f, 3.6f, PI / 8);      /* its rim */
    add_part(md, mesh_done(m, 30), A_SPIN, vp_v3(0, 0, 0), VP3D_SOLID);
    vp3d_mesh *d = mesh_new();
    add_frustum(d, 6, 0, 0, 1.0f, 2.0f, 3.2f, 0.8f, 0);        /* the dome */
    colored(add_part(md, mesh_done(d, 30), A_SPIN, vp_v3(0, 0, 0), VP3D_SOLID), mkrgb(255, 255, 255));
    md->height = 8;
    md->color = mkrgb(40, 255, 120);
}

static void build_spark(model *md)
{
    vp3d_mesh *m = mesh_new();
    add_line(m, -2.4f, 0, 0, 2.4f, 0, 0);
    add_line(m, 0, -2.4f, 0, 0, 2.4f, 0);
    add_line(m, 0, 0, -2.4f, 0, 0, 2.4f);
    add_line(m, -1.6f, -1.6f, 0, 1.6f, 1.6f, 0);
    add_part(md, mesh_done(m, 40), A_SPIN_ALL, vp_v3(0, 0, 0), VP3D_WIREFRAME);
    md->height = 4;
    md->color = mkrgb(255, 255, 255);
}

static void build_quark(model *md)
{
    /* a spinning star of lines through a small cube */
    vp3d_mesh *m = mesh_new();
    const float r = 6.0f, d = 3.6f;
    add_line(m, -r, 0, 0, r, 0, 0);
    add_line(m, 0, -r, 0, 0, r, 0);
    add_line(m, 0, 0, -r, 0, 0, r);
    add_line(m, -d, -d, -d, d, d, d);
    add_line(m, -d, d, -d, d, -d, d);
    add_line(m, d, -d, -d, -d, d, d);
    add_line(m, d, d, -d, -d, -d, d);
    add_part(md, mesh_done(m, 40), A_SPIN_ALL, vp_v3(0, 0, 0), VP3D_WIREFRAME);
    vp3d_mesh *c = mesh_new();
    add_box(c, 0, 0, 0, 2.6f, 2.6f, 2.6f);
    colored(add_part(md, mesh_done(c, 30), A_SPIN_ALL, vp_v3(0, 0, 0), VP3D_SOLID), mkrgb(255, 255, 255));
    md->height = 12;
    md->color = mkrgb(120, 200, 255);
}

static void build_tank(model *md)
{
    rgb track = mkrgb(160, 160, 160);
    vp3d_mesh *tr = mesh_new();
    add_box(tr, -3.6f, 1.4f, 0, 2.2f, 2.8f, 10.0f);            /* the tracks */
    add_box(tr, 3.6f, 1.4f, 0, 2.2f, 2.8f, 10.0f);
    colored(solid(md, tr), track);
    vp3d_mesh *h = mesh_new();
    add_taper(h, 0, 3.4f, 0, 5.4f, 2.6f, 9.0f, 4.6f, 7.0f);    /* the hull */
    solid(md, h);
    vp3d_mesh *t = mesh_new();
    add_frustum(t, 6, 0, 0, 4.7f, 2.6f, 6.6f, 1.9f, 0);        /* the turret */
    add_box(t, 0, 5.8f, 4.2f, 0.9f, 0.9f, 5.0f);               /* the gun */
    colored(add_part(md, mesh_done(t, 30), A_SPIN, vp_v3(0, 0, 0), VP3D_SOLID), mkrgb(255, 220, 60));
    md->height = 8;
    md->color = mkrgb(255, 120, 40);
}

static void build_shell(model *md)
{
    vp3d_mesh *m = mesh_new();
    add_box(m, 0, 0, 0, 2.6f, 2.6f, 2.6f);
    add_part(md, mesh_done(m, 30), A_SPIN_ALL, vp_v3(0, 0, 0), VP3D_SOLID);
    md->height = 3;
    md->color = mkrgb(255, 255, 120);
}

static void build_cruise(model *md)
{
    vp3d_mesh *m = mesh_new();
    add_octa(m, 0, 0, 0, 1.8f, 1.8f, 1.8f);
    add_part(md, mesh_done(m, 30), A_SPIN_ALL, vp_v3(0, 0, 0), VP3D_SOLID);
    md->height = 4;
    md->color = mkrgb(255, 255, 255);
}

static void build_skull(model *md)
{
    /* skull and crossbones lying on the floor */
    vp3d_mesh *m = mesh_new();
    static const float skull[6][3] = {
        {-2.6f, 0.3f, 2.6f}, {2.6f, 0.3f, 2.6f}, {3.0f, 0.3f, 5.6f}, {1.6f, 0.3f, 8.0f},
        {-1.6f, 0.3f, 8.0f}, {-3.0f, 0.3f, 5.6f} };
    add_poly(m, skull, 6, 1);
    add_line(m, -1.6f, 0.3f, 5.6f, -0.6f, 0.3f, 5.6f);
    add_line(m, 0.6f, 0.3f, 5.6f, 1.6f, 0.3f, 5.6f);
    add_line(m, -4.4f, 0.3f, -1.0f, 4.4f, 0.3f, 3.4f);
    add_line(m, -4.4f, 0.3f, 3.4f, 4.4f, 0.3f, -1.0f);
    add_part(md, mesh_done(m, 40), A_NONE, vp_v3(0, 0, 0), VP3D_WIREFRAME);
    md->height = 4;
    md->color = mkrgb(255, 255, 255);
}

/* Electrodes: the original's shapes, one per wave in turn (RRP8.ASM:294-384):
 * star, snowflake, square, triangle, vertical bar, diamond, spike, spiral,
 * "2084" (a slab). */
static void build_electrode(model *md, int shape)
{
    vp3d_mesh *m = mesh_new();
    vp3d_mode mode = VP3D_SOLID;
    switch (shape) {
    case 0:                                                    /* star */
        add_box(m, 0, 3.5f, 0, 8.0f, 2.0f, 2.0f);
        add_box(m, 0, 3.5f, 0, 2.0f, 2.0f, 8.0f);
        add_box(m, 0, 3.5f, 0, 2.0f, 7.0f, 2.0f);
        break;
    case 1:                                                    /* snowflake */
        for (int i = 0; i < 3; i++) {
            float a = i * PI / 3;
            add_line(m, -4.5f * cosf(a), 4.0f, -4.5f * sinf(a), 4.5f * cosf(a), 4.0f, 4.5f * sinf(a));
        }
        add_line(m, 0, 0, 0, 0, 8.0f, 0);
        add_circle(m, 6, 2.0f, 1);
        mode = VP3D_WIREFRAME;
        break;
    case 2: add_box(m, 0, 3.5f, 0, 6.0f, 7.0f, 6.0f); break;                  /* square */
    case 3: add_frustum(m, 3, 0, 0, 0, 4.8f, 8.0f, 0, 0); break;              /* triangle */
    case 4: add_box(m, 0, 4.5f, 0, 2.6f, 9.0f, 2.6f); break;                  /* vertical bar */
    case 5: add_octa(m, 0, 4.5f, 0, 3.8f, 4.5f, 3.8f); break;                 /* diamond */
    case 6:                                                                    /* spike */
        add_frustum(m, 4, 0, 0, 0, 3.4f, 9.5f, 0, PI / 4);
        add_frustum(m, 4, 0, 0, 0, 4.6f, 3.0f, 0, 0);
        break;
    case 7: {                                                                  /* spiral */
        float pts[24][3];
        for (int i = 0; i < 24; i++) {
            float a = i * 0.7f, r = 1 + i * 0.15f;
            pts[i][0] = r * cosf(a); pts[i][1] = i * 0.35f; pts[i][2] = r * sinf(a);
        }
        add_poly(m, (const float (*)[3])pts, 24, 0);
        mode = VP3D_WIREFRAME;
        break;
    }
    default:                                                                   /* "2084" slab */
        add_box(m, 0, 3.5f, 0, 9.0f, 7.0f, 1.6f);
        add_box(m, 0, 0.5f, 0, 10.0f, 1.0f, 3.0f);
        break;
    }
    add_part(md, mesh_done(m, 30), A_NONE, vp_v3(0, 0, 0), mode);
    md->height = 8;
    md->color = mkrgb(255, 140, 0);
}

void models_init(void)
{
    memset(M, 0, sizeof M);
    build_player(&M[MDL_PLAYER]);
    build_grunt(&M[MDL_GRUNT]);
    build_hulk(&M[MDL_HULK]);
    build_brain(&M[MDL_BRAIN]);
    build_prog(&M[MDL_PROG]);
    build_mommy(&M[MDL_MOMMY]);
    build_daddy(&M[MDL_DADDY]);
    build_mikey(&M[MDL_MIKEY]);
    build_spheroid(&M[MDL_SPHEROID]);
    build_enforcer(&M[MDL_ENFORCER]);
    build_spark(&M[MDL_SPARK]);
    build_quark(&M[MDL_QUARK]);
    build_tank(&M[MDL_TANK]);
    build_shell(&M[MDL_SHELL]);
    build_cruise(&M[MDL_CRUISE]);
    build_skull(&M[MDL_SKULL]);
    for (int i = 0; i < ELECTRODE_SHAPES; i++) build_electrode(&M[MDL_ELECTRODE0 + i], i);
}

void models_free(void)
{
    for (int i = 0; i < MDL_COUNT; i++)
        for (int j = 0; j < M[i].n; j++) vp3d_mesh_free(M[i].parts[j].mesh);
    memset(M, 0, sizeof M);
}

rgb model_color(model_id id) { return M[id].color; }
float model_height(model_id id) { return M[id].height; }

/* ---- drawing ------------------------------------------------------------- */

static vp_xf about(vp_vec3 pivot, vp_xf r)
{
    return vp_xf_mul2(vp_xf_translate(pivot.x, pivot.y, pivot.z),
                      vp_xf_mul2(r, vp_xf_translate(-pivot.x, -pivot.y, -pivot.z)));
}

void model_draw(vp_dvg *d, const vp3d_view *v, vp3d_scene *scene, model_id id, const model_pose *p)
{
    const model *md = &M[id];
    vp_xf base = vp_xf_mul2(vp_xf_translate(p->x, p->y, p->z),
                            vp_xf_mul2(vp_xf_scale(p->sxz, p->sy, p->sxz), vp_xf_rot_y(p->heading)));
    float swing = 0.6f * sinf(p->walk);
    vp_vec3 eye = vp3d_eye(v);
    float dist = vp_v3_length(vp_v3_sub(eye, vp_v3(p->x, p->y, p->z)));
    int far = dist > lod_distance;
    rgb last = mkrgb(1, 2, 3);
    int have = 0;
    for (int i = 0; i < md->n; i++) {
        const part *pt = &md->parts[i];
        if (pt->detail && far) continue;
        vp_xf local;
        switch (pt->a) {
        case A_LEG_L:    local = about(pt->pivot, vp_xf_rot_x(swing)); break;
        case A_LEG_R:    local = about(pt->pivot, vp_xf_rot_x(-swing)); break;
        case A_ARM_L:    local = about(pt->pivot, vp_xf_rot_x(-swing * 0.8f)); break;
        case A_ARM_R:    local = about(pt->pivot, vp_xf_rot_x(swing * 0.8f)); break;
        case A_SPIN:     local = vp_xf_rot_y(p->spin); break;
        case A_SPIN_ALL: local = vp_xf_mul2(vp_xf_rot_y(p->spin), vp_xf_rot_x(p->spin * 0.7f)); break;
        default:         local = vp_xf_identity(); break;
        }
        vp_xf place = vp_xf_mul2(base, local);
        rgb c = (pt->own_color && !p->flat_color) ? pt->color : p->color;
        if (p->dim < 1 && p->dim > 0) c = rgb_scale(c, p->dim);
        if (!have || c.r != last.r || c.g != last.g || c.b != last.b) {
            vp_color(d, c.r, c.g, c.b);
            last = c;
            have = 1;
        }
        if (scene) vp3d_scene_mesh(scene, pt->mesh, &place, pt->mode);
        else vp3d_draw_mesh(d, v, pt->mesh, &place, pt->mode);
    }
}
