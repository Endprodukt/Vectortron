/* bind.h — the game's actions and what triggers them.
 *
 * Read directly, never by advmame.rc's names. Every action is AUTO (what
 * AdvanceMAME gives it by default: its keys, the sticks, buttons, the
 * mouse's buttons), NONE, or up to BIND_SLOTS inputs of the owner's (the
 * service menu, CONTROLS): any key, joystick button, axis direction, hat
 * direction or mouse button. One input may do several things.
 *
 * Laying one out, as in every vector game of this cabinet: START 1 or
 * Enter on its line, then the input; the first since the cursor came to
 * the line replaces what it had, every further one is added. While it
 * waits: Enter (START 1 too on a line of the game's) leaves it as it was,
 * START 2 makes it NONE, Delete AUTO.
 */
#ifndef VT_BIND_H
#define VT_BIND_H

#include <stdio.h>
#include <vectorpie/vp.h>

typedef enum {
    ACT_UP, ACT_DOWN, ACT_LEFT, ACT_RIGHT,                      /* move */
    ACT_FIRE_UP, ACT_FIRE_DOWN, ACT_FIRE_LEFT, ACT_FIRE_RIGHT,  /* the fire stick */
    ACT_FIRE,           /* single stick: fire the way you face */
    ACT_STRAFE,         /* single stick: keep the facing */
    ACT_CAMERA,
    ACT_START1, ACT_START2,
    ACT_COIN1, ACT_COIN2,
    ACT_SERVICE,        /* the service menu */
    ACT_COUNT
} action;

#define BIND_SLOTS 4

void bind_init(void);
/* Once per step, after the SDK has read the controls. */
void bind_update(vp_input *in);

int  act_held(action a);
int  act_pressed(action a);         /* down since the last step */

const char *act_name(action a);     /* "FIRE UP" */
/* What an action is bound to, in one text: "AUTO", "NONE", "SPACE",
 * "SPACE + JOY1 BUTTON 3". */
void bind_describe(action a, char *buf, size_t size);
int  bind_is_bound(action a);       /* 0: AUTO or NONE */

/* Laying out from the menu: start waiting for an input for the action
 * (`replace`: it replaces what the action had, else it is added); then
 * call bind_capture() each step: 0 still waiting, 1 changed, -1 left as
 * it was. Inputs held when it starts are ignored until released. */
void bind_capture_begin(action a, int replace);
int  bind_capture(void);
int  bind_capturing(void);
int  bind_capture_adds(void);       /* waiting, and what comes is added */
void bind_clear(action a);          /* AUTO */
void bind_set_none(action a);       /* NONE */
void bind_toggle(action a);         /* NONE, or AUTO again */
void bind_clear_all(void);

/* The settings file: "bind_<action> = auto | off | <input>[, <input>...]". */
int  bind_parse(const char *key, const char *value);
void bind_save(FILE *f);

/* The service menu's own keys, whatever is bound: the arrow keys, any
 * stick or hat, START 1 / Enter / the keypad's Enter, Backspace, and F2 or
 * the service button. Up, down, left and right repeat while held. */
typedef struct { int up, down, left, right, ok, back, service; } menu_keys;
void bind_menu(menu_keys *m);

/* Any key or button at all (to leave a screen). */
int  bind_any_pressed(void);

#endif
