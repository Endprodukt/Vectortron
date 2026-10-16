/* sound.c — the game's sounds, made in code (or loaded, when a file is there). */
#include "sound.h"
#include "romzip.h"
#include "williams.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RATE 22050
#define TAU  6.2831853f

static vp_audio *A;
static vp_sound *S[SND_COUNT];

static const char *const NAMES[SND_COUNT] = {
    "shot", "grunt_die", "electrode_die", "hulk_hit", "rescue", "human_die",
    "prog_make", "brain_die", "cruise", "spheroid_die", "enforcer_birth",
    "enforcer_shot", "enforcer_die", "quark_die", "tank_birth", "tank_shot",
    "shell_bounce", "tank_die", "player_die", "wave_start", "wave_end",
    "extra_man", "coin", "start", "grunt_step", "reprogram", "small_die",
    "spark_die", "shell_die", "prog_die", "transport", "start2",
};

/* ---- the original sounds: the sound tables of the game's source ----------
 * {priority, steps of (repeat, frames, sound number)}; RRG23, RRS22, RRP8,
 * RRH11, RRB10, RRC11, RRTK4 and RRT2.ASM. A priority of 0: no sound. */
typedef struct { int pri, n; wms_step s[3]; } orig;

static const orig ORIG[SND_COUNT] = {
    [SND_SHOT]           = { 0xD0, 1, { {1, 8, 0x01} } },                        /* LASSND */
    [SND_GRUNT_DIE]      = { 0xD0, 2, { {1, 12, 0x14}, {1, 8, 0x17} } },         /* RBSND */
    [SND_ELECTRODE_DIE]  = { 0xD0, 1, { {1, 8, 0x17} } },                        /* PSKSND */
    [SND_HULK_HIT]       = { 0xD0, 1, { {1, 16, 0x06} } },                       /* HKHSND */
    [SND_RESCUE]         = { 0xE0, 1, { {1, 32, 0x0D} } },                       /* SAVSND */
    [SND_HUMAN_DIE]      = { 0xE0, 1, { {1, 24, 0x1A} } },                       /* HKSND */
    [SND_PROG_MAKE]      = { 0xD8, 1, { {1, 8, 0x11} } },                        /* HPSND */
    [SND_BRAIN_DIE]      = { 0xD0, 2, { {1, 4, 0x14}, {1, 8, 0x11} } },          /* BKSND */
    [SND_CRUISE]         = { 0xC8, 2, { {1, 8, 0x15}, {1, 8, 0x14} } },          /* BSHSND */
    [SND_SPHEROID_DIE]   = { 0xD1, 1, { {1, 8, 0x08} } },                        /* CRKSND */
    [SND_ENFORCER_BIRTH] = { 0xD8, 1, { {1, 8, 0x18} } },                        /* ENDSND */
    [SND_ENFORCER_SHOT]  = { 0xD0, 1, { {1, 8, 0x1D} } },                        /* ENFSND */
    [SND_ENFORCER_DIE]   = { 0xD0, 1, { {3, 4, 0x17} } },                        /* ENKSND */
    [SND_QUARK_DIE]      = { 0xD0, 2, { {1, 4, 0x15}, {1, 8, 0x11} } },          /* SQKSND */
    [SND_TANK_BIRTH]     = { 0xD0, 1, { {1, 8, 0x19} } },                        /* TKDSND */
    [SND_TANK_SHOT]      = { 0xC8, 1, { {1, 8, 0x04} } },                        /* TKFSND */
    [SND_SHELL_BOUNCE]   = { 0xC8, 2, { {1, 4, 0x14}, {1, 1, 0x13} } },          /* SRBSND */
    [SND_TANK_DIE]       = { 0xD0, 1, { {1, 8, 0x11} } },                        /* TNKSND */
    [SND_PLAYER_DIE]     = { 0xEE, 2, { {2, 8, 0x11}, {1, 32, 0x17} } },         /* PDSND */
    [SND_WAVE_START]     = { 0, 0, { {0, 0, 0} } },                              /* silent in the original */
    [SND_WAVE_END]       = { 0xE0, 1, { {29, 4, 0x0E} } },                       /* WVSND */
    [SND_EXTRA_MAN]      = { 0xEF, 1, { {1, 32, 0x1E} } },                       /* RPSND */
    [SND_COIN]           = { 0xFF, 1, { {1, 32, 0x0C} } },                       /* CNSND (one of 8) */
    [SND_START]          = { 0xF0, 1, { {1, 16, 0x28} } },                       /* ST1SND */
    [SND_GRUNT_STEP]     = { 0xC0, 1, { {1, 10, 0x06} } },                       /* RMVSND */
    [SND_REPROGRAM]      = { 0xD0, 1, { {2, 3, 0x12} } },                        /* PRGSND */
    [SND_SMALL_DIE]      = { 0xD0, 1, { {2, 4, 0x17} } },                        /* CMKSND */
    [SND_SPARK_DIE]      = { 0xD0, 2, { {1, 4, 0x15}, {1, 8, 0x14} } },          /* SPKSND */
    [SND_SHELL_DIE]      = { 0xD0, 3, { {1, 3, 0x01}, {1, 4, 0x15}, {1, 4, 0x13} } }, /* SHKSND */
    [SND_PROG_DIE]       = { 0xD0, 2, { {1, 4, 0x14}, {2, 4, 0x17} } },          /* PGKSND */
    /* the transporter: $13, then $12 every frame 72 times, every 2 frames
     * 36 times (RRT2.ASM:440-452) */
    [SND_TRANSPORT]      = { 0xFF, 3, { {1, 1, 0x13}, {72, 1, 0x12}, {36, 2, 0x12} } },
    [SND_START2]         = { 0xF0, 1, { {1, 16, 0x25} } },                       /* ST2SND */
};

