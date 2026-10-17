/* Vectortron — a 3D vector tribute to Robotron: 2084, for VectorPie.
 *
 * main.c: the machine around the game: coins and credits, the attract mode
 * (title, the robots and their points, the hall of fame, a demo), one or
 * two players taking turns, the initials of a high score.
 */
#include <vectorpie/vp.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bind.h"
#include "controls.h"
#include "game.h"
#include "models.h"
#include "render.h"
#include "screen.h"
#include "service.h"
#include "settings.h"
#include "sound.h"

#define W VP_DVG_COORD_MAX

enum { S_ATTRACT, S_PLAY, S_INITIALS, S_SERVICE };
enum { A_TITLE, A_ROSTER, A_SCORES, A_DEMO, A_PAGES };

static struct {
    int        state;
    int        page;            /* attract page */
    float      time;            /* seconds in the state / page */
    vp_credits credits;
    world      pw[2];           /* each player's own world, as the original keeps it */
    int        players, cur;
    float      banner;          /* "PLAYER 2" banner, frames */
    float      tunnel;          /* the between-waves effect, frames; 0 = none */
    float      over_time;       /* game over, frames */
    vp_initials entry;
    int        entry_player;    /* the player entering initials, 0/1 */
    int        entry_pending[2];
    vp_score   table[VP_HISCORE_MAX];
    int        table_n;
    int        last_rank;
    world      demo;
    unsigned   seed;
    int        autoplay;
    int        service_hold;    /* frames buttons 2 + 3 have been held together */
    int        service_from;    /* the state the service menu was opened from: back to it */
    vp_game   *game;
} G;

static int hiscore(void) { return G.table_n > 0 ? G.table[0].score : 0; }

static void reload_table(void) { G.table_n = vp_hiscore_load(G.table); }

static void goto_attract(int page)
{
    G.state = S_ATTRACT;
    G.page = page;
    G.time = 0;
    if (page == A_DEMO) {
        world_new_game(&G.demo, 1, 0, 5, G.seed++);
        G.demo.demo = 1;
        G.demo.wave = 1 + (int)(G.seed % 4);
        world_start_wave(&G.demo);
        render_camera(&G.demo, SET.camera, 1);
    }
}

static void start_game(int players)
{
    G.state = S_PLAY;
    G.players = players;
    G.cur = 0;
    G.time = 0;
    G.tunnel = 0;
    G.over_time = 0;
    G.banner = players > 1 ? 120 : 0;
    G.entry_pending[0] = G.entry_pending[1] = 0;
    for (int p = 0; p < players; p++) {
        world_new_game(&G.pw[p], SET.lives, SET.extra_man, SET.difficulty, G.seed++);
        world_start_wave(&G.pw[p]);
    }
    fx_clear();
    vp_lamps_set(0);
    snd_play(players > 1 ? SND_START2 : SND_START);
    render_camera(&G.pw[0], SET.camera, 1);
}

/* The next player with men left, after `from`; -1 if none. */
static int next_player(int from)
{
    for (int k = 1; k <= G.players; k++) {
        int p = (from + k) % G.players;
        if (G.pw[p].lives > 0) return p;
    }
    return -1;
}

static void begin_initials_or_attract(void)
{
    for (int p = 0; p < G.players; p++) {
        if (G.entry_pending[p]) {
            G.entry_pending[p] = 0;
            G.entry_player = p;
            G.state = S_INITIALS;
            G.time = 0;
            vp_initials_begin(&G.entry, 1, 30.0f);
            /* GETHIM: the air raid for the top score, else the tune */
            snd_play(vp_hiscore_rank(G.pw[p].score) == 0 ? SND_HISCORE_TOP : SND_HISCORE);
            return;
        }
    }
    reload_table();
    goto_attract(A_SCORES);
}

/* ---- the loop ------------------------------------------------------------ */

/* The settings in force: called at start and by the service menu. */
void game_apply_settings(void)
{
    G.credits.free_play = SET.free_play;
    screen_set(SET.size_x, SET.size_y, SET.pos_x, SET.pos_y);
    fx_set_level(SET.effects);
    vp_audio_set_master(vp_game_audio(G.game), SET.volume / 10.0f);
}

void game_scores_reset(void)
{
    reload_table();
    G.last_rank = -1;
}

