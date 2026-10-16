/* font3d.h — letters standing in the world, drawn as 3D lines. */
#ifndef VT_FONT3D_H
#define VT_FONT3D_H

#include <vectorpie/vp.h>

enum { FONT3D_LEFT, FONT3D_CENTER, FONT3D_RIGHT };

/* `s` (A-Z 0-9 -) from `origin` (the baseline) along `right`, letters `up`,
 * both unit vectors; `h` the letter height in world units. */
void font3d_text(vp_dvg *d, const vp3d_view *v, const char *s, vp_vec3 origin, vp_vec3 right, vp_vec3 up,
                 float h, int align);

#endif