/* the other seven coin sounds (CNSND picks one of eight at random) */
static const wms_step COINS[8] = { {1, 32, 0x0C}, {1, 32, 0x20}, {3, 16, 0x24}, {1, 32, 0x27},
                                   {1, 32, 0x2D}, {2, 16, 0x35}, {1, 32, 0x3A}, {1, 32, 0x3E} };
static vp_sound *COIN[8];

static int      rom_sounds;        /* 1: the sounds came from the ROM */
static int      cur_pri, pri_frames;
static vp_voice cur_voice;
static unsigned coin_rng = 1;

/* ---- the synthesizer: a few oscillators, a Williams-style noise ---------- */

typedef struct {
    float *buf;
    int    n;
    float  phase, phase2;
    unsigned lfsr;
    float  hold, holdPhase;
} synth;

static void syn_begin(synth *s, float seconds)
{
    s->n = (int)(seconds * RATE);
    s->buf = calloc((size_t)s->n, sizeof(float));
    s->phase = s->phase2 = 0;
    s->lfsr = 0xACE1u;
    s->hold = 0;
    s->holdPhase = 0;
}

static float sq(float ph) { return ph - floorf(ph) < 0.5f ? 1.0f : -1.0f; }
static float saw(float ph) { return 2.0f * (ph - floorf(ph)) - 1.0f; }
static float tri(float ph) { float f = ph - floorf(ph); return f < 0.5f ? 4 * f - 1 : 3 - 4 * f; }

/* Noise sampled and held at `rate` Hz: low rates rumble, high ones hiss, as
 * on the Williams board's noise generator. */
static float noise_at(synth *s, float rate)
{
    s->holdPhase += rate / RATE;
    while (s->holdPhase >= 1.0f) {
        s->holdPhase -= 1.0f;
        unsigned bit = ((s->lfsr >> 0) ^ (s->lfsr >> 2) ^ (s->lfsr >> 3) ^ (s->lfsr >> 5)) & 1u;
        s->lfsr = (s->lfsr >> 1) | (bit << 15);
        s->hold = (s->lfsr & 1u) ? 1.0f : -1.0f;
    }
    return s->hold;
}

