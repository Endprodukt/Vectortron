/* game.c — the rules, after the original's source.
 *
 * File and line references (RRP8.ASM:170 ...) are to historicalsource/robotron.
 * One call of world_step is one frame of the original (60 Hz).
 */
#include "game.h"

#include <math.h>
#include <string.h>

#include "render.h"
#include "sound.h"
#include "waves.h"

/* ---- the original's random helpers (RRS22.ASM:885-912) ------------------- */

static int rnd(world *w)
{
    w->rng ^= w->rng << 13;
    w->rng ^= w->rng >> 17;
    w->rng ^= w->rng << 5;
    return (int)((w->rng >> 9) & 0xFF);
}

/* 1..n, uniform */
static int randu(world *w, int n)
{
    if (n < 1) return 1;
    return 1 + rnd(w) * n / 256;
}

/* 1..n, weighted to the top (RMAX) */
static int rmax(world *w, int n)
{
    int r = rnd(w);
    if (n < 1) return 1;
    while (r > n) r >>= 1;
    return r ? r : 1;
}

/* a signed byte, as the original's SEED tests */
static int srnd(world *w) { return (int)(signed char)rnd(w); }

static float sgn(float v) { return v > 0 ? 1.0f : v < 0 ? -1.0f : 0.0f; }

/* ---- sizes: the original's sprites, in pixels x lines ------------------- */

typedef struct { float w, h; int points; } kind;

static const kind K[T_COUNT] = {
    [T_NONE]      = {  0,  0,    0 },
    [T_GRUNT]     = { 10, 13,  100 },
    [T_HULK]      = { 14, 16,    0 },
    [T_BRAIN]     = { 14, 16,  500 },
    [T_PROG]      = {  8, 14,  100 },
    [T_SPHEROID]  = { 16, 15, 1000 },
    [T_ENFORCER]  = { 10, 11,  150 },
    [T_QUARK]     = { 16, 15, 1000 },
    [T_TANK]      = { 14, 16,  200 },
    [T_ELECTRODE] = { 10,  9,    0 },
    [T_MOMMY]     = {  8, 14,    0 },
    [T_DADDY]     = { 10, 13,    0 },
    [T_MIKEY]     = {  6, 11,    0 },
    [T_SPARK]     = {  8,  7,   25 },
    [T_SHELL]     = {  8,  7,   25 },
    [T_CRUISE]    = {  6,  4,   25 },
};

#define PLAYER_W 8.0f
#define PLAYER_H 12.0f
/* The original tests pixel masks: boxes a little smaller than the sprites
 * play about the same. */
#define HIT 0.8f

static int is_human(int t) { return t == T_MOMMY || t == T_DADDY || t == T_MIKEY; }

static int overlap(float ax, float az, float aw, float ah, float bx, float bz, float bw, float bh)
{
    return fabsf(ax - bx) * 2 < (aw + bw) * HIT && fabsf(az - bz) * 2 < (ah + bh) * HIT;
}

static int ent_overlap(const ent *a, const ent *b)
{
    return overlap(a->x, a->z, K[a->type].w, K[a->type].h, b->x, b->z, K[b->type].w, K[b->type].h);
}

/* Inside the playfield with its sprite? */
static int inside(int type, float x, float z)
{
    float hw = K[type].w / 2, hh = K[type].h / 2;
    return x - hw >= ARENA_X0 && x + hw <= ARENA_X1 && z - hh >= ARENA_Z0 && z + hh <= ARENA_Z1;
}

static int count(const world *w, int type)
{
    int n = 0;
    for (int i = 0; i < w->n; i++) if (w->e[i].type == type) n++;
    return n;
}

static ent *add(world *w, int type, float x, float z)
{
    int i;
    for (i = 0; i < w->n; i++) if (w->e[i].type == T_NONE) break;
    if (i == w->n) {
        if (w->n >= MAX_ENTS) return NULL;
        w->n++;
    }
    ent *e = &w->e[i];
    memset(e, 0, sizeof *e);
    e->type = (uint8_t)type;
    e->x = x;
    e->z = z;
    e->target = -1;
    return e;
}

static void removeat(world *w, int i)
{
    w->e[i].type = T_NONE;
    while (w->n > 0 && w->e[w->n - 1].type == T_NONE) w->n--;
}

/* ---- score ---------------------------------------------------------------- */

static void add_score(world *w, int points)
{
    if (points <= 0) return;
    w->score += points;
    w->scored = 1;
    if (w->demo) return;
    while (w->extra_every > 0 && w->score >= w->next_extra) {
        w->lives++;
        w->next_extra += w->extra_every;
        snd_play(SND_EXTRA_MAN);
        fx_flash(mkrgb(255, 255, 255), 40);
    }
}

/* ---- placing things -------------------------------------------------------- */

static float frand_range(world *w, float a, float b) { return a + (b - a) * (float)rnd(w) / 255.0f; }

/* A random spot for `type`, outside the wave's safety box (`shrink` pulls the
 * box in, for electrodes) and off everything placed so far. */
static void place_random(world *w, ent *e, int safe, float shrink)
{
    float bx0, bx1, by0, by1;
    wave_safety_box(w->wave, &bx0, &bx1, &by0, &by1);
    bx0 += shrink * 2; bx1 -= shrink * 2; by0 += shrink * 1.3f; by1 -= shrink * 1.3f;
    float bz0 = LINE_TO_Z(by1), bz1 = LINE_TO_Z(by0);
    float hw = K[e->type].w / 2, hh = K[e->type].h / 2;
    for (int tries = 0; tries < 40; tries++) {
        e->x = frand_range(w, ARENA_X0 + hw, ARENA_X1 - hw);
        e->z = frand_range(w, ARENA_Z0 + hh, ARENA_Z1 - hh);
        if (safe && e->x + hw > bx0 && e->x - hw < bx1 && e->z + hh > bz0 && e->z - hh < bz1) continue;
        int clash = 0;
        if (tries < 30) {
            for (int i = 0; i < w->n && !clash; i++) {
                const ent *o = &w->e[i];
                if (o != e && o->type != T_NONE && ent_overlap(e, o)) clash = 1;
            }
        }
        if (!clash) return;
    }
}

