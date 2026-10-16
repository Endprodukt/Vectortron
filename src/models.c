/* models.c — the shapes, built from boxes, prisms and lines.
 *
 * Kept small on purpose: a wave can hold well over a hundred things, and a
 * vector monitor pays for every line. Static parts of a model share one mesh;
 * legs and arms are lone lines (or thin boxes) swung about the hip and the
 * shoulder for the walk.
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
    vp3d_mode  mode;
} part;

#define MAX_PARTS 8

typedef struct {
    part  parts[MAX_PARTS];
    int   n;
    float height;
    rgb   color;
} model;

static model M[MDL_COUNT];

rgb rgb_scale(rgb c, float k)
{
    if (k < 0) k = 0;
    if (k > 1) k = 1;
    return mkrgb((int)(c.r * k), (int)(c.g * k), (int)(c.b * k));
}

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

/* A polyline of n points (closed when `closed`). */
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
    p->mesh = mesh;
    p->a = a;
    p->pivot = pivot;
    p->mode = mode;
    p->own_color = 0;
    return p;
}

static void part_color(part *p, rgb c)
{
    p->color = c;
    p->own_color = 1;
}

/* Limbs: one line from the pivot down `len`, out by `spread`. */
static vp3d_mesh *limb(float px, float py, float len, float spread)
{
    vp3d_mesh *m = mesh_new();
    add_line(m, px, py, 0, px + spread, py - len, 0);
    return mesh_done(m, 40);
}

/* Two legs and two arms, swinging. */
static void add_limbs(model *md, float hip_y, float hip_x, float leg, float sh_y, float sh_x, float arm,
                      float arm_out)
{
    add_part(md, limb(-hip_x, hip_y, leg, -0.4f), A_LEG_L, vp_v3(0, hip_y, 0), VP3D_WIREFRAME);
    add_part(md, limb( hip_x, hip_y, leg,  0.4f), A_LEG_R, vp_v3(0, hip_y, 0), VP3D_WIREFRAME);
    if (arm > 0) {
        add_part(md, limb(-sh_x, sh_y, arm, -arm_out), A_ARM_L, vp_v3(0, sh_y, 0), VP3D_WIREFRAME);
        add_part(md, limb( sh_x, sh_y, arm,  arm_out), A_ARM_R, vp_v3(0, sh_y, 0), VP3D_WIREFRAME);
    }
}

/* ---- the models ---------------------------------------------------------- */

static void build_player(model *md)
{
    vp3d_mesh *m = mesh_new();
    add_box(m, 0, 7.6f, 0, 4.2f, 4.4f, 2.6f);          /* torso */
    add_octa(m, 0, 11.4f, 0, 1.6f, 1.7f, 1.6f);        /* head */
    add_part(md, mesh_done(m, 30), A_NONE, vp_v3(0, 0, 0), VP3D_SOLID);
    add_limbs(md, 5.4f, 1.1f, 5.4f, 9.4f, 2.4f, 4.4f, 0.8f);
    md->height = 13;
    md->color = mkrgb(255, 255, 255);
    /* the head and arms in the original's cyan-blue */
    part_color(&md->parts[3], mkrgb(80, 160, 255));
    part_color(&md->parts[4], mkrgb(80, 160, 255));
}

static void build_grunt(model *md)
{
    vp3d_mesh *m = mesh_new();
    add_box(m, 0, 7.8f, 0, 6.4f, 5.2f, 4.0f);          /* body */
    add_box(m, 0, 11.6f, 0, 3.4f, 2.4f, 3.0f);         /* head */
    add_part(md, mesh_done(m, 30), A_NONE, vp_v3(0, 0, 0), VP3D_SOLID);
    add_limbs(md, 5.2f, 1.6f, 5.2f, 9.6f, 3.6f, 5.0f, 1.2f);
    md->height = 13;
    md->color = mkrgb(255, 30, 30);
    vp3d_mesh *fists = mesh_new();                     /* the yellow face plate */
    add_line(fists, -1.2f, 11.8f, -1.6f, 1.2f, 11.8f, -1.6f);
    add_line(fists, -1.2f, 8.6f, -2.1f, 1.2f, 8.6f, -2.1f);
    part_color(add_part(md, mesh_done(fists, 40), A_NONE, vp_v3(0, 0, 0), VP3D_WIREFRAME),
               mkrgb(255, 220, 0));
}

