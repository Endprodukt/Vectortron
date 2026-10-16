/* service.c — the service menu. */
#include "service.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "bind.h"
#include "models.h"
#include "render.h"
#include "settings.h"
#include "sound.h"

#define W VP_DVG_COORD_MAX

enum { PG_MAIN, PG_GAME, PG_DISPLAY, PG_CONTROLS, PG_INPUT, PG_SOUND, PG_COUNT };

static struct {
    int active;
    int page;
    int sel[PG_COUNT];
    int col;                /* controls: 0 slot 1, 1 slot 2, 2 clear */
    int capturing;
    int confirm;            /* the item waiting for a second press */
    int sound;              /* the sound test's sound */
    int hold;               /* frames the leave chord has been held */
    char message[64];
    int message_time;
} S;

void service_open(void)
{
    memset(&S, 0, sizeof S);
    S.active = 1;
    S.page = PG_MAIN;
}

int service_active(void) { return S.active; }

static void note(const char *msg)
{
    snprintf(S.message, sizeof S.message, "%s", msg);
    S.message_time = 120;
}

/* ---- the pages' items ---------------------------------------------------------- */

static const char *const MAIN_ITEMS[] = {
    "GAME SETTINGS", "DISPLAY", "CONTROLS", "INPUT TEST", "SOUND", "RESET HIGH SCORES",
    "RESET ALL SETTINGS", "EXIT",
};
#define MAIN_N 8

static const int EXTRA[] = { 0, 10000, 15000, 20000, 25000, 30000, 40000, 50000 };
#define EXTRA_N 8

enum { GM_FREE, GM_LIVES, GM_EXTRA, GM_DIFF, GM_TWO, GM_DEMO, GM_BACK, GM_N };
enum { DP_SX, DP_SY, DP_PX, DP_PY, DP_CAMERA, DP_FLOOR, DP_EFFECTS, DP_REFRESH, DP_RESET, DP_BACK, DP_N };
enum { SD_VOLUME, SD_TEST, SD_BACK, SD_N };
#define CT_N (ACT_COUNT + 2)       /* the actions, RESET ALL, BACK */