static int init(vp_game *g)
{
    G.game = g;
    bind_init();
    settings_load();
    vp_credits_init(&G.credits);
    snd_init(vp_game_audio(g));
    models_init();
    render_init();
    game_apply_settings();
    reload_table();
    G.seed = 2084;
    G.last_rank = -1;
    goto_attract(A_TITLE);
    /* for development: VT_WAVE=n starts a game at wave n at once,
     * VT_AUTOPLAY=1 lets the demo player play it */
    const char *wave = getenv("VT_WAVE");
    if (wave) {
        start_game(1);
        G.pw[0].wave = atoi(wave) > 0 ? atoi(wave) : 1;
        world_start_wave(&G.pw[0]);
    }
    G.autoplay = getenv("VT_AUTOPLAY") != NULL;
    return VP_OK;
}

static void shutdown(vp_game *g)
{
    (void)g;
    models_free();
}

static void step_play(vp_input *in)
{
    world *w = &G.pw[G.cur];
    controls c;

    if (G.banner > 0) {          /* "PLAYER n": the world waits */
        G.banner--;
        render_camera(w, SET.camera, 0);
        return;
    }
    controls_read(in, &c, &w->face_x, &w->face_z);
    if (G.autoplay) demo_controls(w, &c);
    if (c.camera) {
        SET.camera = (SET.camera + 1) % CAMERA_MODES;
        settings_save();
    }

    if (G.tunnel > 0) {
        if (--G.tunnel <= 0) {
            w->wave++;
            world_start_wave(w);
            render_camera(w, SET.camera, 1);
        }
        return;
    }

    world_step(w, &c);
    fx_step();
    render_camera(w, SET.camera, 0);

    if (w->ph == PH_CLEARED && w->ph_time > 50) {
        G.tunnel = 80;
        snd_play(SND_WAVE_END);
        fx_clear();
    } else if (w->ph == PH_OVER) {
        if (w->ph_time == 1) {
            if (vp_hiscore_rank(w->score) >= 0) G.entry_pending[G.cur] = 1;
        }
        if (w->ph_time > 150) {
            int nx = next_player(G.cur);
            if (nx < 0) {
                begin_initials_or_attract();
            } else {
                G.cur = nx;
                G.banner = 120;
                fx_clear();
                world_resume(&G.pw[G.cur]);
                render_camera(&G.pw[G.cur], SET.camera, 1);
            }
        }
    } else if (w->ph == PH_RESTART) {
        /* a man lost: the other player's turn, if there is one */
        int nx = next_player(G.cur);
        if (nx >= 0 && nx != G.cur) {
            G.cur = nx;
            G.banner = 120;
        }
        fx_clear();
        world_resume(&G.pw[G.cur]);
        render_camera(&G.pw[G.cur], SET.camera, 1);
    }
}

static void update(vp_game *g, float dt)
{
    vp_input *in = vp_game_input(g);
    bind_update(in);
    snd_tick();
    if (G.state != S_SERVICE) {
        int coins = act_pressed(ACT_COIN1) + act_pressed(ACT_COIN2);
        if (coins) {
            G.credits.credits += coins;
            if (G.credits.credits > G.credits.max) G.credits.credits = G.credits.max;
            snd_play(SND_COIN);
        }
    }
    G.time += dt;
    /* the attract mode's demo is silent when the owner says so */
    int quiet = G.state == S_ATTRACT && G.page == A_DEMO && !SET.attract_sound;
    vp_audio_set_master(vp_game_audio(g), quiet ? 0.0f : SET.volume / 10.0f);

    switch (G.state) {
    case S_SERVICE:
        if (service_update(g)) {
            if (G.service_from == S_PLAY) {
                G.state = S_PLAY;       /* back into the paused game */
            } else {
                fx_clear();
                goto_attract(A_TITLE);
            }
        }
        break;
    case S_ATTRACT: {
        vp_credits_blink(&G.credits, vp_game_time(g));
        if (act_pressed(ACT_START1) && vp_credits_take(&G.credits, 1)) { start_game(1); return; }
        if (SET.two_players && act_pressed(ACT_START2) && vp_credits_take(&G.credits, 2)) { start_game(2); return; }
        /* the service menu: the service button, or buttons 2 + 3 held */
        menu_keys mk;
        bind_menu(&mk);
        if (act_held(ACT_STRAFE) && act_held(ACT_CAMERA)) G.service_hold++; else G.service_hold = 0;
        if (mk.service || G.service_hold > 120) {
            G.service_hold = 0;
            G.service_from = S_ATTRACT;
            G.state = S_SERVICE;
            vp_lamps_set(0);
            service_open();
            return;
        }
        static const float LEN[A_PAGES] = { 9, 14, 8, 40 };
        if (G.page == A_DEMO) {
            controls c;
            demo_controls(&G.demo, &c);
            world_step(&G.demo, &c);
            fx_step();
            render_camera(&G.demo, SET.camera, 0);
            if (G.demo.ph == PH_RESTART || G.demo.ph == PH_OVER || G.demo.ph == PH_CLEARED ||
                G.time > LEN[A_DEMO]) {
                fx_clear();
                goto_attract(A_TITLE);
            }
        } else if (G.time > LEN[G.page]) {
            goto_attract((G.page + 1) % A_PAGES);
        }
        break;
    }
    case S_PLAY: {
        /* the service button pauses the game into the service menu (not the
         * 2 + 3 chord: those are strafe and camera here) */
        menu_keys mk;
        bind_menu(&mk);
        if (mk.service) {
            G.service_from = S_PLAY;
            G.state = S_SERVICE;
            snd_stop_all();
            service_open();
            return;
        }
        step_play(in);
        break;
    }
    case S_INITIALS: {
        world *w = &G.pw[G.entry_player];
        if (vp_initials_update(&G.entry, in, dt)) {
            G.last_rank = vp_hiscore_insert(G.entry.name, w->score);
            begin_initials_or_attract();
        }
        break;
    }
    }
}

