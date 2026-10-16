/* bind.c — actions from the cabinet's advmame.rc, or from the owner's own
 * bindings, read straight from SDL (keys, joystick buttons, axes, hats). */
#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "bind.h"

#include <stdlib.h>
#include <string.h>

typedef enum { IN_NONE, IN_KEY, IN_BUTTON, IN_AXIS, IN_HAT } in_type;

typedef struct {
    in_type type;
    int     joy;        /* joystick number, 0 = the first */
    int     code;       /* scancode, button, axis or hat number */
    int     dir;        /* axis: -1 / +1; hat: SDL_HAT_* bit */
} input;

/* the cabinet's advmame.rc names for each action, tried in turn */
static const char *const CABINET[ACT_COUNT][3] = {
    [ACT_UP]         = { "p1_up", "p1_doubleleft_up", NULL },
    [ACT_DOWN]       = { "p1_down", "p1_doubleleft_down", NULL },
    [ACT_LEFT]       = { "p1_left", "p1_doubleleft_left", NULL },
    [ACT_RIGHT]      = { "p1_right", "p1_doubleleft_right", NULL },
    [ACT_FIRE_UP]    = { "p1_doubleright_up", "p2_up", NULL },
    [ACT_FIRE_DOWN]  = { "p1_doubleright_down", "p2_down", NULL },
    [ACT_FIRE_LEFT]  = { "p1_doubleright_left", "p2_left", NULL },
    [ACT_FIRE_RIGHT] = { "p1_doubleright_right", "p2_right", NULL },
    [ACT_FIRE]       = { "p1_button1", NULL, NULL },
    [ACT_STRAFE]     = { "p1_button2", NULL, NULL },
    [ACT_CAMERA]     = { "p1_button3", NULL, NULL },
    [ACT_START1]     = { "start1", NULL, NULL },
    [ACT_START2]     = { "start2", NULL, NULL },
    [ACT_COIN1]      = { "coin1", NULL, NULL },
    [ACT_COIN2]      = { "coin2", NULL, NULL },
    [ACT_SERVICE]    = { "service", "ui_configure", NULL },
};

/* analog sticks the cabinet names: past half travel counts as the direction */
static const struct { action a; const char *axis; int dir; } CABINET_AXES[] = {
    { ACT_LEFT, "p1_stickx", -1 }, { ACT_RIGHT, "p1_stickx", 1 },
    { ACT_UP, "p1_sticky", -1 },   { ACT_DOWN, "p1_sticky", 1 },
    { ACT_FIRE_LEFT, "p2_stickx", -1 }, { ACT_FIRE_RIGHT, "p2_stickx", 1 },
    { ACT_FIRE_UP, "p2_sticky", -1 },   { ACT_FIRE_DOWN, "p2_sticky", 1 },
};

static const char *const NAMES[ACT_COUNT] = {
    "MOVE UP", "MOVE DOWN", "MOVE LEFT", "MOVE RIGHT",
    "FIRE UP", "FIRE DOWN", "FIRE LEFT", "FIRE RIGHT",
    "FIRE FACING", "STRAFE", "CAMERA", "START 1", "START 2", "COIN 1", "COIN 2", "SERVICE",
};

static const char *const KEYS[ACT_COUNT] = {
    "bind_up", "bind_down", "bind_left", "bind_right",
    "bind_fire_up", "bind_fire_down", "bind_fire_left", "bind_fire_right",
    "bind_fire", "bind_strafe", "bind_camera", "bind_start1", "bind_start2",
    "bind_coin1", "bind_coin2", "bind_service",
};

static input B[ACT_COUNT][BIND_SLOTS];
static int held_now[ACT_COUNT], held_before[ACT_COUNT];

#define MAX_JOYS 8
static SDL_Joystick *J[MAX_JOYS];
static int njoys = -1;

static struct {
    int active, slot;
    action a;
    Uint32 start;
    unsigned char keys[SDL_NUM_SCANCODES];
    unsigned char buttons[MAX_JOYS][32];
    signed char axes[MAX_JOYS][16];
    unsigned char hats[MAX_JOYS][4];
} CAP;

static int any_before, any_now;

static void open_joysticks(void)
{
    int n = SDL_NumJoysticks();
    if (n == njoys) return;
    for (int i = 0; i < MAX_JOYS; i++) if (J[i]) { SDL_JoystickClose(J[i]); J[i] = NULL; }
    for (int i = 0; i < n && i < MAX_JOYS; i++) J[i] = SDL_JoystickOpen(i);
    njoys = n;
}

void bind_init(void)
{
    open_joysticks();
}

