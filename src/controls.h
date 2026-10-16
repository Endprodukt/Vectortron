/* controls.h — moving and firing, twin stick or single stick.
 *
 * TWIN STICK (as the original): the left stick moves, the right stick fires
 * in its direction. The right stick is any of
 *   p1_doubleright_up/down/left/right   AdvanceMAME's names for Robotron's
 *                                       fire stick (when advmame.rc has them)
 *   p2_up/down/left/right               the second joystick (keys R F D G)
 *   p2_stickx / p2_sticky               the second joystick, analog
 * The left stick is p1_up/down/left/right, p1_stickx/sticky, or
 * p1_doubleleft_* when advmame.rc names it so.
 *
 * SINGLE STICK (as Robotron X allows): hold button 1 to fire the way you are
 * facing; facing follows the stick. Hold button 2 as well (or alone) to keep
 * the facing while you move: strafe.
 *
 * Both work at all times: the fire stick wins when it is pushed.
 */
#ifndef VT_CONTROLS_H
#define VT_CONTROLS_H

#include <vectorpie/vp.h>

typedef struct {
    float mx, mz;       /* move: -1..1 each, x right, z up the screen */
    float fx, fz;       /* fire direction (8-way, unit), valid when fire */
    int   fire;
    int   camera;       /* pressed: next camera */
} controls;

/* Read this step's controls. Players take turns on the same controls, as on
 * the original cabinet. `facing` is the single-stick fire direction, kept
 * between steps. */
void controls_read(vp_input *in, controls *c, float *facing_x, float *facing_z);

#endif