/* ---- the attract pages --------------------------------------------------- */

static void draw_credits_line(vp_dvg *d, double t)
{
    char buf[48];
    vp_text_font(VP_FONT_ASTEROIDS);
    if (G.credits.free_play) snprintf(buf, sizeof buf, "FREE PLAY");
    else snprintf(buf, sizeof buf, "CREDITS %d", G.credits.credits);
    text(d, mkrgb(160, 160, 160), buf, W / 2.0f, 120, 80, VP_TEXT_CENTER);
    if ((int)(t * 2) & 1) {
        const char *msg = SET.two_players && vp_credits_enough(&G.credits, 2) ? "PRESS 1 OR 2 PLAYER START"
                        : vp_credits_enough(&G.credits, 1) ? "PRESS 1 PLAYER START" : "INSERT COIN";
        text(d, mkrgb(255, 255, 0), msg, W / 2.0f, 330, 110, VP_TEXT_CENTER);
    }
}

static void draw_title(vp_dvg *d, double t)
{
    vp3d_view v;
    vp3d_view_init(&v, 50);
    vp3d_look_at(&v, vp_v3(0, 30, -118), vp_v3(0, 9, 0), vp_v3(0, 1, 0));

    vp_text_font(VP_FONT_ASTEROIDS);
    /* the logo, in the original's color cycle, with a shadow behind */
    for (int k = 3; k >= 0; k--) {
        float hue = (float)t * 0.6f + k * 0.08f;
        float r = 0.5f + 0.5f * sinf(hue * 6.28f), gg = 0.5f + 0.5f * sinf(hue * 6.28f + 2.1f),
              b = 0.5f + 0.5f * sinf(hue * 6.28f + 4.2f);
        rgb c = mkrgb((int)(255 * r), (int)(255 * gg), (int)(255 * b));
        text(d, k ? rgb_scale(c, 0.35f) : c, "VECTORTRON", W / 2.0f + k * 12, 3250 - k * 12, 300,
             VP_TEXT_CENTER);
    }
    text(d, mkrgb(255, 255, 255), "2084", W / 2.0f, 2900, 160, VP_TEXT_CENTER);

    /* a family walking on a turntable */
    model_pose p;
    memset(&p, 0, sizeof p);
    p.dim = 1;
    vp3d_scene_begin(render_scene(), d, &v);
    static const model_id fam[4] = { MDL_DADDY, MDL_MOMMY, MDL_MIKEY, MDL_PLAYER };
    for (int i = 0; i < 4; i++) {
        float a = (float)t * 0.5f + i * 1.5707963f;
        p.x = 24 * sinf(a);
        p.z = 24 * cosf(a);
        p.heading = a + 1.5707963f;
        p.walk = (float)t * 9;
        p.sxz = p.sy = 1;
        p.color = model_color(fam[i]);
        model_draw(d, &v, render_scene(), fam[i], &p);
    }
    vp3d_scene_end(render_scene());
    vp_text_font(VP_FONT_HERSHEY);
    text(d, mkrgb(120, 200, 255), "Save the last human family", W / 2.0f, 980, 110, VP_TEXT_CENTER);
    vp_text_font(VP_FONT_ASTEROIDS);
    text(d, mkrgb(140, 140, 140), "A 3D TRIBUTE TO ROBOTRON 2084 BY WILLIAMS 1982", W / 2.0f, 760, 60,
         VP_TEXT_CENTER);
    draw_credits_line(d, t);
}