static int page_items(int page)
{
    switch (page) {
    case PG_MAIN:     return MAIN_N;
    case PG_GAME:     return GM_N;
    case PG_DISPLAY:  return DP_N;
    case PG_CONTROLS: return CT_N;
    case PG_SOUND:    return SD_N;
    default:          return 1;
    }
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void change_game(int item, int d)
{
    switch (item) {
    case GM_FREE:  SET.free_play = !SET.free_play; break;
    case GM_LIVES: SET.lives = clampi(SET.lives + d, 1, 9); break;
    case GM_EXTRA: {
        int k = 0;
        for (int i = 0; i < EXTRA_N; i++) if (EXTRA[i] == SET.extra_man) k = i;
        SET.extra_man = EXTRA[clampi(k + d, 0, EXTRA_N - 1)];
        break;
    }
    case GM_DIFF:  SET.difficulty = clampi(SET.difficulty + d, 0, 10); break;
    case GM_TWO:   SET.two_players = !SET.two_players; break;
    case GM_DEMO:  SET.attract_sound = !SET.attract_sound; break;
    default: return;
    }
    game_apply_settings();
}

static void change_display(int item, int d)
{
    switch (item) {
    case DP_SX:      SET.size_x = clampi(SET.size_x + d, 50, 120); break;
    case DP_SY:      SET.size_y = clampi(SET.size_y + d, 50, 120); break;
    case DP_PX:      SET.pos_x = clampi(SET.pos_x + d * 10, -600, 600); break;
    case DP_PY:      SET.pos_y = clampi(SET.pos_y + d * 10, -600, 600); break;
    case DP_CAMERA:  SET.camera = (SET.camera + d + 3) % 3; break;
    case DP_FLOOR:   SET.floor = (SET.floor + 1 + d + 4) % 4 - 1; break;
    case DP_EFFECTS: SET.effects = clampi(SET.effects + d, 0, 2); break;
    case DP_REFRESH: SET.refresh = clampi(SET.refresh + d * 10, 30, 60); note("TAKES EFFECT AT THE NEXT START"); return;
    default: return;
    }
    game_apply_settings();
}

/* ---- the step ---------------------------------------------------------------------- */

int service_update(vp_game *g)
{
    (void)g;
    if (!S.active) return 1;
    if (S.message_time > 0) S.message_time--;

    if (S.capturing) {
        int r = bind_capture();
        if (r != 0) {
            S.capturing = 0;
            note(r > 0 ? "BOUND" : "NOTHING PRESSED - UNCHANGED");
        }
        return 0;
    }

    menu_keys k;
    bind_menu(&k);
    int *sel = &S.sel[S.page];
    int n = page_items(S.page);

    if (S.page == PG_INPUT) {
        /* every input is being tested: leave with the service key, or
         * hold START 1 and START 2 */
        if (act_held(ACT_START1) && act_held(ACT_START2)) S.hold++; else S.hold = 0;
        if (k.service || S.hold > 60) { S.page = PG_MAIN; S.hold = 0; }
        return 0;
    }

    if (k.up)   { *sel = (*sel + n - 1) % n; S.confirm = -1; }
    if (k.down) { *sel = (*sel + 1) % n; S.confirm = -1; }
    int d = k.right - k.left;

    switch (S.page) {
    case PG_MAIN:
        if (k.ok) {
            switch (*sel) {
            case 0: S.page = PG_GAME; break;
            case 1: S.page = PG_DISPLAY; break;
            case 2: S.page = PG_CONTROLS; break;
            case 3: S.page = PG_INPUT; break;
            case 4: S.page = PG_SOUND; break;
            case 5:
                if (S.confirm == 5) { remove(vp_hiscore_path()); game_scores_reset(); note("HIGH SCORES RESET"); S.confirm = -1; }
                else { S.confirm = 5; note("PRESS AGAIN TO RESET THE HIGH SCORES"); }
                break;
            case 6:
                if (S.confirm == 6) {
                    settings s0 = { .free_play = 0, .lives = 3, .extra_man = 25000, .difficulty = 5,
                                    .two_players = 1, .attract_sound = 1, .size_x = 100, .size_y = 100,
                                    .camera = 0, .floor = -1, .effects = 2, .refresh = 40, .volume = 8 };
                    SET = s0;
                    bind_clear_all();
                    game_apply_settings();
                    note("ALL SETTINGS RESET");
                    S.confirm = -1;
                } else { S.confirm = 6; note("PRESS AGAIN TO RESET ALL SETTINGS"); }
                break;
            default:
                settings_save();
                S.active = 0;
                return 1;
            }
        }
        if (k.back || k.service) { settings_save(); S.active = 0; return 1; }
        break;
    case PG_GAME:
        if (d) change_game(*sel, d);
        if (k.ok) { if (*sel == GM_BACK) S.page = PG_MAIN; else change_game(*sel, 1); }
        if (k.back) S.page = PG_MAIN;
        break;
    case PG_DISPLAY:
        if (d) change_display(*sel, d);
        if (k.ok) {
            if (*sel == DP_BACK) S.page = PG_MAIN;
            else if (*sel == DP_RESET) {
                SET.size_x = SET.size_y = 100;
                SET.pos_x = SET.pos_y = 0;
                game_apply_settings();
                note("PICTURE RESET");
            } else change_display(*sel, 1);
        }
        if (k.back) S.page = PG_MAIN;
        break;
    case PG_CONTROLS:
        if (d) S.col = clampi(S.col + d, 0, 2);
        if (k.ok) {
            if (*sel < ACT_COUNT) {
                if (S.col == 2) { bind_clear((action)*sel); note("BACK TO THE CABINET'S"); }
                else { bind_capture_begin((action)*sel, S.col); S.capturing = 1; }
            } else if (*sel == ACT_COUNT) {
                if (S.confirm == 100) { bind_clear_all(); note("ALL CONTROLS AS THE CABINET'S"); S.confirm = -1; }
                else { S.confirm = 100; note("PRESS AGAIN TO RESET ALL CONTROLS"); }
            } else {
                S.page = PG_MAIN;
            }
        }
        if (k.back) S.page = PG_MAIN;
        break;
    case PG_SOUND:
        if (*sel == SD_VOLUME && d) { SET.volume = clampi(SET.volume + d, 0, 10); game_apply_settings(); }
        if (*sel == SD_TEST && d) S.sound = (S.sound + d + SND_COUNT) % SND_COUNT;
        if (k.ok) {
            if (*sel == SD_TEST) snd_play((snd_id)S.sound);
            else if (*sel == SD_BACK) S.page = PG_MAIN;
        }
        if (k.back) S.page = PG_MAIN;
        break;
    default:
        break;
    }
    return 0;
}

/* ---- drawing ------------------------------------------------------------------------ */

static void line_item(vp_dvg *d, int i, int sel, const char *label, const char *value, float y, float h)
{
    rgb c = i == sel ? mkrgb(255, 255, 0) : mkrgb(170, 170, 200);
    text(d, c, label, 500, y, h, VP_TEXT_LEFT);
    if (value) text(d, i == sel ? mkrgb(255, 255, 255) : mkrgb(120, 200, 255), value, W - 500.0f, y, h, VP_TEXT_RIGHT);
    if (i == sel) text(d, mkrgb(255, 60, 60), ">", 320, y, h, VP_TEXT_LEFT);
}

static void test_pattern(vp_dvg *d)
{
    /* the board's edge, a grid, a circle and the corners: set the picture
     * so the edge just shows and the circle is round */
    vp_color(d, 0, 90, 140);
    for (int i = 1; i < 8; i++) {
        int x = i * W / 8;
        vp_moveto(d, x, 0); vp_lineto(d, x, W);
        vp_moveto(d, 0, x); vp_lineto(d, W, x);
    }
    vp_color(d, 255, 255, 255);
    vp_moveto(d, 0, 0); vp_lineto(d, W, 0); vp_lineto(d, W, W); vp_lineto(d, 0, W); vp_lineto(d, 0, 0);
    vp_color(d, 255, 60, 60);
    for (int i = 0; i <= 48; i++) {
        float a = i * 6.2831853f / 48;
        /* round on a 4:3 tube: the board's square is stretched 4:3 */
        int x = (int)(W / 2 + cosf(a) * 900 * 0.75f), y = (int)(W / 2 + sinf(a) * 900);
        if (i) vp_lineto(d, x, y); else vp_moveto(d, x, y);
    }
}

void service_draw(vp_dvg *d, double t)
{
    char v[64];
    vp_text_font(VP_FONT_ASTEROIDS);
    const float h = 100, step = 190;
    float y = 3450;
    int sel = S.sel[S.page];

    switch (S.page) {
    case PG_MAIN:
        text(d, mkrgb(255, 60, 60), "SERVICE MENU", W / 2.0f, 3750, 170, VP_TEXT_CENTER);
        for (int i = 0; i < MAIN_N; i++, y -= step + 40) line_item(d, i, sel, MAIN_ITEMS[i], NULL, y, 120);
        break;
    case PG_GAME: {
        text(d, mkrgb(255, 60, 60), "GAME SETTINGS", W / 2.0f, 3750, 160, VP_TEXT_CENTER);
        static const char *const DIFF[11] = { "0 EXTRA LIBERAL", "1", "2", "3 LIBERAL", "4", "5 RECOMMENDED",
                                              "6 CONSERVATIVE", "7", "8 EXTRA CONSERVATIVE", "9", "10" };
        line_item(d, GM_FREE, sel, "FREE PLAY", SET.free_play ? "ON" : "OFF", y, h); y -= step;
        snprintf(v, sizeof v, "%d", SET.lives);
        line_item(d, GM_LIVES, sel, "MEN PER GAME", v, y, h); y -= step;
        if (SET.extra_man) snprintf(v, sizeof v, "%d", SET.extra_man); else snprintf(v, sizeof v, "NONE");
        line_item(d, GM_EXTRA, sel, "EXTRA MAN EVERY", v, y, h); y -= step;
        line_item(d, GM_DIFF, sel, "DIFFICULTY", DIFF[SET.difficulty], y, h); y -= step;
        line_item(d, GM_TWO, sel, "2 PLAYER GAMES", SET.two_players ? "ON" : "OFF", y, h); y -= step;
        line_item(d, GM_DEMO, sel, "ATTRACT MODE SOUNDS", SET.attract_sound ? "ON" : "OFF", y, h); y -= step;
        line_item(d, GM_BACK, sel, "BACK", NULL, y, h);
        break;
    }
    case PG_DISPLAY: {
        test_pattern(d);
        text(d, mkrgb(255, 60, 60), "DISPLAY", W / 2.0f, 3750, 160, VP_TEXT_CENTER);
        static const char *const CAM[3] = { "ROBOTRON X", "ARENA", "CLASSIC" };
        static const char *const FLOOR[4] = { "AUTO", "TILES", "GRID", "NONE" };
        static const char *const FX[3] = { "OFF", "LIGHT", "FULL" };
        snprintf(v, sizeof v, "%d %%", SET.size_x);
        line_item(d, DP_SX, sel, "PICTURE WIDTH", v, y, h); y -= step;
        snprintf(v, sizeof v, "%d %%", SET.size_y);
        line_item(d, DP_SY, sel, "PICTURE HEIGHT", v, y, h); y -= step;
        snprintf(v, sizeof v, "%+d", SET.pos_x);
        line_item(d, DP_PX, sel, "PICTURE LEFT - RIGHT", v, y, h); y -= step;
        snprintf(v, sizeof v, "%+d", SET.pos_y);
        line_item(d, DP_PY, sel, "PICTURE DOWN - UP", v, y, h); y -= step;
        line_item(d, DP_CAMERA, sel, "CAMERA", CAM[SET.camera], y, h); y -= step;
        line_item(d, DP_FLOOR, sel, "FLOOR", FLOOR[SET.floor + 1], y, h); y -= step;
        line_item(d, DP_EFFECTS, sel, "EFFECTS", FX[SET.effects], y, h); y -= step;
        snprintf(v, sizeof v, "%d HZ", SET.refresh);
        line_item(d, DP_REFRESH, sel, "REFRESH ON THE BOARD", v, y, h); y -= step;
        line_item(d, DP_RESET, sel, "RESET PICTURE", NULL, y, h); y -= step;
        line_item(d, DP_BACK, sel, "BACK", NULL, y, h);
        break;
    }
    case PG_CONTROLS: {
        text(d, mkrgb(255, 60, 60), "CONTROLS", W / 2.0f, 3800, 150, VP_TEXT_CENTER);
        const float ch = 70, cs = 170;
        float yy = 3560;
        static const char *const COLS[3] = { "INPUT 1", "INPUT 2", "CLEAR" };
        static const float CX[3] = { 1350, 2450, 3550 };
        for (int c = 0; c < 3; c++)
            text(d, c == S.col ? mkrgb(255, 255, 0) : mkrgb(120, 120, 160), COLS[c], CX[c], yy + 40, 60, VP_TEXT_LEFT);
        yy -= cs;
        for (int a = 0; a < ACT_COUNT; a++, yy -= cs) {
            int on = a == sel;
            rgb lc = on ? mkrgb(255, 255, 0) : (act_held((action)a) ? mkrgb(60, 255, 60) : mkrgb(170, 170, 200));
            text(d, lc, act_name((action)a), 250, yy, ch, VP_TEXT_LEFT);
            for (int sl = 0; sl < 2; sl++) {
                char b[48];
                bind_describe((action)a, sl, b, sizeof b);
                rgb vc = on && S.col == sl ? mkrgb(255, 255, 255) : mkrgb(120, 200, 255);
                if (S.capturing && on && S.col == sl && ((int)(t * 4) & 1)) snprintf(b, sizeof b, "PRESS ...");
                text(d, vc, b, CX[sl], yy, ch * 0.85f, VP_TEXT_LEFT);
            }
            if (on && S.col == 2) text(d, mkrgb(255, 255, 255), "CLEAR", CX[2], yy, ch * 0.85f, VP_TEXT_LEFT);
        }
        line_item(d, ACT_COUNT, sel, "RESET ALL TO THE CABINET", NULL, yy - 20, ch);
        line_item(d, ACT_COUNT + 1, sel, "BACK", NULL, yy - 20 - cs, ch);
        if (S.capturing) {
            snprintf(v, sizeof v, "PRESS AN INPUT FOR %s", act_name((action)sel));
            text(d, mkrgb(255, 255, 0), v, W / 2.0f, 230, 80, VP_TEXT_CENTER);
        }
        break;
    }
    case PG_INPUT: {
        text(d, mkrgb(255, 60, 60), "INPUT TEST", W / 2.0f, 3750, 160, VP_TEXT_CENTER);
        for (int a = 0; a < ACT_COUNT; a++) {
            int col = a % 2;
            float yy = 3350 - (a / 2) * 300.0f;
            rgb c = act_held((action)a) ? mkrgb(60, 255, 60) : mkrgb(90, 90, 120);
            text(d, c, act_name((action)a), col ? 2300.0f : 400.0f, yy, 100, VP_TEXT_LEFT);
        }
        text(d, mkrgb(170, 170, 200), "SERVICE OR HOLD START 1 + START 2 TO RETURN", W / 2.0f, 500, 70, VP_TEXT_CENTER);
        break;
    }
    case PG_SOUND: {
        text(d, mkrgb(255, 60, 60), "SOUND", W / 2.0f, 3750, 160, VP_TEXT_CENTER);
        snprintf(v, sizeof v, "%d", SET.volume);
        line_item(d, SD_VOLUME, sel, "VOLUME", v, y, h); y -= step;
        char nm[48];
        snprintf(nm, sizeof nm, "%s", snd_name((snd_id)S.sound));
        for (char *p = nm; *p; p++) { if (*p == '_') *p = ' '; else if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 32); }
        line_item(d, SD_TEST, sel, "SOUND TEST", nm, y, h); y -= step;
        line_item(d, SD_BACK, sel, "BACK", NULL, y, h); y -= step * 2;
        text(d, mkrgb(120, 200, 255), snd_original() ? "ORIGINAL SOUNDS FROM THE SOUND ROM"
                                                     : "NO SOUND ROM FOUND - SOUNDS MADE IN CODE",
             W / 2.0f, y, 75, VP_TEXT_CENTER);
        break;
    }
    }
    if (S.page != PG_INPUT && !S.capturing)
        text(d, mkrgb(120, 120, 160), "UP DOWN SELECT   LEFT RIGHT CHANGE   FIRE OK   STRAFE BACK",
             W / 2.0f, 110, 60, VP_TEXT_CENTER);
    if (S.message_time > 0) text(d, mkrgb(255, 255, 255), S.message, W / 2.0f, 340, 80, VP_TEXT_CENTER);
}