static void init_grunt(world *w, ent *e) { e->timer = randu(w, w->p[P_ROBSPD]); }

static void init_human(world *w, ent *e)
{
    e->timer = 1 + (rnd(w) & 7);    /* staggered starts */
    e->timer2 = 0;
}

static void hulk_pick_target(world *w, ent *e)
{
    e->target = -1;
    if (rnd(w) <= 0xC0) {              /* ~75 %: a human, round-robin (GTARG) */
        for (int k = 0; k < w->n; k++) {
            int i = (w->hulk_rr + k) % (w->n ? w->n : 1);
            if (is_human(w->e[i].type)) {
                e->target = (int16_t)i;
                w->hulk_rr = i + 1;
                break;
            }
        }
    }
}

static void init_hulk(world *w, ent *e)
{
    hulk_pick_target(w, e);
    e->timer = w->p[P_HLKSPD];
    e->timer2 = 0;
    e->axis = (uint8_t)(rnd(w) & 1);
}

static void place_brain(world *w, ent *e)
{
    /* near an edge (GBCORD, RRB10.ASM:74-104) */
    place_random(w, e, 0, 0);
    if (rnd(w) & 0x80) {
        e->x = (rnd(w) & 0x80) ? ARENA_X0 + 8 + frand_range(w, 0, 24) : ARENA_X1 - 8 - frand_range(w, 0, 24);
    } else {
        e->z = (rnd(w) & 0x80) ? ARENA_Z0 + 9 + frand_range(w, 0, 24) : ARENA_Z1 - 9 - frand_range(w, 0, 24);
    }
    e->timer = 12;
    e->timer3 = rmax(w, w->p[P_BSHTIM]);
    e->state = 0;
}

static int quota(world *w) { return (rmax(w, w->p[P_ENFNUM]) + 1) / 2; }

static void place_spheroid(world *w, ent *e)
{
    e->x = (rnd(w) & 0x80) ? ARENA_X0 + 12 : ARENA_X1 - 12;
    e->z = frand_range(w, ARENA_Z0 + 8, ARENA_Z1 - 8);
    e->vx = e->vz = 0;
    e->state = 0;
    e->timer2 = 0;
    e->timer3 = rmax(w, w->p[P_CDPTIM]) * 10;
    e->count = quota(w);
}

static void place_quark(world *w, ent *e)
{
    e->x = frand_range(w, ARENA_X0 + 8, ARENA_X1 - 8);
    e->z = (rnd(w) & 0x80) ? ARENA_Z1 - 9 : ARENA_Z0 + 9;
    e->vx = e->vz = 0;
    e->state = 0;
    e->timer2 = 0;
    e->timer3 = rmax(w, w->p[P_TDPTIM]) * 15;
    e->count = quota(w);
}

static void init_tank(world *w, ent *e, int seeded)
{
    e->timer = 2;
    e->timer2 = 0;
    e->timer3 = seeded ? w->p[P_TNKSHT] + (rnd(w) & 31) : w->p[P_TNKSHT];
}

/* ---- the wave -------------------------------------------------------------- */

static void load_params(world *w)
{
    for (int p = 0; p < P_COUNT; p++) w->p[p] = wave_param(p, w->wave, w->difficulty, w->lives);
}

/* The "bozo" table: easier values for a beginner on waves 1-4 (RRG23.ASM:478-513). */
static void apply_bozo(world *w)
{
    if (w->wave > 4) return;
    if (!(w->lives == 1 || (w->wave <= 2 && w->lives < w->start_lives))) return;
    static const int P[4] = { P_CDPTIM, P_ENSTIM, P_ROBSPD, P_RMXSPD };
    for (int i = 0; i < 4; i++) w->p[P[i]] = wave_bozo(P[i], w->wave);
}

/* A turn begins: the player in the middle, the wave materializes. */
static void begin_turn(world *w)
{
    w->px = 152;
    w->pz = LINE_TO_Z(130);
    w->face_x = 0;
    w->face_z = 1;
    w->pheading = 0;
    w->rescued = 0;
    w->fire_count = 0;
    w->shells_fired = 0;
    w->grunt_clock = -10;
    w->exec_clock = 0;
    w->exec_passes = 0;
    w->scored = 0;
    memset(w->shots, 0, sizeof w->shots);

    w->transporter = count(w, T_BRAIN) > 0;
    int order = 0;
    for (int i = 0; i < w->n; i++) {
        ent *e = &w->e[i];
        if (e->type == T_NONE) continue;
        e->age = w->transporter ? -(rnd(w) % 100) : -order;
        order++;
    }
    /* the original: one object a frame, each taking a while, then the player */
    w->mat_len = w->transporter ? 150 : order + 70;
    w->ph = PH_MATERIALIZE;
    w->ph_time = 0;
    snd_play(SND_WAVE_START);
}

void world_new_game(world *w, int lives, int extra_man, int difficulty, unsigned seed)
{
    memset(w, 0, sizeof *w);
    w->wave = 1;
    w->lives = w->start_lives = lives;
    w->extra_every = extra_man;
    w->next_extra = extra_man;
    w->difficulty = difficulty;
    w->rng = seed * 2654435761u + 1u;
    if (!w->rng) w->rng = 1;
}