static void draw_roster(vp_dvg *d, double t)
{
    static const struct { model_id m; const char *name; const char *pts; } R[] = {
        { MDL_GRUNT,    "GRUNT",    "100" },
        { MDL_HULK,     "HULK",     "INDESTRUCTIBLE" },
        { MDL_BRAIN,    "BRAIN",    "500" },
        { MDL_SPHEROID, "SPHEROID", "1000" },
        { MDL_ENFORCER, "ENFORCER", "150" },
        { MDL_QUARK,    "QUARK",    "1000" },
        { MDL_TANK,     "TANK",     "200" },
        { MDL_ELECTRODE0, "ELECTRODE", "0" },
        { MDL_MOMMY,    "MOMMY",    "1000 - 5000" },
        { MDL_DADDY,    "DADDY",    "1000 - 5000" },
        { MDL_MIKEY,    "MIKEY",    "1000 - 5000" },
        { MDL_PROG,     "PROG",     "100" },
    };
    const int n = (int)(sizeof R / sizeof R[0]);
    vp_text_font(VP_FONT_ASTEROIDS);
    text(d, mkrgb(255, 255, 0), "MEET THE ROBOTRONS", W / 2.0f, 3780, 150, VP_TEXT_CENTER);

    /* the cards appear one after the other, in two columns */
    int shown = (int)(G.time / 0.8f) + 1;
    if (shown > n) shown = n;
    for (int i = 0; i < shown; i++) {
        int col = i / 6, row = i % 6;
        float cx = col ? 2250.0f : 250.0f, cy = 3250.0f - row * 520;
        /* each card its own camera, far enough that the model stands about
         * 420 board units tall, the model moved across the camera's plane
         * so that it lands at (cx, cy) */
        float h = model_height(R[i].m);
        if (h < 9) h = 9;
        vp3d_view vv;
        vp3d_view_init(&vv, 30);
        float a = (float)t * 1.2f + i;
        float far = h * (W / 420.0f) / 2 / (tanf(15 * 3.14159265f / 180) / vv.aspect);
        vp_vec3 tgt = vp_v3(0, h / 2, 0);
        vp_vec3 eye = vp_v3_add(tgt, vp_v3_scale(vp_v3_normalize(vp_v3(sinf(a), 0.45f, -cosf(a))), far));
        vp3d_look_at(&vv, eye, tgt, vp_v3(0, 1, 0));
        model_pose p;
        memset(&p, 0, sizeof p);
        p.sxz = p.sy = 1;
        p.dim = 1;
        p.walk = (float)t * 8;
        p.spin = (float)t * 3;
        p.color = model_color(R[i].m);
        float lift = (R[i].m == MDL_SPHEROID || R[i].m == MDL_QUARK || R[i].m == MDL_ENFORCER) ? 6 : 0;
        vp_vec3 fwd = vp_v3_normalize(vp_v3_sub(tgt, eye));
        vp_vec3 right = vp_v3_normalize(vp_v3_cross(vp_v3(0, 1, 0), fwd));
        vp_vec3 up = vp_v3_cross(fwd, right);
        float half_w = far * tanf(15 * 3.14159265f / 180), half_h = half_w / vv.aspect;
        float off_x = (cx + 300 - W / 2.0f) / (W / 2.0f), off_y = (cy + 100 - W / 2.0f) / (W / 2.0f);
        vp_vec3 move = vp_v3_add(vp_v3_scale(right, off_x * half_w), vp_v3_scale(up, off_y * half_h));
        p.x = move.x;
        p.y = move.y + lift;
        p.z = move.z;
        vp3d_scene_begin(render_scene(), d, &vv);
        model_draw(d, &vv, render_scene(), R[i].m, &p);
        vp3d_scene_end(render_scene());
        text(d, p.color, R[i].name, cx + 620, cy + 160, 100, VP_TEXT_LEFT);
        text(d, mkrgb(255, 255, 255), R[i].pts, cx + 620, cy - 20, 80, VP_TEXT_LEFT);
    }
    draw_credits_line(d, t);
}

static void draw_scores(vp_dvg *d, double t)
{
    vp_text_font(VP_FONT_ASTEROIDS);
    text(d, mkrgb(255, 60, 60), "HALL OF FAME", W / 2.0f, 3650, 200, VP_TEXT_CENTER);
    vp_color(d, 255, 255, 255);
    if (G.table_n == 0) text(d, mkrgb(255, 255, 255), "NO SCORES YET", W / 2.0f, 2400, 120, VP_TEXT_CENTER);
    else {
        vp_color(d, 120, 220, 255);
        vp_hiscore_draw(d, G.table, G.table_n, W / 2.0f, 3150, 120, G.last_rank, t);
    }
    draw_credits_line(d, t);
}