static void build_hulk(model *md)
{
    vp3d_mesh *m = mesh_new();
    add_box(m, 0, 10.5f, 0, 10.0f, 7.0f, 6.0f);        /* the chest */
    add_box(m, 0, 15.4f, 0.4f, 4.0f, 2.8f, 3.6f);      /* the head */
    add_box(m, 0, 6.2f, 0, 6.0f, 1.8f, 4.4f);          /* the belt */
    add_part(md, mesh_done(m, 30), A_NONE, vp_v3(0, 0, 0), VP3D_SOLID);
    vp3d_mesh *l = mesh_new();
    add_box(l, -2.0f, 2.7f, 0, 2.2f, 5.4f, 2.6f);
    add_part(md, mesh_done(l, 30), A_LEG_L, vp_v3(0, 5.4f, 0), VP3D_SOLID);
    vp3d_mesh *r = mesh_new();
    add_box(r, 2.0f, 2.7f, 0, 2.2f, 5.4f, 2.6f);
    add_part(md, mesh_done(r, 30), A_LEG_R, vp_v3(0, 5.4f, 0), VP3D_SOLID);
    vp3d_mesh *al = mesh_new();
    add_box(al, -6.3f, 10.0f, 0, 2.2f, 6.0f, 2.4f);
    add_part(md, mesh_done(al, 30), A_ARM_L, vp_v3(0, 13.0f, 0), VP3D_SOLID);
    vp3d_mesh *ar = mesh_new();
    add_box(ar, 6.3f, 10.0f, 0, 2.2f, 6.0f, 2.4f);
    add_part(md, mesh_done(ar, 30), A_ARM_R, vp_v3(0, 13.0f, 0), VP3D_SOLID);
    md->height = 17;
    md->color = mkrgb(40, 255, 40);
    part_color(&md->parts[0], mkrgb(40, 255, 40));
    part_color(&md->parts[1], mkrgb(255, 60, 40));
    part_color(&md->parts[2], mkrgb(255, 60, 40));
}

static void build_brain(model *md)
{
    vp3d_mesh *m = mesh_new();
    add_frustum(m, 8, 0, 0.6f, 11.0f, 4.6f, 13.6f, 3.6f, 0);   /* the big brain */
    add_frustum(m, 8, 0, 0.6f, 13.6f, 3.6f, 15.4f, 0, 0);
    add_part(md, mesh_done(m, 25), A_NONE, vp_v3(0, 0, 0), VP3D_SOLID);
    vp3d_mesh *grooves = mesh_new();                           /* its folds */
    add_line(grooves, 0, 15.3f, 0.6f, 0, 11.2f, -4.0f);
    add_line(grooves, -3.2f, 13.6f, 0.6f, 3.2f, 13.6f, 0.6f);
    part_color(add_part(md, mesh_done(grooves, 40), A_NONE, vp_v3(0, 0, 0), VP3D_WIREFRAME),
               mkrgb(255, 120, 255));
    vp3d_mesh *b = mesh_new();
    add_box(b, 0, 7.8f, 0, 3.6f, 4.4f, 2.4f);                  /* the little body */
    part_color(add_part(md, mesh_done(b, 30), A_NONE, vp_v3(0, 0, 0), VP3D_SOLID), mkrgb(80, 120, 255));
    add_limbs(md, 5.6f, 0.9f, 5.6f, 9.4f, 2.0f, 4.0f, 0.8f);
    md->height = 16;
    md->color = mkrgb(200, 60, 255);
    for (int i = 3; i < md->n; i++) part_color(&md->parts[i], mkrgb(80, 120, 255));
}

static void build_prog(model *md)
{
    /* a human, scrambled: a tall thin diamond on legs */
    vp3d_mesh *m = mesh_new();
    add_octa(m, 0, 9.0f, 0, 2.2f, 4.0f, 1.6f);
    add_part(md, mesh_done(m, 30), A_NONE, vp_v3(0, 0, 0), VP3D_SOLID);
    add_limbs(md, 5.2f, 1.0f, 5.2f, 0, 0, 0, 0);
    md->height = 13;
    md->color = mkrgb(255, 80, 255);
}

static void build_mommy(model *md)
{
    vp3d_mesh *m = mesh_new();
    add_frustum(m, 6, 0, 0, 3.0f, 3.4f, 9.0f, 1.1f, 0);    /* the dress */
    add_octa(m, 0, 10.9f, 0, 1.5f, 1.7f, 1.5f);            /* head */
    add_part(md, mesh_done(m, 30), A_NONE, vp_v3(0, 0, 0), VP3D_SOLID);
    add_limbs(md, 3.2f, 0.9f, 3.2f, 8.6f, 1.6f, 4.0f, 1.0f);
    vp3d_mesh *hair = mesh_new();                          /* the yellow hair */
    add_line(hair, -1.6f, 11.8f, 0.6f, 0, 12.8f, 1.0f);
    add_line(hair, 0, 12.8f, 1.0f, 1.6f, 11.8f, 0.6f);
    part_color(add_part(md, mesh_done(hair, 40), A_NONE, vp_v3(0, 0, 0), VP3D_WIREFRAME),
               mkrgb(255, 230, 0));
    md->height = 13;
    md->color = mkrgb(255, 90, 160);
}