static int input_active(const input *in)
{
    switch (in->type) {
    case IN_KEY: {
        const Uint8 *k = SDL_GetKeyboardState(NULL);
        return in->code > 0 && in->code < SDL_NUM_SCANCODES && k[in->code];
    }
    case IN_BUTTON:
        return in->joy < MAX_JOYS && J[in->joy] && SDL_JoystickGetButton(J[in->joy], in->code);
    case IN_AXIS: {
        if (in->joy >= MAX_JOYS || !J[in->joy]) return 0;
        int v = SDL_JoystickGetAxis(J[in->joy], in->code);
        return in->dir < 0 ? v < -16000 : v > 16000;
    }
    case IN_HAT:
        return in->joy < MAX_JOYS && J[in->joy] && (SDL_JoystickGetHat(J[in->joy], in->code) & in->dir);
    default:
        return 0;
    }
}

static int bound(action a) { return B[a][0].type != IN_NONE || B[a][1].type != IN_NONE; }

void bind_update(vp_input *in)
{
    open_joysticks();
    for (int a = 0; a < ACT_COUNT; a++) {
        held_before[a] = held_now[a];
        int h = 0;
        if (bound((action)a)) {
            for (int s = 0; s < BIND_SLOTS; s++) h |= input_active(&B[a][s]);
        } else {
            for (int k = 0; k < 3 && CABINET[a][k]; k++) h |= vp_input_held(in, CABINET[a][k]);
            for (size_t k = 0; k < sizeof CABINET_AXES / sizeof CABINET_AXES[0]; k++) {
                if (CABINET_AXES[k].a != (action)a) continue;
                float v = vp_input_axis(in, CABINET_AXES[k].axis);
                if (CABINET_AXES[k].dir < 0 ? v < -0.5f : v > 0.5f) h = 1;
            }
        }
        held_now[a] = h;
    }
    /* anything at all, for "press any button" */
    any_before = any_now;
    any_now = 0;
    int nk = 0;
    const Uint8 *k = SDL_GetKeyboardState(&nk);
    for (int i = 1; i < nk && !any_now; i++) if (k[i] && i != SDL_SCANCODE_ESCAPE) any_now = 1;
    for (int j = 0; j < MAX_JOYS && !any_now; j++) {
        if (!J[j]) continue;
        for (int b = 0; b < SDL_JoystickNumButtons(J[j]); b++) if (SDL_JoystickGetButton(J[j], b)) any_now = 1;
    }
}

int act_held(action a) { return a < ACT_COUNT && held_now[a]; }
int act_pressed(action a) { return a < ACT_COUNT && held_now[a] && !held_before[a]; }
int bind_any_pressed(void) { return any_now && !any_before; }

const char *act_name(action a) { return a < ACT_COUNT ? NAMES[a] : "?"; }

static void describe(const input *in, char *buf, size_t size)
{
    switch (in->type) {
    case IN_KEY: {
        const char *n = SDL_GetScancodeName((SDL_Scancode)in->code);
        snprintf(buf, size, "KEY %s", n && *n ? n : "?");
        for (char *p = buf; *p; p++) if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 32);
        break;
    }
    case IN_BUTTON: snprintf(buf, size, "JOY %d BUTTON %d", in->joy + 1, in->code + 1); break;
    case IN_AXIS:   snprintf(buf, size, "JOY %d AXIS %d %c", in->joy + 1, in->code + 1, in->dir < 0 ? '-' : '+'); break;
    case IN_HAT: {
        const char *d = in->dir == SDL_HAT_UP ? "UP" : in->dir == SDL_HAT_DOWN ? "DOWN"
                      : in->dir == SDL_HAT_LEFT ? "LEFT" : "RIGHT";
        snprintf(buf, size, "JOY %d HAT %s", in->joy + 1, d);
        break;
    }
    default: snprintf(buf, size, "-"); break;
    }
}

void bind_describe(action a, int slot, char *buf, size_t size)
{
    if (!bound(a)) {
        snprintf(buf, size, slot == 0 ? "CABINET" : "-");
        return;
    }
    describe(&B[a][slot], buf, size);
}

/* ---- capturing ----------------------------------------------------------- */

static void snapshot(void)
{
    int nk = 0;
    const Uint8 *k = SDL_GetKeyboardState(&nk);
    memset(CAP.keys, 0, sizeof CAP.keys);
    for (int i = 0; i < nk && i < SDL_NUM_SCANCODES; i++) CAP.keys[i] = k[i];
    memset(CAP.buttons, 0, sizeof CAP.buttons);
    memset(CAP.axes, 0, sizeof CAP.axes);
    memset(CAP.hats, 0, sizeof CAP.hats);
    for (int j = 0; j < MAX_JOYS; j++) {
        if (!J[j]) continue;
        for (int b = 0; b < SDL_JoystickNumButtons(J[j]) && b < 32; b++) CAP.buttons[j][b] = SDL_JoystickGetButton(J[j], b);
        for (int x = 0; x < SDL_JoystickNumAxes(J[j]) && x < 16; x++) {
            int v = SDL_JoystickGetAxis(J[j], x);
            CAP.axes[j][x] = (signed char)(v < -16000 ? -1 : v > 16000 ? 1 : 0);
        }
        for (int h = 0; h < SDL_JoystickNumHats(J[j]) && h < 4; h++) CAP.hats[j][h] = SDL_JoystickGetHat(J[j], h);
    }
}

