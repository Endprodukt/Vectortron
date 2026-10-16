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
    /* the original: a wide green robot on red legs, an antenna on its head */
    rgb green = mkrgb(40, 255, 40), dark = mkrgb(20, 170, 40), red = mkrgb(255, 50, 30);
    colored(leg(md, -2.2f, 6.0f, 2.6f, 3.0f, 4.0f, 1), red);
    colored(leg(md, 2.2f, 6.0f, 2.6f, 3.0f, 4.0f, 0), red);
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
    add_box(h, 0, 19.4f, 0.6f, 1.0f, 1.0f, 1.0f);              /* the antenna's knob */
    solid(md, h);
    vp3d_mesh *ant = mesh_new();
    add_line(ant, 0, 16.9f, 0.6f, 0, 18.9f, 0.6f);
    add_part(md, mesh_done(ant, 40), A_NONE, vp_v3(0, 0, 0), VP3D_WIREFRAME);
    vp3d_mesh *visor = mesh_new();
    add_box(visor, 0, 15.8f, 2.4f, 3.0f, 0.8f, 0.5f);
    detail(colored(solid(md, visor), red));
    colored(arm(md, -6.4f, 13.4f, 6.0f, 2.4f, 3.0f, 1), green);
    colored(arm(md, 6.4f, 13.4f, 6.0f, 2.4f, 3.0f, 0), green);
    md->height = 20;
    md->color = green;
}

static void build_brain(model *md)
{
    rgb blue = mkrgb(80, 120, 255), pink = mkrgb(255, 120, 255);
    colored(leg(md, -0.9f, 5.6f, 1.3f, 1.4f, 2.2f, 1), mkrgb(40, 220, 80));
    colored(leg(md, 0.9f, 5.6f, 1.3f, 1.4f, 2.2f, 0), mkrgb(40, 220, 80));
    vp3d_mesh *b = mesh_new();
    add_taper(b, 0, 7.8f, 0, 3.0f, 4.4f, 2.0f, 3.8f, 2.4f);
    colored(solid(md, b), blue);
    vp3d_mesh *face = mesh_new();
    add_box(face, 0, 10.8f, 0.4f, 3.0f, 2.2f, 2.4f);
    colored(solid(md, face), mkrgb(60, 140, 255));             /* the blue face */
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
    /* the original: a grey-white dome with a red band, blue underneath */
    vp3d_mesh *m = mesh_new();
    add_frustum(m, 8, 0, 0, -1.2f, 2.0f, 0, 4.6f, PI / 8);     /* the underside */
    colored(add_part(md, mesh_done(m, 30), A_SPIN, vp_v3(0, 0, 0), VP3D_SOLID), mkrgb(70, 110, 255));
    vp3d_mesh *r = mesh_new();
    add_frustum(r, 8, 0, 0, 0, 4.6f, 1.0f, 3.6f, PI / 8);      /* the red band */
    colored(add_part(md, mesh_done(r, 30), A_SPIN, vp_v3(0, 0, 0), VP3D_SOLID), mkrgb(255, 40, 40));
    vp3d_mesh *d = mesh_new();
    add_frustum(d, 6, 0, 0, 1.0f, 3.0f, 3.6f, 1.2f, 0);        /* the dome */
    add_part(md, mesh_done(d, 30), A_SPIN, vp_v3(0, 0, 0), VP3D_SOLID);
    md->height = 8;
    md->color = mkrgb(225, 225, 235);
}

static void build_spark(model *md)
{
    /* the original's red crosses, spinning */
    vp3d_mesh *m = mesh_new();
    add_line(m, -2.6f, 0, 0, 2.6f, 0, 0);
    add_line(m, 0, -2.6f, 0, 0, 2.6f, 0);
    add_line(m, 0, 0, -2.6f, 0, 0, 2.6f);
    add_part(md, mesh_done(m, 40), A_SPIN_ALL, vp_v3(0, 0, 0), VP3D_WIREFRAME);
    md->height = 4;
    md->color = mkrgb(255, 40, 40);
}

static void build_quark(model *md)
{
    /* the original's "square": a blue square with a green cross in it,
     * here a turning blue cube frame with a green X on every side */
    vp3d_mesh *m = mesh_new();
    add_box(m, 0, 0, 0, 9.0f, 9.0f, 9.0f);
    add_part(md, mesh_done(m, 30), A_SPIN_ALL, vp_v3(0, 0, 0), VP3D_WIREFRAME);
    vp3d_mesh *x = mesh_new();
    const float h = 4.5f, k = 3.2f;
    for (int f = 0; f < 4; f++) {
        float c = cosf(f * PI / 2), sn = sinf(f * PI / 2);
        /* the face at distance h along (sn, 0, c), an X across it */
        add_line(x, sn * h - c * k, -k, c * h + sn * k, sn * h + c * k, k, c * h - sn * k);
        add_line(x, sn * h - c * k, k, c * h + sn * k, sn * h + c * k, -k, c * h - sn * k);
    }
    add_line(x, -k, h, -k, k, h, k);
    add_line(x, -k, h, k, k, h, -k);
    add_line(x, -k, -h, -k, k, -h, k);
    add_line(x, -k, -h, k, k, -h, -k);
    colored(add_part(md, mesh_done(x, 40), A_SPIN_ALL, vp_v3(0, 0, 0), VP3D_WIREFRAME), mkrgb(40, 255, 40));
    md->height = 12;
    md->color = mkrgb(60, 80, 255);
}