void world_start_wave(world *w)
{
    w->n = 0;
    load_params(w);
    apply_bozo(w);
    w->rmxspd_saved = w->p[P_RMXSPD];

    /* in the original's order: hulks, brains, humans, electrodes, grunts,
     * spheroids, quarks (PLSTRT, RRG23.ASM:104-208) */
    for (int i = 0; i < wave_count(w->wave, WC_HULK); i++) {
        ent *e = add(w, T_HULK, 0, 0);
        if (!e) break;
        place_random(w, e, 1, 0);
        init_hulk(w, e);
    }
    for (int i = 0; i < wave_count(w->wave, WC_BRAIN); i++) {
        ent *e = add(w, T_BRAIN, 0, 0);
        if (!e) break;
        place_brain(w, e);
    }
    static const int HT[3] = { T_MOMMY, T_DADDY, T_MIKEY };
    static const int HC[3] = { WC_MOMMY, WC_DADDY, WC_MIKEY };
    for (int k = 0; k < 3; k++) {
        for (int i = 0; i < wave_count(w->wave, HC[k]); i++) {
            ent *e = add(w, HT[k], 0, 0);
            if (!e) break;
            place_random(w, e, 0, 0);
            init_human(w, e);
        }
    }
    for (int i = 0; i < wave_count(w->wave, WC_ELECTRODE); i++) {
        ent *e = add(w, T_ELECTRODE, 0, 0);
        if (!e) break;
        place_random(w, e, 1, -3);
    }
    for (int i = 0; i < wave_count(w->wave, WC_GRUNT); i++) {
        ent *e = add(w, T_GRUNT, 0, 0);
        if (!e) break;
        place_random(w, e, 1, 0);
        init_grunt(w, e);
    }
    for (int i = 0; i < wave_count(w->wave, WC_SPHEROID); i++) {
        ent *e = add(w, T_SPHEROID, 0, 0);
        if (!e) break;
        place_spheroid(w, e);
    }
    for (int i = 0; i < wave_count(w->wave, WC_QUARK); i++) {
        ent *e = add(w, T_QUARK, 0, 0);
        if (!e) break;
        place_quark(w, e);
    }
    w->spheroids_saved = count(w, T_SPHEROID);
    begin_turn(w);
}

void world_resume(world *w)
{
    /* RRG23.ASM:828-893: progs, missiles, sparks and shells are lost; left
     * over enforcers come back as spheroids; everything else is placed anew */
    int enforcers = count(w, T_ENFORCER), spheroids = count(w, T_SPHEROID);
    int n = enforcers / 4;
    if (n == 0 && spheroids == 0 && enforcers > 0) n = 1;
    int target = spheroids + n;
    if (target > w->spheroids_saved) target = w->spheroids_saved;
    if (target < spheroids) target = spheroids;

    for (int i = 0; i < w->n; i++) {
        int t = w->e[i].type;
        if (t == T_PROG || t == T_CRUISE || t == T_SPARK || t == T_SHELL || t == T_ENFORCER) removeat(w, i);
    }
    for (int i = spheroids; i < target; i++) add(w, T_SPHEROID, 0, 0);

    /* the grunts keep the pace they had (RMXSPD as saved, ROBSPD no faster) */
    int robspd = w->p[P_ROBSPD];
    load_params(w);
    w->p[P_ROBSPD] = robspd;
    apply_bozo(w);
    w->p[P_RMXSPD] = w->rmxspd_saved;
    if (w->p[P_ROBSPD] < w->p[P_RMXSPD]) w->p[P_ROBSPD] = w->p[P_RMXSPD];

    for (int i = 0; i < w->n; i++) {
        ent *e = &w->e[i];
        switch (e->type) {
        case T_GRUNT:     place_random(w, e, 1, 0); init_grunt(w, e); break;
        case T_HULK:      place_random(w, e, 1, 0); init_hulk(w, e); break;
        case T_ELECTRODE: place_random(w, e, 1, -3); break;
        case T_BRAIN:     place_brain(w, e); break;
        case T_SPHEROID:  place_spheroid(w, e); break;
        case T_QUARK:     place_quark(w, e); break;
        case T_TANK:      place_random(w, e, 1, 0); init_tank(w, e, 1); break;
        case T_MOMMY: case T_DADDY: case T_MIKEY:
            e->state = 0;
            place_random(w, e, 0, 0);
            init_human(w, e);
            break;
        default: break;
        }
        e->vx = e->vz = 0;
    }
    begin_turn(w);
}

int world_robots_left(const world *w)
{
    int n = 0;
    for (int i = 0; i < w->n; i++) {
        switch (w->e[i].type) {
        case T_GRUNT: case T_SPHEROID: case T_ENFORCER: case T_BRAIN: case T_TANK: case T_QUARK: n++; break;
        default: break;
        }
    }
    return n;
}

/* (wave - 1) % 10 picks the shape (GTWCOL, RRG23.ASM:373-401); the 9th is
 * the 1st again. */
int wave_electrode_shape(int wave)
{
    static const int S[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 0, 8 };
    return S[((wave > 0 ? wave : 1) - 1) % 10];
}

/* ---- killing things --------------------------------------------------------- */

static rgb kill_color(int type)
{
    switch (type) {
    case T_GRUNT:     return mkrgb(255, 40, 40);
    case T_BRAIN:     return mkrgb(200, 60, 255);
    case T_PROG:      return mkrgb(255, 80, 255);
    case T_SPHEROID:  return mkrgb(255, 120, 40);
    case T_ENFORCER:  return mkrgb(40, 255, 120);
    case T_QUARK:     return mkrgb(120, 200, 255);
    case T_TANK:      return mkrgb(255, 160, 40);
    case T_ELECTRODE: return mkrgb(255, 200, 80);
    default:          return mkrgb(255, 255, 255);
    }
}

static void human_dies(world *w, int i)
{
    ent *h = &w->e[i];
    fx_skull(h->x, h->z);
    snd_play(SND_HUMAN_DIE);
    removeat(w, i);
}

