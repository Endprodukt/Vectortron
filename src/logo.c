/* logo.c — the marquee's lettering as vectors: chunky rounded letters, a
 * yellow outline round a red one (the marquee's red letters with a yellow
 * edge), and 2084 below in yellow, its 0 a diamond and its 8 pointed, as on
 * the marquee.
 *
 * Every glyph is one or more closed contours on a 14 high grid (holes
 * marked). The corners are rounded when drawn (each becomes a short arc),
 * and the inner line is the contour moved into the letter's body.
 */
#include "logo.h"
#include "models.h"         /* rgb */

#include <math.h>
#include <string.h>

typedef struct { float x, y; } pt;
typedef struct { int n, hole; pt p[14]; } contour;
typedef struct { float w; int n; contour c[3]; } glyph;

#define H 14.0f

static const glyph G_V = { 10, 1, { { 7, 0, { {0,14},{3,14},{5,4},{7,14},{10,14},{6.5f,0},{3.5f,0} } } } };
static const glyph G_E = { 9.5f, 1, { { 12, 0, { {0,0},{9.5f,0},{9.5f,3},{3,3},{3,5.5f},{8.5f,5.5f},{8.5f,8.5f},{3,8.5f},{3,11},{9.5f,11},{9.5f,14},{0,14} } } } };
static const glyph G_C = { 10, 1, { { 8, 0, { {0,0},{10,0},{10,3},{3,3},{3,11},{10,11},{10,14},{0,14} } } } };
static const glyph G_T = { 10, 1, { { 8, 0, { {0,14},{10,14},{10,11},{6.5f,11},{6.5f,0},{3.5f,0},{3.5f,11},{0,11} } } } };
static const glyph G_O = { 10, 2, { { 4, 0, { {0,0},{10,0},{10,14},{0,14} } },
                                    { 4, 1, { {3,3},{3,11},{7,11},{7,3} } } } };
static const glyph G_R = { 10, 2, { { 10, 0, { {0,0},{3,0},{3,5.5f},{5.6f,5.5f},{7.4f,0},{10,0},{8.1f,5.9f},{10,7.5f},{10,14},{0,14} } },
                                    { 4, 1, { {3,8.5f},{3,11},{7,11},{7,8.5f} } } } };
static const glyph G_N = { 10, 1, { { 10, 0, { {0,0},{3,0},{3,8.2f},{7,0},{10,0},{10,14},{7,14},{7,5.8f},{3,14},{0,14} } } } };

static const glyph G_2 = { 10, 1, { { 12, 0, { {0,0},{10,0},{10,3},{3.5f,3},{3.5f,6},{10,6},{10,14},{0,14},{0,11},{6.5f,11},{6.5f,8.5f},{0,8.5f} } } } };
static const glyph G_0 = { 10, 2, { { 6, 0, { {5,0},{10,4},{10,10},{5,14},{0,10},{0,4} } },
                                    { 6, 1, { {5,3.6f},{3,5.3f},{3,8.7f},{5,10.4f},{7,8.7f},{7,5.3f} } } } };
static const glyph G_8 = { 10, 3, { { 9, 0, { {0,0},{7.5f,0},{10,2.5f},{10,5},{8.4f,7},{10,9},{10,11.5f},{7.5f,14},{0,14} } },
                                    { 4, 1, { {3,3},{3,5.5f},{7,5.5f},{7,3} } },
                                    { 4, 1, { {3,8.5f},{3,11},{7,11},{7,8.5f} } } } };
static const glyph G_4 = { 10, 2, { { 11, 0, { {6,0},{9,0},{9,3},{10,3},{10,6},{9,6},{9,14},{6,14},{0,6},{0,3},{6,3} } },
                                    { 3, 1, { {6,6},{6,9.4f},{3.4f,6} } } } };

static const glyph *glyph_of(char c)
{
    switch (c) {
    case 'V': return &G_V;
    case 'E': return &G_E;
    case 'C': return &G_C;
    case 'T': return &G_T;
    case 'O': return &G_O;
    case 'R': return &G_R;
    case 'N': return &G_N;
    case '2': return &G_2;
    case '0': return &G_0;
    case '8': return &G_8;
    case '4': return &G_4;
    default:  return NULL;
    }
}

#define MAXP 48

/* The contour with its corners rounded: each becomes three points, the arc's
 * middle taken from the corner's quadratic curve. */
