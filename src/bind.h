/* bind.h — the game's actions and what triggers them.
 *
 * Every action follows the cabinet's advmame.rc (the VectorPie way) until
 * the owner binds it otherwise in the service menu: up to two inputs each,
 * any key, joystick button, joystick axis direction or hat direction.
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

#define BIND_SLOTS 2

void bind_init(void);
/* Once per step, after the SDK has read the controls. */
void bind_update(vp_input *in);

int  act_held(action a);
int  act_pressed(action a);         /* down since the last step */

const char *act_name(action a);     /* "FIRE UP" */
/* What a slot is bound to: "KEY SPACE", "JOY 1 BUTTON 3", "JOY 1 AXIS 2 +",
 * "JOY 1 HAT UP", or "-" (slot 0 unbound: "CABINET"). */
void bind_describe(action a, int slot, char *buf, size_t size);

/* Binding from the menu: start waiting for an input for (a, slot); then
 * call bind_capture() each step: 0 still waiting, 1 bound, -1 gave up
 * (5 seconds). Inputs held when it starts are ignored until released. */
void bind_capture_begin(action a, int slot);
int  bind_capture(void);
void bind_clear(action a);          /* back to the cabinet's */
void bind_clear_all(void);

/* The settings file: "bind_<action> = <input>[, <input>]". */
int  bind_parse(const char *key, const char *value);
void bind_save(FILE *f);

/* The service menu's own keys: the bound actions, and always the arrow
 * keys, Return, Backspace, F2 and the first joystick's stick, hat and
 * buttons 1 / 2, so that no binding can lock the owner out. Up, down, left
 * and right repeat while held. */
typedef struct { int up, down, left, right, ok, back, service; } menu_keys;
void bind_menu(menu_keys *m);

/* Any key or button at all (to leave a screen). */
int  bind_any_pressed(void);

#endif