/* A robot destroyed (by a shot: dx, dz its direction; or by an electrode). */
static void destroy(world *w, int i, float dx, float dz)
{
    ent *e = &w->e[i];
    int type = e->type;
    float x = e->x, z = e->z;
    add_score(w, K[type].points);
    int pieces = 14;
    float size = 12;
    switch (type) {
    case T_GRUNT:
        snd_play(SND_GRUNT_DIE);
        /* each grunt destroyed hurries the rest (RRP8.ASM:241-246) */
        {
            int nw = w->p[P_ROBSPD] * 0xE0 >> 8;
            if (nw >= w->p[P_RMXSPD]) w->p[P_ROBSPD] = nw;
        }
        break;
    case T_BRAIN:
        snd_play(SND_BRAIN_DIE);
        pieces = 20; size = 16;
        if (e->state == 1 && e->target >= 0 && is_human(w->e[e->target].type)) human_dies(w, e->target);
        break;
    case T_PROG:      snd_play(SND_GRUNT_DIE); break;
    case T_CRUISE:    snd_play(SND_SMALL_DIE); pieces = 4; size = 4; break;
    case T_SPARK:     snd_play(SND_SMALL_DIE); pieces = 4; size = 4; break;
    case T_SHELL:     snd_play(SND_SMALL_DIE); pieces = 4; size = 4; break;
    case T_SPHEROID:  snd_play(SND_SPHEROID_DIE); pieces = 22; fx_popup(x, z, 1000); break;
    case T_QUARK:     snd_play(SND_QUARK_DIE); pieces = 22; fx_popup(x, z, 1000); break;
    case T_ENFORCER:  snd_play(SND_ENFORCER_DIE); break;
    case T_TANK:      snd_play(SND_TANK_DIE); pieces = 20; break;
    case T_ELECTRODE: snd_play(SND_ELECTRODE_DIE); pieces = 8; size = 8; break;
    default: break;
    }
    fx_explode(x, 0, z, kill_color(type), dx, dz, pieces, size);
    removeat(w, i);
}

/* ---- the player ---------------------------------------------------------------- */

static void player_move(world *w, const controls *c)
{
    float mx = c->mx, mz = c->mz;
    float m = fmaxf(fabsf(mx), fabsf(mz));
    float len = sqrtf(mx * mx + mz * mz);
    if (len > 1) len = 1;
    if (m > 0.05f) {
        /* the original: 1 pixel and 1 line a frame, diagonals not shortened */
        float vx = mx / m * len, vz = mz / m * len;
        w->px += vx;
        w->pz += vz;
        if (w->px < ARENA_X0 + PLAYER_W / 2) w->px = ARENA_X0 + PLAYER_W / 2;
        if (w->px > ARENA_X1 - PLAYER_W / 2) w->px = ARENA_X1 - PLAYER_W / 2;
        if (w->pz < ARENA_Z0 + PLAYER_H / 2) w->pz = ARENA_Z0 + PLAYER_H / 2;
        if (w->pz > ARENA_Z1 - PLAYER_H / 2) w->pz = ARENA_Z1 - PLAYER_H / 2;
        w->pheading = atan2f(vx, vz);
        w->pwalk += 0.33f;
    }
}

static void player_fire(world *w, const controls *c)
{
    if (!c->fire) {
        w->fire_count = 0;
        return;
    }
    float dx = sgn(roundf(c->fx)), dz = sgn(roundf(c->fz));
    if (dx == 0 && dz == 0) { w->fire_count = 0; return; }
    if (dx != w->fire_dx || dz != w->fire_dz) {
        w->fire_count = 0;
        w->fire_dx = dx;
        w->fire_dz = dz;
    }
    w->fire_count++;
    /* first shot after 2 frames, then every 8 (LSPROC, RRG23.ASM:912-972) */
    int due = w->fire_count == 2 || (w->fire_count > 2 && w->fire_count % 8 == 0);
    if (!due) return;
    for (int i = 0; i < MAX_SHOTS; i++) {
        shot *s = &w->shots[i];
        if (s->live) continue;
        s->live = 1;
        s->dx = dx;
        s->dz = dz;
        s->x = w->px + dx * 4;
        s->z = w->pz + dz * 4;
        snd_play_ex(SND_SHOT, 0.7f, 1.0f);
        return;
    }
    w->fire_count--;        /* four out: try again next frame */
}

/* Which things a shot can hit, in the original's order: electrodes, the
 * robot list, the velocity objects. */
static int shot_group(int t)
{
    switch (t) {
    case T_ELECTRODE: return 0;
    case T_GRUNT: case T_HULK: case T_BRAIN: case T_PROG: case T_CRUISE: case T_TANK: return 1;
    case T_SPHEROID: case T_ENFORCER: case T_SPARK: case T_QUARK: case T_SHELL: return 2;
    default: return -1;
    }
}

static void hulk_knockback(world *w, int i, float dx, float dz)
{
    ent *e = &w->e[i];
    float kx = dx * ((srnd(w) >= 0) ? 4 : 2);
    float kz = dz * ((rnd(w) < 0xC0) ? 2 : 1);
    if (inside(T_HULK, e->x + kx, e->z)) e->x += kx;
    if (inside(T_HULK, e->x, e->z + kz)) e->z += kz;
    snd_play(SND_HULK_HIT);
}

static void shots_step(world *w)
{
    for (int s = 0; s < MAX_SHOTS; s++) {
        shot *sh = &w->shots[s];
        if (!sh->live) continue;
        /* 6 pixels / 6 lines a frame, checked in two halves */
        for (int half = 0; half < 2 && sh->live; half++) {
            sh->x += sh->dx * 3;
            sh->z += sh->dz * 3;
            if (sh->x < ARENA_X0 || sh->x > ARENA_X1 || sh->z < ARENA_Z0 || sh->z > ARENA_Z1) {
                sh->live = 0;
                break;
            }
            int hit = -1, best = 99;
            for (int i = 0; i < w->n; i++) {
                const ent *e = &w->e[i];
                int g = shot_group(e->type);
                if (g < 0 || g >= best || e->age < 0) continue;
                if (overlap(sh->x, sh->z, 4, 4, e->x, e->z, K[e->type].w, K[e->type].h)) { hit = i; best = g; }
            }
            if (hit < 0) continue;
            sh->live = 0;
            if (w->e[hit].type == T_HULK) hulk_knockback(w, hit, sh->dx, sh->dz);
            else destroy(w, hit, sh->dx, sh->dz);
        }
    }
}

/* ---- the robots --------------------------------------------------------------- */

static void face(ent *e, float dx, float dz)
{
    if (dx != 0 || dz != 0) e->heading = atan2f(dx, dz);
}