static float expsweep(float f0, float f1, float u)
{
    if (u < 0) u = 0;
    if (u > 1) u = 1;
    return f0 * powf(f1 / f0, u);
}

static vp_sound *syn_end(synth *s, float gain)
{
    vp_sound *out = NULL;
    /* a few ms of fade in and out: no clicks */
    int edge = RATE / 400;
    for (int i = 0; i < edge && i < s->n; i++) {
        float k = (float)i / edge;
        s->buf[i] *= k;
        s->buf[s->n - 1 - i] *= k;
    }
    for (int i = 0; i < s->n; i++) {
        float v = s->buf[i] * gain;
        s->buf[i] = v > 1 ? 1 : v < -1 ? -1 : v;
    }
    vp_sound_from_samples(A, s->buf, s->n, 1, RATE, &out);
    free(s->buf);
    return out;
}

/* ---- the sounds ---------------------------------------------------------- */

static vp_sound *mk_shot(void)
{
    /* the laser: a quick square chirp falling from high */
    synth s; syn_begin(&s, 0.11f);
    for (int i = 0; i < s.n; i++) {
        float t = (float)i / RATE, u = t / 0.11f;
        s.phase += expsweep(2600, 700, u) / RATE;
        s.buf[i] = 0.35f * sq(s.phase) * (1 - u);
    }
    return syn_end(&s, 1.0f);
}

static vp_sound *mk_noise_boom(float seconds, float r0, float r1, float tone)
{
    synth s; syn_begin(&s, seconds);
    for (int i = 0; i < s.n; i++) {
        float t = (float)i / RATE, u = t / seconds;
        float env = expf(-4.0f * u) * (1 - u);
        float v = noise_at(&s, expsweep(r0, r1, u));
        if (tone > 0) {
            s.phase += expsweep(tone, tone * 0.3f, u) / RATE;
            v = 0.7f * v + 0.3f * sq(s.phase);
        }
        s.buf[i] = 0.6f * v * env;
    }
    return syn_end(&s, 1.0f);
}

static vp_sound *mk_hulk_hit(void)
{
    /* a low thump */
    synth s; syn_begin(&s, 0.12f);
    for (int i = 0; i < s.n; i++) {
        float t = (float)i / RATE, u = t / 0.12f;
        s.phase += expsweep(180, 60, u) / RATE;
        s.buf[i] = 0.7f * tri(s.phase) * (1 - u) + 0.15f * noise_at(&s, 900) * (1 - u) * (1 - u);
    }
    return syn_end(&s, 1.0f);
}

static vp_sound *mk_rescue(void)
{
    /* a rising run of square notes: "bloo-bloo-bloop" */
    synth s; syn_begin(&s, 0.42f);
    static const float notes[] = { 523, 659, 784, 1047, 1319, 1568 };
    for (int i = 0; i < s.n; i++) {
        float t = (float)i / RATE;
        int k = (int)(t / 0.07f);
        if (k > 5) k = 5;
        float local = t - k * 0.07f;
        s.phase += notes[k] * (1.0f + 0.4f * local / 0.07f) / RATE;
        s.buf[i] = 0.32f * sq(s.phase) * (k == 5 ? 1 - local / 0.07f : 1.0f);
    }
    return syn_end(&s, 1.0f);
}

static vp_sound *mk_human_die(void)
{
    /* a falling, wobbling cry */
    synth s; syn_begin(&s, 0.6f);
    for (int i = 0; i < s.n; i++) {
        float t = (float)i / RATE, u = t / 0.6f;
        float f = expsweep(900, 140, u) * (1 + 0.08f * sinf(TAU * 18 * t));
        s.phase += f / RATE;
        s.buf[i] = 0.4f * saw(s.phase) * (1 - u);
    }
    return syn_end(&s, 1.0f);
}

