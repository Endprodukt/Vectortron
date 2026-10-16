/* service.h — the service menu: game settings, the picture's size and
 * position, the controls, sound, resets.
 *
 * Opened in the attract mode with the service button (F2, or SERVICE as
 * bound) or by holding buttons 2 and 3 together for two seconds. Every change
 * takes effect at once; leaving saves save/settings.txt.
 */
#ifndef VT_SERVICE_H
#define VT_SERVICE_H

#include <vectorpie/vp.h>

void service_open(void);
int  service_active(void);
/* One step; returns 1 when the owner leaves the menu. */
int  service_update(vp_game *g);
void service_draw(vp_dvg *d, double t);

/* main.c: put the settings in force (credits, picture, sound, effects...) */
void game_apply_settings(void);
/* main.c: the high score table was reset */
void game_scores_reset(void);

#endif