static void grunt_steps(world *w)
{
    /* one process for every grunt: a pass every 4 frames (RRP8.ASM:154-227) */
    if (++w->grunt_clock < 4) return;
    w->grunt_clock = 0;
    int stepped = 0;
    for (int i = 0; i < w->n; i++) {
        ent *e = &w->e[i];
        if (e->type != T_GRUNT) continue;
        if (--e->timer > 0) continue;
        e->timer = randu(w, w->p[P_ROBSPD]);
        float dz = w->pz - e->z, mz = 0, mx;
        if (dz >= 2) mz = 4;
        else if (dz <= -2) mz = -4;
        mx = (e->x - w->px >= 2) ? -4 : 4;      /* the original steps right when level */
        if (inside(T_GRUNT, e->x, e->z + mz)) e->z += mz;
        if (inside(T_GRUNT, e->x + mx, e->z)) e->x += mx;
        e->walk += 1.5707963f;
        face(e, w->px - e->x, w->pz - e->z);
        stepped = 1;
        /* a grunt walking into an electrode: both go, 100 points */
        for (int k = 0; k < w->n; k++) {
            ent *o = &w->e[k];
            if (o->type == T_ELECTRODE && ent_overlap(e, o)) {
                destroy(w, k, 0, 0);
                destroy(w, i, 0, 0);
                break;
            }
        }
    }
    if (stepped) snd_play_ex(SND_GRUNT_STEP, 0.35f, 1.0f);
}

static void hulk_choose(world *w, ent *e)
{
    /* HULKND, RRH11.ASM:142-187: turn to the other axis, toward the target */
    float tx = w->px, tz = w->pz;
    if (e->target >= 0) {
        const ent *t = &w->e[e->target];
        if (is_human(t->type)) { tx = t->x; tz = t->z; }
        else e->target = -1;
    }
    e->timer2 = (rnd(w) & 31) + 1;
    if (e->axis == 0) {
        e->axis = 1;
        float dest = tz + (float)(srnd(w) >> 3);
        e->ax = 0;
        e->az = dest > e->z ? 1 : -1;
    } else {
        e->axis = 0;
        float dest = tx + (float)(srnd(w) >> 3) * 2;
        e->az = 0;
        e->ax = dest > e->x ? 1 : -1;
    }
}

static void hulk_step(world *w, int i)
{
    ent *e = &w->e[i];
    if (--e->timer > 0) return;
    e->timer = w->p[P_HLKSPD];
    if (--e->timer2 <= 0) hulk_choose(w, e);
    for (int tries = 0; tries < 2; tries++) {
        float sx = e->ax * ((e->alt & 1) ? 4 : 3), sz = e->az * 2;
        if (inside(T_HULK, e->x + sx, e->z + sz)) {
            e->x += sx;
            e->z += sz;
            e->alt++;
            break;
        }
        hulk_choose(w, e);
    }
    e->walk += 1.5707963f;
    face(e, e->ax, e->az);
    /* hulks crush electrodes and humans (RRH11.ASM:64-71) */
    for (int k = 0; k < w->n; k++) {
        ent *o = &w->e[k];
        if (o->type == T_ELECTRODE && ent_overlap(e, o)) destroy(w, k, e->ax, e->az);
        else if (is_human(o->type) && o->state == 0 && ent_overlap(e, o)) human_dies(w, k);
    }
}

static void fire_cruise(world *w, ent *b)
{
    if (count(w, T_CRUISE) >= 8) return;
    ent *m = add(w, T_CRUISE, b->x, b->z);
    if (!m) return;
    m->timer = 2;
    m->timer2 = 0;
    m->age = 0;
    snd_play_ex(SND_CRUISE, 0.8f, 1.0f);
}

static void brain_step(world *w, int i)
{
    ent *e = &w->e[i];
    if (e->state == 1) {
        /* reprogramming a human: 20 flashes of 4 frames (RRB10.ASM:242-340) */
        ent *h = e->target >= 0 ? &w->e[e->target] : NULL;
        if (!h || !is_human(h->type)) { e->state = 0; e->timer = w->p[P_BRNSPD]; return; }
        if (--e->timer > 0) return;
        e->timer = 4;
        snd_play_ex(SND_REPROGRAM, 0.6f, 1.0f + 0.03f * (20 - e->count));
        if (--e->count > 0) return;
        h->type = T_PROG;
        h->state = 0;
        h->timer = 3;
        h->age = 0;
        h->axis = (uint8_t)(rnd(w) & 1);
        h->tx = (float)srnd(w) / 128.0f * 56;
        h->tz = (float)srnd(w) / 128.0f * 17;
        snd_play(SND_PROG_MAKE);
        e->state = 0;
        e->target = -1;
        e->timer = w->p[P_BRNSPD];
        return;
    }
    if (--e->timer > 0) return;
    e->timer = w->p[P_BRNSPD];

    /* the nearest human by |dx| + |dy|, else the player (GETHTG) */
    float tx = w->px, tz = w->pz, best = 1e9f;
    int target = -1;
    for (int k = 0; k < w->n; k++) {
        const ent *h = &w->e[k];
        if (!is_human(h->type) || h->state != 0) continue;
        float d = fabsf(h->x - e->x) + fabsf(h->z - e->z);
        if (d < best) { best = d; target = k; tx = h->x; tz = h->z; }
    }
    float dx = tx - e->x, dz = tz - e->z;
    float sx = fabsf(dx) > 4 ? sgn(dx) * 2 : 0;
    float sz = dz >= 0 ? 1 : -1;
    if (inside(T_BRAIN, e->x + sx, e->z)) e->x += sx;
    if (inside(T_BRAIN, e->x, e->z + sz)) e->z += sz;
    e->walk += 0.8f;
    face(e, dx, dz);

    if (target >= 0 && fabsf(tx - e->x) <= 6 && fabsf(tz - e->z) <= 3) {
        e->state = 1;
        e->target = (int16_t)target;
        e->count = 20;
        e->timer = 4;
        w->e[target].state = 1;
        return;
    }
    if (--e->timer3 <= 0) {
        e->timer3 = rmax(w, w->p[P_BSHTIM]);
        fire_cruise(w, e);
    }
}