void bind_capture_begin(action a, int slot)
{
    CAP.active = 1;
    CAP.a = a;
    CAP.slot = slot;
    CAP.start = SDL_GetTicks();
    snapshot();
}

int bind_capture(void)
{
    if (!CAP.active) return -1;
    if (SDL_GetTicks() - CAP.start > 5000) { CAP.active = 0; return -1; }
    input got = { IN_NONE, 0, 0, 0 };
    int nk = 0;
    const Uint8 *k = SDL_GetKeyboardState(&nk);
    for (int i = 1; i < nk && i < SDL_NUM_SCANCODES && got.type == IN_NONE; i++) {
        if (i == SDL_SCANCODE_ESCAPE) continue;             /* the cabinet's exit: never bound */
        if (k[i] && !CAP.keys[i]) { got.type = IN_KEY; got.code = i; }
        if (!k[i]) CAP.keys[i] = 0;                          /* released: may be taken next time */
    }
    for (int j = 0; j < MAX_JOYS && got.type == IN_NONE; j++) {
        if (!J[j]) continue;
        for (int b = 0; b < SDL_JoystickNumButtons(J[j]) && b < 32 && got.type == IN_NONE; b++) {
            int v = SDL_JoystickGetButton(J[j], b);
            if (v && !CAP.buttons[j][b]) { got.type = IN_BUTTON; got.joy = j; got.code = b; }
            if (!v) CAP.buttons[j][b] = 0;
        }
        for (int x = 0; x < SDL_JoystickNumAxes(J[j]) && x < 16 && got.type == IN_NONE; x++) {
            int v = SDL_JoystickGetAxis(J[j], x);
            int d = v < -20000 ? -1 : v > 20000 ? 1 : 0;
            if (d && d != CAP.axes[j][x]) { got.type = IN_AXIS; got.joy = j; got.code = x; got.dir = d; }
            if (v > -8000 && v < 8000) CAP.axes[j][x] = 0;
        }
        for (int h = 0; h < SDL_JoystickNumHats(J[j]) && h < 4 && got.type == IN_NONE; h++) {
            int v = SDL_JoystickGetHat(J[j], h);
            static const int DIRS[4] = { SDL_HAT_UP, SDL_HAT_DOWN, SDL_HAT_LEFT, SDL_HAT_RIGHT };
            for (int q = 0; q < 4 && got.type == IN_NONE; q++)
                if ((v & DIRS[q]) && !(CAP.hats[j][h] & DIRS[q])) { got.type = IN_HAT; got.joy = j; got.code = h; got.dir = DIRS[q]; }
            CAP.hats[j][h] = (unsigned char)(CAP.hats[j][h] & v);
        }
    }
    if (got.type == IN_NONE) return 0;
    if (!bound(CAP.a)) memset(B[CAP.a], 0, sizeof B[CAP.a]);
    B[CAP.a][CAP.slot] = got;
    CAP.active = 0;
    /* the input just bound must not also count as a press this step */
    held_now[CAP.a] = held_before[CAP.a] = 1;
    any_now = any_before = 1;
    return 1;
}

void bind_clear(action a) { memset(B[a], 0, sizeof B[a]); }
void bind_clear_all(void) { memset(B, 0, sizeof B); }

/* ---- the settings file ------------------------------------------------------ */

static int parse_input(const char *s, input *in)
{
    char kind[16];
    int a = 0, b = 0;
    char d = 0;
    memset(in, 0, sizeof *in);
    if (sscanf(s, " key:%d", &a) == 1) { in->type = IN_KEY; in->code = a; return 1; }
    if (sscanf(s, " joy%d:button%d", &a, &b) == 2) { in->type = IN_BUTTON; in->joy = a; in->code = b; return 1; }
    if (sscanf(s, " joy%d:axis%d%c", &a, &b, &d) == 3) {
        in->type = IN_AXIS; in->joy = a; in->code = b; in->dir = d == '-' ? -1 : 1; return 1;
    }
    if (sscanf(s, " joy%d:hat%d:%15[a-z]", &a, &b, kind) == 3) {
        in->type = IN_HAT; in->joy = a; in->code = b;
        in->dir = !strcmp(kind, "up") ? SDL_HAT_UP : !strcmp(kind, "down") ? SDL_HAT_DOWN
                : !strcmp(kind, "left") ? SDL_HAT_LEFT : SDL_HAT_RIGHT;
        return 1;
    }
    return 0;
}