static vp_sound *mk_prog(void)
{
    /* a weird descending warble */
    synth s; syn_begin(&s, 0.7f);
    for (int i = 0; i < s.n; i++) {
        float t = (float)i / RATE, u = t / 0.7f;
        float f = expsweep(1400, 200, u) * (1 + 0.25f * sq(t * 30));
        s.phase += f / RATE;
        s.buf[i] = 0.3f * sq(s.phase) * (1 - u * 0.7f);
    }
    return syn_end(&s, 1.0f);
}

static vp_sound *mk_cruise(void)
{
    /* a little vibrato beep that climbs */
    synth s; syn_begin(&s, 0.25f);
    for (int i = 0; i < s.n; i++) {
        float t = (float)i / RATE, u = t / 0.25f;
        float f = expsweep(400, 1200, u) * (1 + 0.15f * sinf(TAU * 40 * t));
        s.phase += f / RATE;
        s.buf[i] = 0.28f * tri(s.phase) * (1 - u);
    }
    return syn_end(&s, 1.0f);
}

static vp_sound *mk_enforcer_birth(void)
{
    /* a rising "zwip" */
    synth s; syn_begin(&s, 0.22f);
    for (int i = 0; i < s.n; i++) {
        float t = (float)i / RATE, u = t / 0.22f;
        s.phase += expsweep(300, 2200, u) / RATE;
        s.buf[i] = 0.3f * saw(s.phase) * (1 - u);
    }
    return syn_end(&s, 1.0f);
}

static vp_sound *mk_spark(void)
{
    /* a short high zap */
    synth s; syn_begin(&s, 0.09f);
    for (int i = 0; i < s.n; i++) {
        float t = (float)i / RATE, u = t / 0.09f;
        s.phase += expsweep(3200, 1500, u) / RATE;
        s.buf[i] = 0.22f * sq(s.phase) * (1 - u) + 0.1f * noise_at(&s, 8000) * (1 - u);
    }
    return syn_end(&s, 1.0f);
}

static vp_sound *mk_tank_shot(void)
{
    synth s; syn_begin(&s, 0.18f);
    for (int i = 0; i < s.n; i++) {
        float t = (float)i / RATE, u = t / 0.18f;
        s.phase += expsweep(500, 120, u) / RATE;
        s.buf[i] = 0.45f * sq(s.phase) * (1 - u) * (1 - u);
    }
    return syn_end(&s, 1.0f);
}

static vp_sound *mk_bounce(void)
{
    synth s; syn_begin(&s, 0.05f);
    for (int i = 0; i < s.n; i++) {
        float t = (float)i / RATE, u = t / 0.05f;
        s.phase += 880.0f / RATE;
        s.buf[i] = 0.2f * tri(s.phase) * (1 - u);
    }
    return syn_end(&s, 1.0f);
}

static vp_sound *mk_tank_birth(void)
{
    synth s; syn_begin(&s, 0.3f);
    for (int i = 0; i < s.n; i++) {
        float t = (float)i / RATE, u = t / 0.3f;
        s.phase += expsweep(120, 600, u) / RATE;
        s.buf[i] = 0.35f * sq(s.phase) * (1 - u);
    }
    return syn_end(&s, 1.0f);
}

static vp_sound *mk_player_die(void)
{
    /* the long one: a warbling sweep down into noise */
    synth s; syn_begin(&s, 1.8f);
    for (int i = 0; i < s.n; i++) {
        float t = (float)i / RATE, u = t / 1.8f;
        float f = expsweep(1600, 60, u) * (1 + 0.3f * sq(t * 22));
        s.phase += f / RATE;
        s.phase2 += f * 1.5f / RATE;
        float tone = 0.5f * sq(s.phase) + 0.25f * saw(s.phase2);
        float nz = noise_at(&s, expsweep(9000, 400, u));
        s.buf[i] = (0.35f * tone * (1 - u) + 0.35f * nz * u) * (1 - u * u);
    }
    return syn_end(&s, 1.0f);
}