static void build_daddy(model *md)
{
    vp3d_mesh *m = mesh_new();
    add_box(m, 0, 7.9f, 0, 4.4f, 5.0f, 2.6f);              /* suit */
    add_octa(m, 0, 11.8f, 0, 1.6f, 1.7f, 1.6f);            /* head */
    add_part(md, mesh_done(m, 30), A_NONE, vp_v3(0, 0, 0), VP3D_SOLID);
    vp3d_mesh *hat = mesh_new();
    add_box(hat, 0, 13.6f, 0, 3.8f, 0.8f, 3.8f);
    add_box(hat, 0, 14.6f, 0, 2.4f, 1.4f, 2.4f);
    part_color(add_part(md, mesh_done(hat, 30), A_NONE, vp_v3(0, 0, 0), VP3D_SOLID), mkrgb(255, 220, 140));
    add_limbs(md, 5.4f, 1.1f, 5.4f, 9.8f, 2.6f, 4.4f, 0.8f);
    md->height = 15;
    md->color = mkrgb(70, 110, 255);
}

static void build_mikey(model *md)
{
    vp3d_mesh *m = mesh_new();
    add_box(m, 0, 5.0f, 0, 3.0f, 3.0f, 2.0f);
    add_octa(m, 0, 7.6f, 0, 1.3f, 1.4f, 1.3f);
    add_part(md, mesh_done(m, 30), A_NONE, vp_v3(0, 0, 0), VP3D_SOLID);
    add_limbs(md, 3.5f, 0.8f, 3.5f, 6.2f, 1.7f, 2.8f, 0.6f);
    md->height = 9;
    md->color = mkrgb(255, 60, 60);
    for (int i = 1; i < md->n; i++) part_color(&md->parts[i], mkrgb(60, 255, 60));
}

static void build_spheroid(model *md)
{
    /* three rings, turning: drawn wireframe, see-through */
    vp3d_mesh *m = mesh_new();
    add_circle(m, 12, 5.0f, 0);
    add_circle(m, 12, 5.0f, 1);
    add_circle(m, 12, 5.0f, 2);
    add_part(md, mesh_done(m, 40), A_SPIN_ALL, vp_v3(0, 0, 0), VP3D_WIREFRAME);
    md->height = 10;
    md->color = mkrgb(255, 80, 20);
}

static void build_enforcer(model *md)
{
    vp3d_mesh *m = mesh_new();
    add_octa(m, 0, 0, 0, 4.4f, 1.6f, 4.4f);                /* the saucer */
    add_frustum(m, 4, 0, 0, 1.4f, 1.8f, 3.6f, 0, PI / 4);   /* its tower */
    add_part(md, mesh_done(m, 30), A_SPIN, vp_v3(0, 0, 0), VP3D_SOLID);
    md->height = 8;
    md->color = mkrgb(40, 255, 120);
}

static void build_spark(model *md)
{
    vp3d_mesh *m = mesh_new();
    add_line(m, -2.4f, 0, 0, 2.4f, 0, 0);
    add_line(m, 0, -2.4f, 0, 0, 2.4f, 0);
    add_line(m, 0, 0, -2.4f, 0, 0, 2.4f);
    add_part(md, mesh_done(m, 40), A_SPIN_ALL, vp_v3(0, 0, 0), VP3D_WIREFRAME);
    md->height = 4;
    md->color = mkrgb(255, 255, 255);
}

static void build_quark(model *md)
{
    /* a spinning star of lines through its center */
    vp3d_mesh *m = mesh_new();
    const float r = 5.0f, d = 3.0f;
    add_line(m, -r, 0, 0, r, 0, 0);
    add_line(m, 0, -r, 0, 0, r, 0);
    add_line(m, 0, 0, -r, 0, 0, r);
    add_line(m, -d, -d, -d, d, d, d);
    add_line(m, -d, d, -d, d, -d, d);
    add_line(m, d, -d, -d, -d, d, d);
    add_line(m, d, d, -d, -d, -d, d);
    add_part(md, mesh_done(m, 40), A_SPIN_ALL, vp_v3(0, 0, 0), VP3D_WIREFRAME);
    md->height = 10;
    md->color = mkrgb(120, 200, 255);
}