static int rounded(const contour *c, float r, pt *out)
{
    int n = 0;
    for (int i = 0; i < c->n; i++) {
        pt a = c->p[(i + c->n - 1) % c->n], p = c->p[i], b = c->p[(i + 1) % c->n];
        float d1 = hypotf(p.x - a.x, p.y - a.y), d2 = hypotf(b.x - p.x, b.y - p.y);
        float rr = r;
        if (rr > d1 / 2) rr = d1 / 2;
        if (rr > d2 / 2) rr = d2 / 2;
        pt p1 = { p.x + (a.x - p.x) / d1 * rr, p.y + (a.y - p.y) / d1 * rr };
        pt p2 = { p.x + (b.x - p.x) / d2 * rr, p.y + (b.y - p.y) / d2 * rr };
        pt m = { 0.25f * p1.x + 0.5f * p.x + 0.25f * p2.x, 0.25f * p1.y + 0.5f * p.y + 0.25f * p2.y };
        out[n++] = p1;
        out[n++] = m;
        out[n++] = p2;
    }
    /* a short edge rounded from both ends leaves two points on one spot:
     * drop the doubles (an edge of no length has no direction) */
    int m = 0;
    for (int i = 0; i < n; i++) {
        pt prev = out[(i + n - 1) % n];
        if (fabsf(out[i].x - prev.x) < 1e-3f && fabsf(out[i].y - prev.y) < 1e-3f) continue;
        out[m++] = out[i];
    }
    return m;
}

/* The contour moved by `d` into the letter's body: into the outer contour,
 * out of a hole. */
static void inset(const pt *in, int n, int hole, float d, pt *out)
{
    float area = 0;
    for (int i = 0; i < n; i++) {
        const pt *p = &in[i], *q = &in[(i + 1) % n];
        area += p->x * q->y - q->x * p->y;
    }
    float side = (area > 0 ? 1.0f : -1.0f) * (hole ? -1.0f : 1.0f);    /* the body's side of each edge */
    for (int i = 0; i < n; i++) {
        pt a = in[(i + n - 1) % n], p = in[i], b = in[(i + 1) % n];
        float l1 = hypotf(p.x - a.x, p.y - a.y), l2 = hypotf(b.x - p.x, b.y - p.y);
        /* the body-side normals of the two edges, and their bisector */
        float n1x = -(p.y - a.y) / l1 * side, n1y = (p.x - a.x) / l1 * side;
        float n2x = -(b.y - p.y) / l2 * side, n2y = (b.x - p.x) / l2 * side;
        float bx = n1x + n2x, by = n1y + n2y, bl = hypotf(bx, by);
        if (bl < 1e-4f) { bx = n1x; by = n1y; bl = 1; }
        bx /= bl;
        by /= bl;
        float k = bx * n1x + by * n1y;          /* the miter: longer at sharper corners */
        if (k < 0.5f) k = 0.5f;
        out[i].x = p.x + bx * d / k;
        out[i].y = p.y + by * d / k;
    }
}

static void polyline(vp_dvg *d, const pt *p, int n, float ox, float oy, float s)
{
    vp_moveto(d, (int)(ox + p[0].x * s), (int)(oy + p[0].y * s));
    for (int i = 1; i <= n; i++) vp_lineto(d, (int)(ox + p[i % n].x * s), (int)(oy + p[i % n].y * s));
}

/* A word, its left edge at ox, baseline oy, letters h high. The outline in
 * `edge`, the inner line in `fill` (none when its brightness is 0). */
static void word(vp_dvg *d, const char *s, float ox, float oy, float h, float gap, float r, float in,
                 rgb edge, rgb fill)
{
    float scale = h / H;
    pt outer[MAXP], inner[MAXP];
    for (; *s; s++) {
        const glyph *g = glyph_of(*s);
        if (!g) continue;
        for (int c = 0; c < g->n; c++) {
            int n = rounded(&g->c[c], r, outer);
            vp_color(d, edge.r, edge.g, edge.b);
            polyline(d, outer, n, ox, oy, scale);
            if (fill.r || fill.g || fill.b) {
                inset(outer, n, g->c[c].hole, in, inner);
                vp_color(d, fill.r, fill.g, fill.b);
                polyline(d, inner, n, ox, oy, scale);
            }
        }
        ox += (g->w + gap) * scale;
    }
}

static float width_of(const char *s, float gap)
{
    float w = 0;
    for (; *s; s++) {
        const glyph *g = glyph_of(*s);
        if (g) w += g->w + gap;
    }
    return w - gap;
}

void logo_draw(vp_dvg *d, float cx, float y, float h, double t)
{
    /* the marquee's colors; the red breathes a little */
    rgb yellow = mkrgb(255, 230, 0);
    float k = 0.8f + 0.2f * sinf((float)t * 2.5f);
    rgb red = mkrgb((int)(255 * k), (int)(40 * k), (int)(30 * k));
    const float gap = 2.0f, scale = h / H;
    word(d, "VECTORTRON", cx - width_of("VECTORTRON", gap) * scale / 2, y, h, gap, 1.3f, 1.0f, yellow, red);

    /* 2084 below, smaller, yellow with a dimmer line inside */
    float h2 = h * 0.5f, s2 = h2 / H, y2 = y - h2 - h * 0.18f;
    word(d, "2084", cx - width_of("2084", gap) * s2 / 2, y2, h2, gap, 0.6f, 1.1f, yellow, mkrgb(150, 110, 0));
}