static vp_sound *mk_wave_start(void)
{
    /* the materialize: a rising sweep with a flutter */
    synth s; syn_begin(&s, 1.1f);
    for (int i = 0; i < s.n; i++) {
        float t = (float)i / RATE, u = t / 1.1f;
        float f = expsweep(80, 1800, u) * (1 + 0.1f * sinf(TAU * 25 * t));
        s.phase += f / RATE;
        s.phase2 += f * 1.01f / RATE;
        s.buf[i] = 0.22f * (sq(s.phase) + saw(s.phase2)) * (0.3f + 0.7f * u) * (1 - u * u * u);
    }
    return syn_end(&s, 1.0f);
}

static vp_sound *mk_wave_end(void)
{
    synth s; syn_begin(&s, 0.9f);
    for (int i = 0; i < s.n; i++) {
        float t = (float)i / RATE, u = t / 0.9f;
        float f = expsweep(1800, 100, u);
        s.phase += f / RATE;
        s.buf[i] = 0.25f * sq(s.phase) * sq(t * 16) * (1 - u);
    }
    return syn_end(&s, 1.0f);
}

static vp_sound *mk_extra_man(void)
{
    /* a bright repeated chime */
    synth s; syn_begin(&s, 0.9f);
    for (int i = 0; i < s.n; i++) {
        float t = (float)i / RATE;
        float local = fmodf(t, 0.15f);
        float f = (int)(t / 0.15f) % 2 ? 1568 : 2093;
        s.phase += f / RATE;
        s.buf[i] = 0.28f * sq(s.phase) * (1 - local / 0.15f);
    }
    return syn_end(&s, 1.0f);
}

static vp_sound *mk_coin(void)
{
    synth s; syn_begin(&s, 0.3f);
    for (int i = 0; i < s.n; i++) {
        float t = (float)i / RATE, u = t / 0.3f;
        s.phase += (t < 0.06f ? 988.0f : 1319.0f) / RATE;
        s.buf[i] = 0.3f * sq(s.phase) * (1 - u);
    }
    return syn_end(&s, 1.0f);
}

static vp_sound *mk_start(void)
{
    synth s; syn_begin(&s, 0.5f);
    for (int i = 0; i < s.n; i++) {
        float t = (float)i / RATE, u = t / 0.5f;
        s.phase += expsweep(200, 1000, u) / RATE;
        s.buf[i] = 0.3f * saw(s.phase) * (1 - u);
    }
    return syn_end(&s, 1.0f);
}

static vp_sound *mk_step(void)
{
    /* a short low click: the march */
    synth s; syn_begin(&s, 0.04f);
    for (int i = 0; i < s.n; i++) {
        float t = (float)i / RATE, u = t / 0.04f;
        s.phase += expsweep(220, 90, u) / RATE;
        s.buf[i] = 0.5f * sq(s.phase) * (1 - u);
    }
    return syn_end(&s, 1.0f);
}

static vp_sound *mk_reprogram(void)
{
    synth s; syn_begin(&s, 0.05f);
    for (int i = 0; i < s.n; i++) {
        float t = (float)i / RATE, u = t / 0.05f;
        s.phase += expsweep(700, 1400, u) / RATE;
        s.buf[i] = 0.25f * sq(s.phase) * (1 - u);
    }
    return syn_end(&s, 1.0f);
}

