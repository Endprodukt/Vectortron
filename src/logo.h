/* logo.h — the marquee's lettering, VECTORTRON and 2084, as vectors. */
#ifndef VT_LOGO_H
#define VT_LOGO_H

#include <vectorpie/vp.h>

/* The logo centered on x = cx: VECTORTRON with its baseline at y and
 * letters h high, 2084 below it. Board coordinates. */
void logo_draw(vp_dvg *d, float cx, float y, float h, double t);

#endif