static void prog_aim(world *w, ent *e)
{
    if (rnd(w) > 0xF8) {
        e->tx = (float)srnd(w) / 128.0f * 56;
        e->tz = (float)srnd(w) / 128.0f * 17;
    }
    e->axis = (uint8_t)((srnd(w) < 0) ? 1 : 0);
}

static void prog_step(world *w, int i)
{
    ent *e = &w->e[i];
    if (--e->timer > 0) return;
    e->timer = 3;
    float gx = w->px + e->tx, gz = w->pz + e->tz;
    if (rnd(w) > 0xE4) prog_aim(w, e);
    for (int tries = 0; tries < 2; tries++) {
        float sx = 0, sz = 0;
        if (e->axis == 0) sx = fabsf(gx - e->x) >= 2 ? sgn(gx - e->x) * 4 : 0;
        else              sz = fabsf(gz - e->z) >= 2 ? sgn(gz - e->z) * 4 : 0;
        if ((sx != 0 || sz != 0) && inside(T_PROG, e->x + sx, e->z + sz)) {
            e->x += sx;
            e->z += sz;
            e->vx = sx / 3;
            e->vz = sz / 3;
            face(e, sx, sz);
            break;
        }
        e->axis ^= 1;
    }
    e->walk += 1.2f;
}

static void cruise_step(world *w, int i)
{
    ent *e = &w->e[i];
    e->spin += 0.3f;
    if (--e->timer > 0) return;
    e->timer = 2;
    if (--e->timer2 <= 0) {
        /* GCMDIR, RRB10.ASM:672-698 */
        e->timer2 = rmax(w, 7);
        if (srnd(w) < 0) {
            float gx = w->px + (float)((rnd(w) & 15) - 6) * 2;
            e->ax = sgn(gx - e->x);
            if (srnd(w) >= 0) e->az = sgn(w->pz + (float)((rnd(w) & 15) - 6) - e->z);
            else e->az = 0;
        } else {
            e->ax = 0;
            e->az = sgn(w->pz + (float)((rnd(w) & 15) - 6) - e->z);
        }
    }
    for (int k = 0; k < 2; k++) {
        float nx = e->x + e->ax * 2, nz = e->z + e->az;
        if (!inside(T_CRUISE, nx, e->z)) { e->ax = -e->ax; nx = e->x; }
        if (!inside(T_CRUISE, e->x, nz)) { e->az = -e->az; nz = e->z; }
        e->x = nx;
        e->z = nz;
    }
    e->vx = e->ax * 2;
    e->vz = e->az;
}

/* Velocity objects slide along the walls: a component that would leave the
 * field is not applied (RRS22.ASM:1638-1653). */
static void slide(ent *e)
{
    if (inside(e->type, e->x + e->vx, e->z)) e->x += e->vx;
    if (inside(e->type, e->x, e->z + e->vz)) e->z += e->vz;
}

static void spawn_enforcer(world *w, ent *s)
{
    ent *e = add(w, T_ENFORCER, s->x, s->z);
    if (!e) return;
    e->timer = 3;
    e->timer2 = 0;
    e->timer3 = rmax(w, w->p[P_ENSTIM]);
    e->age = 0;
    snd_play(SND_ENFORCER_BIRTH);
}

static void spheroid_step(world *w, int i)
{
    ent *e = &w->e[i];
    e->spin += 0.15f;
    if (e->state == 2) {
        /* all dropped: off to a side, gone (RRC11.ASM:106-129) */
        e->x += e->vx;
        if (e->x < ARENA_X0 + 6 || e->x > ARENA_X1 - 20) removeat(w, i);
        return;
    }
    slide(e);
    if ((w->ph_time & 1) == 0) {
        if (--e->timer2 <= 0) {
            e->ax = (float)((rnd(w) & 0x1F) - 16) / 256.0f * 2;
            e->az = (float)(((rnd(w) ^ rnd(w)) & 0x3F) - 32) / 256.0f;
            e->timer2 = randu(w, 15);
        }
        e->vx += e->ax;
        e->vz += e->az;
        if (e->vx > 2) e->vx = 2;
        if (e->vx < -2) e->vx = -2;
        if (e->vz > 2) e->vz = 2;
        if (e->vz < -2) e->vz = -2;
        e->vx -= e->vx / 64;
        e->vz -= e->vz / 128;
    }
    if (--e->timer3 > 0) return;
    int between = w->p[P_CDPTIM] / 4;
    if (between < 1) between = 1;
    if (e->state == 0) {
        e->state = 1;
        e->timer3 = rmax(w, between) * 16;
        return;
    }
    e->timer3 = rmax(w, between) * 16;
    if (count(w, T_ENFORCER) < 8) {
        spawn_enforcer(w, e);
        e = &w->e[i];
        if (--e->count <= 0) {
            e->state = 2;
            e->vx = (srnd(w) < 0) ? -2 : 2;
        }
    }
}

static void spawn_spark(world *w, ent *en)
{
    if (count(w, T_SPARK) >= 20) return;
    ent *s = add(w, T_SPARK, en->x, en->z);
    if (!s) return;
    float ox = (float)(srnd(w) >> 3) * 2, oz = (float)(srnd(w) >> 3);
    if (w->px < ARENA_X0 + 32) ox = 0;
    s->vx = 4 * (w->px + ox - s->x) / 256;
    s->vz = 4 * (w->pz + oz - s->z) / 256;
    s->ax = (float)((rnd(w) & 0x1F) - 16) / 256.0f * 2;
    s->az = (float)((rnd(w) & 0x1F) - 16) / 256.0f;
    s->count = ((rnd(w) & 15) + 20) * 4;
    s->age = 0;
    snd_play_ex(SND_ENFORCER_SHOT, 0.7f, 1.0f);
}

static void enforcer_step(world *w, int i)
{
    ent *e = &w->e[i];
    e->spin += 0.12f;
    slide(e);
    if (e->age < 40) return;        /* growing (RRC11.ASM:275-281) */
    if (--e->timer > 0) return;
    e->timer = 3;
    if (--e->timer2 <= 0) {
        int t = rnd(w) & 0x1F;
        e->timer2 = t ? t : 256;
        float tx = w->px + (float)(srnd(w) >> 3) * 2, tz = w->pz + (float)(srnd(w) >> 3);
        e->vx = 2 * (tx - e->x) / 256;
        e->vz = 2 * (tz - e->z) / 256;
    }
    if (--e->timer3 <= 0) {
        e->timer3 = rmax(w, w->p[P_ENSTIM]);
        spawn_spark(w, e);
    }
}