static vp_sound *make(snd_id id)
{
    switch (id) {
    case SND_SHOT:           return mk_shot();
    case SND_GRUNT_DIE:      return mk_noise_boom(0.32f, 6000, 700, 0);
    case SND_ELECTRODE_DIE:  return mk_noise_boom(0.28f, 12000, 3000, 2400);
    case SND_HULK_HIT:       return mk_hulk_hit();
    case SND_RESCUE:         return mk_rescue();
    case SND_HUMAN_DIE:      return mk_human_die();
    case SND_PROG_MAKE:      return mk_prog();
    case SND_BRAIN_DIE:      return mk_noise_boom(0.5f, 5000, 300, 600);
    case SND_CRUISE:         return mk_cruise();
    case SND_SPHEROID_DIE:   return mk_noise_boom(0.45f, 9000, 600, 1500);
    case SND_ENFORCER_BIRTH: return mk_enforcer_birth();
    case SND_ENFORCER_SHOT:  return mk_spark();
    case SND_ENFORCER_DIE:   return mk_noise_boom(0.3f, 9000, 1200, 900);
    case SND_QUARK_DIE:      return mk_noise_boom(0.5f, 7000, 400, 2000);
    case SND_TANK_BIRTH:     return mk_tank_birth();
    case SND_TANK_SHOT:      return mk_tank_shot();
    case SND_SHELL_BOUNCE:   return mk_bounce();
    case SND_TANK_DIE:       return mk_noise_boom(0.55f, 4000, 200, 300);
    case SND_PLAYER_DIE:     return mk_player_die();
    case SND_WAVE_START:     return mk_wave_start();
    case SND_WAVE_END:       return mk_wave_end();
    case SND_EXTRA_MAN:      return mk_extra_man();
    case SND_COIN:           return mk_coin();
    case SND_START:          return mk_start();
    case SND_GRUNT_STEP:     return mk_step();
    case SND_REPROGRAM:      return mk_reprogram();
    case SND_SMALL_DIE:
    case SND_SPARK_DIE:
    case SND_SHELL_DIE:      return mk_noise_boom(0.12f, 10000, 3000, 1800);
    case SND_PROG_DIE:       return mk_noise_boom(0.32f, 6000, 700, 0);
    case SND_TRANSPORT:      return mk_wave_start();
    case SND_START2:         return mk_start();
    default:                 return NULL;
    }
}

/* ---- finding the sound ROM -------------------------------------------------- */

/* The sound ROM's names in MAME's Robotron sets, newest first. */
static const char *const ROM_NAMES[] = { "video_sound_rom_1.ic12", "robotron.snd", "vsr1.ic12", NULL };

static int rom_ok(const unsigned char *r, size_t n)
{
    if (!r || n != 4096) return 0;
    unsigned reset = r[n - 2] << 8 | r[n - 1];
    return reset >= 0xF000;
}

static unsigned char *try_rom(const char *path, size_t *n)
{
    unsigned char *r = romzip_load(path, ROM_NAMES, n);
    if (r && !rom_ok(r, *n)) { free(r); r = NULL; }
    if (r) printf("vectortron: original sounds from %s\n", path);
    return r;
}

/* VT_SOUND_ROM, then next to the game, then AdvanceMAME's ROM folders (its
 * dir_rom, and the usual places on a cabinet). */
static unsigned char *find_rom(size_t *n)
{
    char path[1024];
    unsigned char *r;
    const char *env = getenv("VT_SOUND_ROM");
    if (env && (r = try_rom(env, n))) return r;
    static const char *const LOCAL[] = { "assets/robotron.zip", "assets/robotron.snd",
                                         "assets/video_sound_rom_1.ic12", NULL };
    for (int i = 0; LOCAL[i]; i++)
        if ((r = try_rom(vp_asset_path(LOCAL[i], path, sizeof path), n))) return r;

    char dirs[2048] = "";
    vp_rc *rc = NULL;
    if (vp_rc_load(NULL, &rc) == VP_OK) {
        const char *d = vp_rc_get(rc, "dir_rom");
        if (d) snprintf(dirs, sizeof dirs, "%s", d);
        vp_rc_free(rc);
    }
    const char *home = getenv("HOME");
    size_t len = strlen(dirs);
    snprintf(dirs + len, sizeof dirs - len, "%s/usr/local/share/advance/rom:%s/.advance/rom:/home/pi/.advance/rom",
             len ? ":" : "", home ? home : "/root");
    for (char *tok = strtok(dirs, ":;"); tok; tok = strtok(NULL, ":;")) {
        snprintf(path, sizeof path, "%s/robotron.zip", tok);
        if ((r = try_rom(path, n))) return r;
    }
    return NULL;
}