static void build_tank(model *md)
{
    vp3d_mesh *m = mesh_new();
    add_box(m, 0, 2.2f, 0, 9.0f, 3.2f, 10.0f);             /* the tracks and hull */
    add_part(md, mesh_done(m, 30), A_NONE, vp_v3(0, 0, 0), VP3D_SOLID);
    vp3d_mesh *t = mesh_new();
    add_box(t, 0, 5.2f, 0, 5.0f, 2.8f, 5.0f);              /* the turret */
    add_line(t, 0, 5.4f, 2.5f, 0, 6.6f, 7.0f);             /* the gun, raised */
    add_part(md, mesh_done(t, 30), A_SPIN, vp_v3(0, 0, 0), VP3D_SOLID);
    md->height = 8;
    md->color = mkrgb(255, 120, 40);
    part_color(&md->parts[1], mkrgb(255, 220, 60));
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
    /* skull and crossbones lying on the floor, facing the camera */
    vp3d_mesh *m = mesh_new();
    static const float skull[6][3] = {
        {-2.6f, 0.3f, 2.6f}, {2.6f, 0.3f, 2.6f}, {3.0f, 0.3f, 5.6f}, {1.6f, 0.3f, 8.0f},
        {-1.6f, 0.3f, 8.0f}, {-3.0f, 0.3f, 5.6f} };
    add_poly(m, skull, 6, 1);
    add_line(m, -1.6f, 0.3f, 5.6f, -0.6f, 0.3f, 5.6f);     /* eyes */
    add_line(m, 0.6f, 0.3f, 5.6f, 1.6f, 0.3f, 5.6f);
    add_line(m, -4.4f, 0.3f, -1.0f, 4.4f, 0.3f, 3.4f);     /* bones */
    add_line(m, -4.4f, 0.3f, 3.4f, 4.4f, 0.3f, -1.0f);
    add_part(md, mesh_done(m, 40), A_NONE, vp_v3(0, 0, 0), VP3D_WIREFRAME);
    md->height = 4;
    md->color = mkrgb(255, 255, 255);
}

/* Electrodes: a different shape every few waves, as in the original. */
static void build_electrode(model *md, int shape)
{
    vp3d_mesh *m = mesh_new();
    vp3d_mode mode = VP3D_SOLID;
    switch (shape) {
    case 0: add_box(m, 0, 3.5f, 0, 6.0f, 7.0f, 6.0f); break;                 /* a block */
    case 1: add_octa(m, 0, 4.5f, 0, 3.8f, 4.5f, 3.8f); break;                /* a diamond */
    case 2: add_frustum(m, 3, 0, 0, 0, 4.6f, 8.0f, 0, 0); break;             /* a pyramid */
    case 3:                                                                   /* a star */
        add_box(m, 0, 3.5f, 0, 8.0f, 2.0f, 2.0f);
        add_box(m, 0, 3.5f, 0, 2.0f, 2.0f, 8.0f);
        add_box(m, 0, 3.5f, 0, 2.0f, 7.0f, 2.0f);
        break;
    case 4: add_frustum(m, 6, 0, 0, 0, 4.0f, 3.0f, 4.0f, 0);                  /* a drum */
            add_frustum(m, 6, 0, 0, 3.0f, 2.0f, 6.0f, 0, 0); break;
    default: {                                                                /* a cage */
        add_circle(m, 8, 4.0f, 1);
        add_line(m, -4.0f, 0, 0, 0, 7.0f, 0);
        add_line(m, 4.0f, 0, 0, 0, 7.0f, 0);
        add_line(m, 0, 0, -4.0f, 0, 7.0f, 0);
        add_line(m, 0, 0, 4.0f, 0, 7.0f, 0);
        mode = VP3D_WIREFRAME;
        break;
    }
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

void model_draw(vp_dvg *d, const vp3d_view *v, model_id id, const model_pose *p)
{
    const model *md = &M[id];
    vp_xf base = vp_xf_mul2(vp_xf_translate(p->x, p->y, p->z),
                            vp_xf_mul2(vp_xf_scale(p->sxz, p->sy, p->sxz), vp_xf_rot_y(p->heading)));
    float swing = 0.6f * sinf(p->walk);
    rgb last = mkrgb(1, 2, 3);
    int have = 0;
    for (int i = 0; i < md->n; i++) {
        const part *pt = &md->parts[i];
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
        if (!have || c.r != last.r || c.g != last.g || c.b != last.b) {
            vp_color(d, c.r, c.g, c.b);
            last = c;
            have = 1;
        }
        vp3d_draw_mesh(d, v, pt->mesh, &place, pt->mode);
    }
}