static void spark_step(world *w, int i)
{
    ent *e = &w->e[i];
    e->spin += 0.4f;
    slide(e);
    if (e->age % 4 == 0) {
        e->vx += e->ax;
        e->vz += e->az;
    }
    if (--e->count <= 0) removeat(w, i);
}

static void spawn_tank(world *w, ent *q)
{
    if (count(w, T_TANK) >= 20) return;
    ent *t = add(w, T_TANK, q->x, q->z);
    if (!t) return;
    init_tank(w, t, 0);
    t->age = 0;
    snd_play(SND_TANK_BIRTH);
}

static void quark_step(world *w, int i)
{
    ent *e = &w->e[i];
    e->spin += 0.25f;
    if (e->state == 2) {
        e->z += e->vz;
        if (e->z < ARENA_Z0 + 8 || e->z > ARENA_Z1 - 8) removeat(w, i);
        return;
    }
    slide(e);
    if (w->ph_time % 3 == 0 && --e->timer2 <= 0) {
        e->timer2 = (rnd(w) & 31) + 1;
        float sx = (float)rmax(w, w->p[P_SQSPD]) * 4 / 256 * 2;
        float sz = (float)rmax(w, w->p[P_SQSPD]) * 8 / 256;
        if (e->x < ARENA_X0 + 30) e->vx = sx;
        else if (e->x > ARENA_X1 - 30) e->vx = -sx;
        else e->vx = (srnd(w) < 0) ? -sx : sx;
        if (e->z < ARENA_Z0 + 30) e->vz = sz;
        else if (e->z > ARENA_Z1 - 30) e->vz = -sz;
        else e->vz = (srnd(w) < 0) ? -sz : sz;
    }
    if (--e->timer3 > 0) return;
    int between = w->p[P_TDPTIM] / 2 + 1;
    e->timer3 = rmax(w, between) * 3;
    if (e->state == 0) { e->state = 1; return; }
    if (count(w, T_TANK) < 20) {
        spawn_tank(w, e);
        e = &w->e[i];
        if (--e->count <= 0) {
            e->state = 2;
            e->vz = e->z > ARENA_CZ ? 2 : -2;
        }
    }
}

static void fire_shell(world *w, ent *t)
{
    ent *s = add(w, T_SHELL, t->x, t->z);
    if (!s) return;
    float spd = (float)w->p[P_SHLSPD];
    if (rnd(w) >= 0x80) {
        /* a rebound shot: off a wall, back at the player (RRTK4.ASM:459-552) */
        float gx, gz;
        if (srnd(w) < 0) {
            int right = w->px > ARENA_CX;
            if ((rnd(w) & 7) == 0) right = !right;
            gx = right ? ARENA_X1 - 4 : ARENA_X0 + 4;
            gz = (t->z + w->pz) / 2;
        } else {
            int top = w->pz > ARENA_CZ;
            if ((rnd(w) & 7) == 0) top = !top;
            gz = top ? ARENA_Z1 - 4 : ARENA_Z0 + 4;
            gx = (t->x + w->px) / 2;
        }
        float vx = (gx - t->x) / 256 * 2 / 2, vz = (gz - t->z) / 256;
        float T = spd / 256;
        for (int k = 0; k < 12 && (vx != 0 || vz != 0); k++) {
            if (fabsf(vx) >= T * 2 || fabsf(vz) >= 2 * T) break;
            vx *= 2;
            vz *= 2;
        }
        s->vx = vx;
        s->vz = vz;
    } else {
        float ox = (float)(srnd(w) >> 3) * 2, oz = (float)(srnd(w) >> 3);
        if (w->px < ARENA_X0 + 32) ox = 0;
        s->vx = (w->px + ox - t->x) * spd / 8192;
        s->vz = (w->pz + oz - t->z) * spd / 8192;
    }
    s->count = ((rnd(w) & 31) + 48) * 2;
    s->age = 0;
    snd_play_ex(SND_TANK_SHOT, 0.8f, 1.0f);
}

static void tank_step(world *w, int i)
{
    ent *e = &w->e[i];
    if (e->age < 48) return;        /* growing (RRTK4.ASM:216) */
    if (--e->timer > 0) return;
    e->timer = 2;
    int blocked = !inside(T_TANK, e->x + e->ax, e->z + e->az);
    if (--e->timer2 <= 0 || blocked) {
        float tx, tz;
        if (rnd(w) <= 0x60) { tx = w->px; tz = w->pz; }
        else { tx = frand_range(w, ARENA_X0, ARENA_X1); tz = frand_range(w, ARENA_Z0, ARENA_Z1); }
        e->timer2 = (rnd(w) & 31) + 1;
        e->ax = tx > e->x ? 1 : -1;
        e->az = fabsf(tz - e->z) < 16 ? 0 : sgn(tz - e->z);
    }
    if (inside(T_TANK, e->x + e->ax, e->z + e->az)) {
        e->x += e->ax;
        e->z += e->az;
    }
    face(e, e->ax, e->az);
    e->spin = atan2f(w->px - e->x, w->pz - e->z) - e->heading;
    if (--e->timer3 <= 0) {
        e->timer3 = w->p[P_TNKSHT];
        /* the original's counter only falls when a shell is shot: after 21
         * shells a turn, tanks stop firing (RRTK4.ASM:393-396) */
        if (w->shells_fired <= 20) {
            w->shells_fired++;
            fire_shell(w, e);
        }
    }
}

static void shell_step(world *w, int i)
{
    ent *e = &w->e[i];
    e->spin += 0.3f;
    float nx = e->x + e->vx, nz = e->z + e->vz;
    int bounce = 0;
    if (!inside(T_SHELL, nx, e->z)) { e->vx = -e->vx; bounce = 1; } else e->x = nx;
    if (!inside(T_SHELL, e->x, nz)) { e->vz = -e->vz; bounce = 1; } else e->z = nz;
    if (bounce) snd_play_ex(SND_SHELL_BOUNCE, 0.5f, 1.0f);
    if (--e->count <= 0) removeat(w, i);
}