static vp_sound *record(const unsigned char *rom, size_t n, const wms_step *steps, int count)
{
    int frames = 0;
    vp_sound *out = NULL;
    float *buf = wms_record(rom, n, steps, count, RATE, 4.0f, &frames);
    if (!buf) return NULL;
    vp_sound_from_samples(A, buf, frames, 1, RATE, &out);
    free(buf);
    return out;
}

void snd_init(vp_audio *a)
{
    A = a;
    if (!a) return;
    size_t rn = 0;
    unsigned char *rom = getenv("VT_SYNTH_SOUNDS") ? NULL : find_rom(&rn);
    rom_sounds = rom != NULL;
    for (int i = 0; i < SND_COUNT; i++) {
        char rel[128], path[1024];
        snprintf(rel, sizeof rel, "assets/sounds/%s.wav", NAMES[i]);
        if (vp_sound_load(a, vp_asset_path(rel, path, sizeof path), &S[i]) == VP_OK) continue;
        snprintf(rel, sizeof rel, "assets/sounds/%s.ogg", NAMES[i]);
        if (vp_sound_load(a, vp_asset_path(rel, path, sizeof path), &S[i]) == VP_OK) continue;
        if (rom) {
            if (ORIG[i].n) S[i] = record(rom, rn, ORIG[i].s, ORIG[i].n);
            continue;
        }
        S[i] = make((snd_id)i);
    }
    if (rom) {
        for (int i = 0; i < 8; i++) COIN[i] = record(rom, rn, &COINS[i], 1);
        free(rom);
    } else {
        printf("vectortron: no Robotron sound ROM found, using sounds made in code\n");
    }
}

int snd_original(void) { return rom_sounds; }

const char *snd_name(snd_id id) { return id < SND_COUNT ? NAMES[id] : "?"; }

int snd_original_steps(snd_id id, const wms_step **steps)
{
    if (id >= SND_COUNT) return 0;
    *steps = ORIG[id].s;
    return ORIG[id].n;
}

unsigned char *snd_find_rom(size_t *size) { return find_rom(size); }

void snd_tick(void)
{
    if (pri_frames > 0 && --pri_frames == 0) cur_pri = 0;
}

static int seq_frames(const orig *o)
{
    int f = 0;
    for (int i = 0; i < o->n; i++) f += o->s[i].repeat * o->s[i].frames;
    return f;
}

void snd_play_ex(snd_id id, float volume, float pitch)
{
    if (!A || id >= SND_COUNT) return;
    if (!rom_sounds) {
        if (S[id]) vp_sound_play(A, S[id], volume, pitch, 0);
        return;
    }
    /* the board has one voice: a sound cuts in only on one of the same
     * priority or lower, while that one's sequence runs (SNDLDV) */
    const orig *o = &ORIG[id];
    if (!o->n || o->pri < cur_pri) return;
    vp_sound *snd = S[id];
    if (id == SND_COIN) {
        coin_rng = coin_rng * 1103515245u + 12345u;
        vp_sound *c = COIN[(coin_rng >> 16) & 7];
        if (c) snd = c;
    }
    if (!snd) return;
    if (cur_voice) vp_voice_stop(A, cur_voice, 0.004f);
    cur_voice = vp_sound_play(A, snd, 1.0f, 1.0f, 0);
    cur_pri = o->pri;
    pri_frames = seq_frames(o);
}

void snd_play(snd_id id) { snd_play_ex(id, 1.0f, 1.0f); }

vp_voice snd_loop(snd_id id, float volume)
{
    if (!A || id >= SND_COUNT || !S[id]) return 0;
    return vp_sound_play(A, S[id], volume, 1.0f, 1);
}

void snd_stop(vp_voice v) { vp_voice_stop(A, v, 0.05f); }

void snd_stop_all(void) { vp_audio_stop_all(A, 0.1f); }