static void draw_play(vp_dvg *d, double t)
{
    world *w = &G.pw[G.cur];
    if (G.tunnel > 0) {
        render_wave_tunnel(d, 1 - G.tunnel / 80.0f, t);
        render_hud(d, &G.pw[0], &G.pw[1], G.players, G.cur, hiscore(), t);
        return;
    }
    render_world(d, w, t);
    if (SET.camera == 2) render_hud(d, &G.pw[0], &G.pw[1], G.players, G.cur, hiscore(), t);
    else render_wall_hud(d, &G.pw[0], &G.pw[1], G.players, G.cur, t);
    vp_text_font(VP_FONT_ASTEROIDS);
    if (G.banner > 0) {
        /* whose turn it is: big, blinking, with that player's score */
        char buf[24];
        if (((int)G.banner / 8) % 2 == 0) {
            snprintf(buf, sizeof buf, "PLAYER %d", G.cur + 1);
            text(d, G.cur ? mkrgb(80, 160, 255) : mkrgb(255, 60, 60), buf, W / 2.0f, 2250, 280, VP_TEXT_CENTER);
        }
        text(d, mkrgb(255, 255, 0), "GET READY", W / 2.0f, 1850, 140, VP_TEXT_CENTER);
        snprintf(buf, sizeof buf, "%d", G.pw[G.cur].score);
        text(d, mkrgb(255, 255, 255), buf, W / 2.0f, 1550, 110, VP_TEXT_CENTER);
    }
    if (w->ph == PH_OVER) {
        render_big_text(d, "GAME OVER", mkrgb(255, 60, 60), W / 2.0f, 2150, 280, 1, t);
        if (G.players > 1) {
            char buf[24];
            snprintf(buf, sizeof buf, "PLAYER %d", G.cur + 1);
            text(d, mkrgb(255, 255, 0), buf, W / 2.0f, 1800, 160, VP_TEXT_CENTER);
        }
    }
}

static void draw(vp_game *g, vp_dvg *d)
{
    double t = vp_game_time(g);
    switch (G.state) {
    case S_SERVICE:
        service_draw(d, t);
        break;
    case S_ATTRACT:
        switch (G.page) {
        case A_TITLE:  draw_title(d, t); break;
        case A_ROSTER: draw_roster(d, t); break;
        case A_SCORES: draw_scores(d, t); break;
        case A_DEMO:
            render_world(d, &G.demo, t);
            vp_text_font(VP_FONT_ASTEROIDS);
            text(d, mkrgb(255, 255, 255), "DEMO", W / 2.0f, 3900, 100, VP_TEXT_CENTER);
            draw_credits_line(d, t);
            break;
        }
        break;
    case S_PLAY:
        draw_play(d, t);
        break;
    case S_INITIALS: {
        char buf[48];
        vp_text_font(VP_FONT_ASTEROIDS);
        if (G.players > 1) {
            snprintf(buf, sizeof buf, "PLAYER %d", G.entry_player + 1);
            text(d, mkrgb(255, 255, 0), buf, W / 2.0f, 3400, 160, VP_TEXT_CENTER);
        }
        text(d, mkrgb(255, 60, 60), "CONGRATULATIONS", W / 2.0f, 3000, 180, VP_TEXT_CENTER);
        text(d, mkrgb(120, 200, 255), "YOU HAVE JOINED THE HALL OF FAME", W / 2.0f, 2650, 90, VP_TEXT_CENTER);
        snprintf(buf, sizeof buf, "%d", G.pw[G.entry_player].score);
        text(d, mkrgb(255, 255, 255), buf, W / 2.0f, 2300, 150, VP_TEXT_CENTER);
        text(d, mkrgb(255, 255, 0), "ENTER YOUR INITIALS", W / 2.0f, 1800, 110, VP_TEXT_CENTER);
        vp_color(d, 255, 255, 255);
        vp_initials_draw(&G.entry, d, W / 2.0f, 1200, 260);
        break;
    }
    }
}

int main(int argc, char **argv)
{
    vp_game_desc desc = {0};
    desc.title = "Vectortron";
    desc.init = init;
    desc.update = update;
    desc.draw = draw;
    desc.shutdown = shutdown;
    desc.update_hz = 60;
    desc.frame_hz = 40;             /* the picture, on the board too (the logic runs at 60) */
    return vp_run(&desc, argc, argv);
}
