/* screen.h — the picture's size and position on the monitor.
 *
 * Every line the game draws, its own and the SDK's (3D, text), goes through
 * vp_moveto / vp_lineto; the build wraps both (ld --wrap) so that each point
 * is scaled about the middle and moved here first. */
#ifndef VT_SCREEN_H
#define VT_SCREEN_H

/* size in percent (100: the whole board), offset in board units */
void screen_set(int size_x, int size_y, int pos_x, int pos_y);

#endif