static void build_tank(model *md)
{
    /* the original: a green box on tracks, with an orange face */
    vp3d_mesh *tr = mesh_new();
    add_box(tr, -3.4f, 1.3f, 0, 2.0f, 2.6f, 9.0f);             /* the tracks */
    add_box(tr, 3.4f, 1.3f, 0, 2.0f, 2.6f, 9.0f);
    colored(solid(md, tr), mkrgb(20, 150, 40));
    vp3d_mesh *h = mesh_new();
    add_box(h, 0, 6.2f, 0, 7.6f, 7.4f, 7.0f);                  /* the box */
    add_box(h, 0, 10.4f, -0.5f, 2.4f, 1.0f, 2.4f);             /* its hatch */
    solid(md, h);
    vp3d_mesh *face = mesh_new();
    add_box(face, -1.6f, 7.6f, 3.6f, 1.6f, 1.6f, 0.4f);        /* the eyes */
    add_box(face, 1.6f, 7.6f, 3.6f, 1.6f, 1.6f, 0.4f);
    add_box(face, 0, 4.6f, 3.6f, 3.6f, 0.8f, 0.4f);            /* the mouth */
    colored(solid(md, face), mkrgb(255, 130, 20));
    vp3d_mesh *gun = mesh_new();
    add_box(gun, 0, 9.0f, 4.4f, 0.9f, 0.9f, 3.2f);
    detail(colored(solid(md, gun), mkrgb(255, 220, 60)));
    md->height = 11;
    md->color = mkrgb(40, 255, 40);
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

/* A flat shape standing upright, `depth` thick: its outline in the xy plane
 * (star-shaped round its centroid), capped
 * front and back with triangles from the centroid so every face is convex. */
static void add_slab(vp3d_mesh *m, const float (*pts)[2], int n, float depth)
{
    float cx = 0, cy = 0;
    for (int i = 0; i < n; i++) { cx += pts[i][0]; cy += pts[i][1]; }
    cx /= n;
    cy /= n;
    float z0 = -depth / 2, z1 = depth / 2;
    int f = vp3d_mesh_vertex_count(m);
    for (int i = 0; i < n; i++) vp3d_mesh_vertex(m, pts[i][0], pts[i][1], z0);
    int bk = vp3d_mesh_vertex_count(m);
    for (int i = 0; i < n; i++) vp3d_mesh_vertex(m, pts[i][0], pts[i][1], z1);
    int fc = vp3d_mesh_vertex(m, cx, cy, z0);
    int bc = vp3d_mesh_vertex(m, cx, cy, z1);
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        int t1[3] = { fc, f + j, f + i };
        int t2[3] = { bc, bk + i, bk + j };
        int q[4] = { f + i, f + j, bk + j, bk + i };
        vp3d_mesh_face(m, t1, 3);
        vp3d_mesh_face(m, t2, 3);
        vp3d_mesh_face(m, q, 4);
    }
}

/* The digits of "2084" as segments on a 2 x 2 cell (as font3d). */
static void add_digit(vp3d_mesh *m, char c, float x0, float y0, float w, float h, float z, int mirror)
{
    static const float PX[9] = { 0, 0.5f, 1, 0, 0.5f, 1, 0, 0.5f, 1 };
    static const float PY[9] = { 1, 1, 1, 0.5f, 0.5f, 0.5f, 0, 0, 0 };
    const char *g = c == '2' ? "0225533668" : c == '0' ? "02288660" : c == '8' ? "0228866035" : "033528";
    for (int k = 0; g[k] && g[k + 1]; k += 2) {
        int a = g[k] - '0', b = g[k + 1] - '0';
        float xa = x0 + PX[a] * w, xb = x0 + PX[b] * w;
        if (mirror) { xa = -xa; xb = -xb; }
        add_line(m, xa, y0 + PY[a] * h, z, xb, y0 + PY[b] * h, z);
    }
}

/* Electrodes: the original's nine shapes, one per wave in turn (GTWCOL,
 * RRP8.ASM:294-384): star, snowflake, square, triangle, vertical bar,
 * diamond, spike, spiral, "2084". Flat like the sprites, standing up; the
 * game turns them to face the camera. */