/* Humans wander at random: a step every 8 frames (RRH11.ASM:264-557). */
static void human_step(world *w, int i)
{
    static const int DX[8] = { -1, 1, 0, 0, -1, 1, 1, -1 };
    static const int DZ[8] = { 0, 0, -1, 1, 1, 1, -1, -1 };    /* L R D U UL UR DR DL */
    ent *e = &w->e[i];
    if (e->state == 1) {           /* in a brain's grip: shaking */
        e->spin += 1;
        return;
    }
    if (--e->timer > 0) return;
    e->timer = 8;
    for (int tries = 0; tries < 3; tries++) {
        if (e->timer2 <= 0) {
            int d = rnd(w) & 7;
            e->ax = (float)DX[d];
            e->az = (float)DZ[d];
            e->timer2 = (rnd(w) & 0x7F) + 1;
        }
        float sx = e->ax * ((e->alt & 1) ? 1 : 2), sz = e->az;
        int blocked = !inside(e->type, e->x + sx, e->z + sz);
        if (!blocked) {
            ent probe = *e;
            probe.x += sx;
            probe.z += sz;
            for (int k = 0; k < w->n && !blocked; k++)
                if (w->e[k].type == T_ELECTRODE && ent_overlap(&probe, &w->e[k])) blocked = 1;
        }
        if (blocked) { e->timer2 = 0; continue; }
        e->x += sx;
        e->z += sz;
        e->alt++;
        e->timer2--;
        e->walk += 1.0f;
        face(e, sx, sz);
        break;
    }
}

/* ---- the player meets things -------------------------------------------------- */

static int deadly(int t)
{
    return t != T_NONE && !is_human(t);
}

static void player_collide(world *w)
{
    for (int i = 0; i < w->n; i++) {
        ent *e = &w->e[i];
        if (e->type == T_NONE) continue;
        if (!overlap(w->px, w->pz, PLAYER_W, PLAYER_H, e->x, e->z, K[e->type].w, K[e->type].h)) continue;
        if (deadly(e->type)) {
            /* what kills the player survives (PCFLG) */
            w->ph = PH_DYING;
            w->ph_time = 0;
            snd_play(SND_PLAYER_DIE);
            fx_flash(mkrgb(255, 255, 255), 60);
            return;
        }
    }
    for (int i = 0; i < w->n; i++) {
        ent *e = &w->e[i];
        if (!is_human(e->type)) continue;
        if (!overlap(w->px, w->pz, PLAYER_W, PLAYER_H, e->x, e->z, K[e->type].w, K[e->type].h)) continue;
        /* the rescue: 1000, 2000 ... 5000 (RRH11.ASM:453-470) */
        if (w->rescued < 5) w->rescued++;
        add_score(w, 1000 * w->rescued);
        fx_popup(e->x, e->z, 1000 * w->rescued);
        snd_play(SND_RESCUE);
        removeat(w, i);
    }
}

/* The game exec, every 15 frames: the grunts hurry, the wave ends
 * (RRG23.ASM:403-458). */
static void game_exec(world *w)
{
    if (++w->exec_clock < 15) return;
    w->exec_clock = 0;
    w->exec_passes++;
    int p = w->exec_passes;
    if ((p == 18 || (p > 18 && (p - 18) % 15 == 0)) && count(w, T_GRUNT) < 30) {
        if (w->scored) { w->p[P_RMXSPD] -= 1; w->p[P_ROBSPD] -= 2; }
        else           { w->p[P_RMXSPD] -= 2; w->p[P_ROBSPD] -= 4; }   /* "bone him for stalling" */
        if (w->p[P_RMXSPD] < 1) w->p[P_RMXSPD] = 1;
        if (w->p[P_ROBSPD] < w->p[P_RMXSPD]) w->p[P_ROBSPD] = w->p[P_RMXSPD];
        w->scored = 0;
    }
    if (world_robots_left(w) == 0) {
        w->ph = PH_CLEARED;
        w->ph_time = 0;
        fx_flash(mkrgb(255, 255, 255), 30);
    }
}

void world_step(world *w, const controls *c)
{
    w->ph_time++;
    switch (w->ph) {
    case PH_MATERIALIZE:
        for (int i = 0; i < w->n; i++) w->e[i].age++;
        if (w->ph_time >= w->mat_len) {
            w->ph = PH_PLAY;
            w->ph_time = 0;
            for (int i = 0; i < w->n; i++) if (w->e[i].age < 0) w->e[i].age = 0;
        }
        return;
    case PH_DYING:
        if (w->ph_time >= 150) {
            w->lives--;
            w->ph = w->lives > 0 ? PH_RESTART : PH_OVER;
            w->ph_time = 0;
        }
        return;
    case PH_PLAY:
        break;
    default:
        return;
    }

    player_move(w, c);
    player_fire(w, c);
    shots_step(w);
    grunt_steps(w);
    for (int i = 0; i < w->n; i++) {
        ent *e = &w->e[i];
        if (e->type == T_NONE) continue;
        e->age++;
        switch (e->type) {
        case T_HULK:     hulk_step(w, i); break;
        case T_BRAIN:    brain_step(w, i); break;
        case T_PROG:     prog_step(w, i); break;
        case T_CRUISE:   cruise_step(w, i); break;
        case T_SPHEROID: spheroid_step(w, i); break;
        case T_ENFORCER: enforcer_step(w, i); break;
        case T_SPARK:    spark_step(w, i); break;
        case T_QUARK:    quark_step(w, i); break;
        case T_TANK:     tank_step(w, i); break;
        case T_SHELL:    shell_step(w, i); break;
        case T_MOMMY: case T_DADDY: case T_MIKEY: human_step(w, i); break;
        default: break;
        }
    }
    player_collide(w);
    if (w->ph == PH_PLAY) game_exec(w);
}