static void format_input(const input *in, char *buf, size_t size)
{
    switch (in->type) {
    case IN_KEY:    snprintf(buf, size, "key:%d", in->code); break;
    case IN_BUTTON: snprintf(buf, size, "joy%d:button%d", in->joy, in->code); break;
    case IN_AXIS:   snprintf(buf, size, "joy%d:axis%d%c", in->joy, in->code, in->dir < 0 ? '-' : '+'); break;
    case IN_HAT:
        snprintf(buf, size, "joy%d:hat%d:%s", in->joy, in->code,
                 in->dir == SDL_HAT_UP ? "up" : in->dir == SDL_HAT_DOWN ? "down" : in->dir == SDL_HAT_LEFT ? "left" : "right");
        break;
    default: snprintf(buf, size, "none"); break;
    }
}

int bind_parse(const char *key, const char *value)
{
    for (int a = 0; a < ACT_COUNT; a++) {
        if (strcmp(key, KEYS[a])) continue;
        memset(B[a], 0, sizeof B[a]);
        char copy[256];
        snprintf(copy, sizeof copy, "%s", value);
        int slot = 0;
        for (char *tok = strtok(copy, ","); tok && slot < BIND_SLOTS; tok = strtok(NULL, ","))
            if (parse_input(tok, &B[a][slot])) slot++;
        return 1;
    }
    return 0;
}

void bind_save(FILE *f)
{
    for (int a = 0; a < ACT_COUNT; a++) {
        char s0[48], s1[48];
        format_input(&B[a][0], s0, sizeof s0);
        format_input(&B[a][1], s1, sizeof s1);
        if (B[a][1].type != IN_NONE) fprintf(f, "%-16s = %s, %s\n", KEYS[a], s0, s1);
        else fprintf(f, "%-16s = %s\n", KEYS[a], s0);
    }
}

/* ---- the service menu's keys ---------------------------------------------------- */

static int raw_key(int sc) { const Uint8 *k = SDL_GetKeyboardState(NULL); return k[sc]; }

static int joy0_dir(int axis, int sign, int hatbit)
{
    if (!J[0]) return 0;
    int v = SDL_JoystickNumAxes(J[0]) > axis ? SDL_JoystickGetAxis(J[0], axis) : 0;
    if (sign < 0 ? v < -16000 : v > 16000) return 1;
    return SDL_JoystickNumHats(J[0]) > 0 && (SDL_JoystickGetHat(J[0], 0) & hatbit);
}

static int joy0_button(int b) { return J[0] && SDL_JoystickNumButtons(J[0]) > b && SDL_JoystickGetButton(J[0], b); }

void bind_menu(menu_keys *m)
{
    static int prev[7], hold[4];
    int now[7];
    now[0] = act_held(ACT_UP) || act_held(ACT_FIRE_UP) || raw_key(SDL_SCANCODE_UP) || joy0_dir(1, -1, SDL_HAT_UP);
    now[1] = act_held(ACT_DOWN) || act_held(ACT_FIRE_DOWN) || raw_key(SDL_SCANCODE_DOWN) || joy0_dir(1, 1, SDL_HAT_DOWN);
    now[2] = act_held(ACT_LEFT) || act_held(ACT_FIRE_LEFT) || raw_key(SDL_SCANCODE_LEFT) || joy0_dir(0, -1, SDL_HAT_LEFT);
    now[3] = act_held(ACT_RIGHT) || act_held(ACT_FIRE_RIGHT) || raw_key(SDL_SCANCODE_RIGHT) || joy0_dir(0, 1, SDL_HAT_RIGHT);
    now[4] = act_held(ACT_FIRE) || act_held(ACT_START1) || raw_key(SDL_SCANCODE_RETURN) || raw_key(SDL_SCANCODE_KP_ENTER)
          || joy0_button(0);
    now[5] = act_held(ACT_STRAFE) || raw_key(SDL_SCANCODE_BACKSPACE) || joy0_button(1);
    now[6] = act_held(ACT_SERVICE) || raw_key(SDL_SCANCODE_F2);
    int *out[7] = { &m->up, &m->down, &m->left, &m->right, &m->ok, &m->back, &m->service };
    for (int i = 0; i < 7; i++) {
        int fire = now[i] && !prev[i];
        if (i < 4) {
            /* held: repeat after a third of a second, then 8 times a second */
            if (now[i]) hold[i]++; else hold[i] = 0;
            if (hold[i] > 20 && (hold[i] - 20) % 7 == 0) fire = 1;
        }
        *out[i] = fire;
        prev[i] = now[i];
    }
}