static void build_electrode(model *md, int shape)
{
    vp3d_mesh *m = mesh_new();
    vp3d_mesh *lines = NULL;
    const float D = 1.6f, cy = 4.8f;
    float pts[20][2];
    int n = 0;
    switch (shape) {
    case 0:                                                    /* a sun: an 8-pointed star */
        for (int i = 0; i < 16; i++) {
            float a = i * PI / 8, r = (i & 1) ? 2.4f : 4.8f;
            pts[n][0] = r * cosf(a); pts[n][1] = cy + r * sinf(a); n++;
        }
        add_slab(m, (const float (*)[2])pts, n, D);
        break;
    case 1:                                                    /* snowflake: a checkered diamond */
        pts[0][0] = 0; pts[0][1] = cy - 4.6f; pts[1][0] = 4.6f; pts[1][1] = cy;
        pts[2][0] = 0; pts[2][1] = cy + 4.6f; pts[3][0] = -4.6f; pts[3][1] = cy;
        add_slab(m, (const float (*)[2])pts, 4, D);
        lines = mesh_new();
        for (int k = -1; k <= 1; k++) {
            for (int sd = -1; sd <= 1; sd += 2) {
                float o = k * 1.6f, z = sd * (D / 2 + 0.05f);
                add_line(lines, -2.3f + o, cy - 2.3f - o, z, 2.3f + o, cy + 2.3f - o, z);
                add_line(lines, -2.3f + o, cy + 2.3f + o, z, 2.3f + o, cy - 2.3f + o, z);
            }
        }
        break;
    case 2: add_box(m, 0, cy, 0, 7.6f, 7.6f, D); break;                       /* square */
    case 3:                                                                    /* triangle */
        pts[0][0] = -4.0f; pts[0][1] = 0.6f; pts[1][0] = 4.0f; pts[1][1] = 0.6f;
        pts[2][0] = 4.0f; pts[2][1] = 8.6f;
        add_slab(m, (const float (*)[2])pts, 3, D);
        break;
    case 4: add_box(m, 0, cy, 0, 2.6f, 9.4f, D); break;                       /* vertical bar */
    case 5:                                                                    /* diamond */
        pts[0][0] = 0; pts[0][1] = cy - 4.6f; pts[1][0] = 4.0f; pts[1][1] = cy;
        pts[2][0] = 0; pts[2][1] = cy + 4.6f; pts[3][0] = -4.0f; pts[3][1] = cy;
        add_slab(m, (const float (*)[2])pts, 4, D);
        break;
    case 6:                                                                    /* spike: a comb */
        add_box(m, 0, 3.6f, 0, 10.0f, 2.8f, D);
        lines = mesh_new();
        for (int k = 0; k < 5; k++) {
            float x = -4.0f + k * 2.0f;
            add_line(lines, x, 5.0f, 0, x, 8.2f, 0);
        }
        break;
    case 7: {                                                                  /* a square spiral */
        float x = -4.2f, y = cy - 4.2f, len = 8.4f;
        float pp[16][3];
        int k = 0, dir = 0;
        pp[k][0] = x; pp[k][1] = y; pp[k][2] = 0; k++;
        while (k < 14 && len > 0.8f) {
            static const float DX[4] = { 1, 0, -1, 0 }, DY[4] = { 0, 1, 0, -1 };
            x += DX[dir] * len;
            y += DY[dir] * len;
            pp[k][0] = x; pp[k][1] = y; pp[k][2] = 0; k++;
            if (dir & 1) len -= 1.8f;
            dir = (dir + 1) & 3;
        }
        float front[16][3], back[16][3];
        for (int i = 0; i < k; i++) {
            front[i][0] = back[i][0] = pp[i][0];
            front[i][1] = back[i][1] = pp[i][1];
            front[i][2] = -0.6f;
            back[i][2] = 0.6f;
        }
        add_poly(m, (const float (*)[3])front, k, 0);
        add_poly(m, (const float (*)[3])back, k, 0);
        add_part(md, mesh_done(m, 30), A_NONE, vp_v3(0, 0, 0), VP3D_WIREFRAME);
        md->height = 10;
        md->color = mkrgb(40, 255, 40);
        return;
    }
    default:                                                                   /* "2084" */
        add_box(m, 0, 3.4f, 0, 11.0f, 5.0f, D);
        lines = mesh_new();
        for (int sd = -1; sd <= 1; sd += 2) {
            float z = sd * (D / 2 + 0.05f);
            /* the back reads right from behind too */
            for (int k = 0; k < 4; k++) add_digit(lines, "2084"[k], -4.8f + k * 2.5f, 1.6f, 1.6f, 3.6f, z, sd > 0);
        }
        break;
    }
    add_part(md, mesh_done(m, 30), A_NONE, vp_v3(0, 0, 0), VP3D_SOLID);
    if (lines) add_part(md, mesh_done(lines, 40), A_NONE, vp_v3(0, 0, 0), VP3D_WIREFRAME);
    md->height = 10;
    md->color = mkrgb(40, 255, 40);
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
