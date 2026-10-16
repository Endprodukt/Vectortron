/* font3d.c — a stroke font standing in the world (the score on the far wall,
 * as in Robotron X). 16-segment style: each glyph is a set of segments
 * between nine points on a 2 x 2 cell:
 *
 *    0---1---2
 *    |       |
 *    3---4---5
 *    |       |
 *    6---7---8
 */
#include "font3d.h"

#include <string.h>

static const float PX[9] = { 0, 0.5f, 1, 0, 0.5f, 1, 0, 0.5f, 1 };
static const float PY[9] = { 1, 1, 1, 0.5f, 0.5f, 0.5f, 0, 0, 0 };

/* Pairs of point indices; "" for space. */
static const char *glyph(char c)
{
    switch (c) {
    case '0': return "0228866026";
    case '1': return "17681331";
    case '2': return "0225533668";
    case '3': return "02288635";
    case '4': return "033528";
    case '5': return "200334488776";
    case '6': return "2006688553";
    case '7': return "0228";
    case '8': return "0228866035";
    case '9': return "5330022886";
    case 'A': return "60022835";
    case 'B': return "06011534455886";
    case 'C': return "200668";
    case 'D': return "060115588776";
    case 'E': return "20066834";
    case 'F': return "200634";
    case 'G': return "2006688554";
    case 'H': return "062835";
    case 'I': return "021768";
    case 'J': return "28877663";
    case 'K': return "063238";
    case 'L': return "0668";
    case 'M': return "06044228";
    case 'N': return "060882";
    case 'O': return "02288660";
    case 'P': return "06022553";
    case 'Q': return "0228866048";
    case 'R': return "0602255348";
    case 'S': return "2003355886";
    case 'T': return "0217";
    case 'U': return "066828";
    case 'V': return "0727";
    case 'W': return "06644882";
    case 'X': return "0826";
    case 'Y': return "042447";
    case 'Z': return "022686";
    case '-': return "35";
    default:  return "";
    }
}

void font3d_text(vp_dvg *d, const vp3d_view *v, const char *s, vp_vec3 origin, vp_vec3 right, vp_vec3 up,
                 float h, int align)
{
    float adv = h * 0.8f;               /* letter width 0.55 h, gap 0.25 h */
    float width = adv * (float)strlen(s) - h * 0.25f;
    float start = align == FONT3D_CENTER ? -width / 2 : align == FONT3D_RIGHT ? -width : 0;
    for (int i = 0; s[i]; i++) {
        char c = s[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        const char *g = glyph(c);
        float x0 = start + adv * i;
        int n = (int)strlen(g);
        for (int k = 0; k + 1 < n; k += 2) {
            int a = g[k] - '0', b = g[k + 1] - '0';
            vp_vec3 pa = vp_v3_add(origin, vp_v3_add(vp_v3_scale(right, x0 + PX[a] * h * 0.55f),
                                                    vp_v3_scale(up, PY[a] * h)));
            vp_vec3 pb = vp_v3_add(origin, vp_v3_add(vp_v3_scale(right, x0 + PX[b] * h * 0.55f),
                                                    vp_v3_scale(up, PY[b] * h)));
            vp3d_line(d, v, pa, pb);
        }
    }
}
