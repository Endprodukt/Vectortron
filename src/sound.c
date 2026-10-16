/* sound.c — the game's sounds, made in code (or loaded, when a file is there). */
#include "sound.h"

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
};

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
    case SND_SMALL_DIE:      return mk_noise_boom(0.12f, 10000, 3000, 1800);
    default:                 return NULL;
    }
}

void snd_init(vp_audio *a)
{
    A = a;
    if (!a) return;
    for (int i = 0; i < SND_COUNT; i++) {
        char rel[128], path[1024];
        snprintf(rel, sizeof rel, "assets/sounds/%s.wav", NAMES[i]);
        if (vp_sound_load(a, vp_asset_path(rel, path, sizeof path), &S[i]) == VP_OK) continue;
        snprintf(rel, sizeof rel, "assets/sounds/%s.ogg", NAMES[i]);
        if (vp_sound_load(a, vp_asset_path(rel, path, sizeof path), &S[i]) == VP_OK) continue;
        S[i] = make((snd_id)i);
    }
}

void snd_play_ex(snd_id id, float volume, float pitch)
{
    if (A && id < SND_COUNT && S[id]) vp_sound_play(A, S[id], volume, pitch, 0);
}

void snd_play(snd_id id) { snd_play_ex(id, 1.0f, 1.0f); }

vp_voice snd_loop(snd_id id, float volume)
{
    if (!A || id >= SND_COUNT || !S[id]) return 0;
    return vp_sound_play(A, S[id], volume, 1.0f, 1);
}

void snd_stop(vp_voice v) { vp_voice_stop(A, v, 0.05f); }

void snd_stop_all(void) { vp_audio_stop_all(A, 0.1f); }
