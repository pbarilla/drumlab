/* SPDX-License-Identifier: GPL-3.0-only */
/* Kits: the 32 archetypes (arch.h, ours), the preset kits (recipes over the archetypes), and a generator that rolls a new kit.
 * A rolled kit is never random noise: every sound is one of our archetypes of its column, moved a little (pitch, decay, noise, grit) inside
 * limits that keep it musical; the A and B patches move together so the morph stays sensible. The first four sounds (the ones a
 * pattern reaches first) are always a kick, a snare, a hat and a bell / tom / cowbell. A seed makes the kit: the same seed, the same kit. */
#include "arch.h"

static uint32_t gen_rng = 0x2545F491u;
static uint32_t gen_rand(void) { gen_rng ^= gen_rng << 13; gen_rng ^= gen_rng >> 17; gen_rng ^= gen_rng << 5; return gen_rng; }
static int32_t gen_span(int32_t d) { return d ? (int32_t)(gen_rand() % (2u * (uint32_t)d + 1u)) - d : 0; }
static uint16_t gclamp(int32_t v, int32_t lo, int32_t hi) { return (uint16_t)(v < lo ? lo : v > hi ? hi : v); }

/* how far each parameter may move, in 16 bit units (a thousandth of the range is a thousand times the frequency or time, so 6575 is an octave) */
static const uint16_t GJ[DR_PARAMS] = {0, 1800, 0, 2400, 0, 2200, 2600, 0, 2200, 2200, 0, 0, 2400, 2800, 2600, 1400, 1200, 400, 0, 0, 0};
#define OCT_U 6575

/* sound s from archetype a of its column; jit: how far it may wander (0 = exactly the archetype); the kit's own character on top */
static void gen_sound(uint32_t s, uint32_t a, uint32_t jit, const int8_t *tune, int32_t decay_u, int32_t dist, int32_t noise)
{
    uint32_t c = s & 3u, i, q;
    int32_t d[DR_PARAMS];
    for (i = 0; i < DR_PARAMS; i++) d[i] = jit ? gen_span(GJ[i]) : 0;
    for (q = 0; q < 2u; q++)
        for (i = 0; i < DR_PARAMS; i++) {
            int32_t v = arch_patch[c][a][q][i] + d[i];
            if (i == 1u || i == 8u || i == 15u) v += *tune * OCT_U / 100;       /* the kit's tuning moves every frequency */
            if (i == 3u || i == 12u) v += decay_u;
            if (i == 13u) v += noise * 655;
            if (i == 14u) { v += dist * 655; v = v > 50000 ? 50000 : v; }          /* never past about three quarters of the grit */
            if (i == 0u || i == 4u || i == 7u || i == 10u) v = arch_patch[c][a][q][i];             /* the switches stay as designed */
            kp[s][q][i] = gclamp(v, i == 17u ? 30000 : 0, 65535);
        }
    snm[s] = (uint8_t)(1u + c * 8u + a);
    knob[s][0] = PITCH_MID; knob[s][1] = 0;
}
static int32_t gen_decay_units(uint32_t pct) { return pct == 100u ? 0 : (int32_t)(((int64_t)((int32_t)log2fx(pct * 1000u) - (int32_t)log2fx(100u * 1000u)) * OCT_U) >> 24); }

static void kit_lab1(void)                                  /* the first kit: LAB-1 */
{
    uint32_t s, i;
    for (s = 0; s < NS; s++) { for (i = 0; i < DR_PARAMS; i++) { kp[s][0][i] = kit_patch[s][0][i]; kp[s][1][i] = kit_patch[s][1][i]; } snm[s] = 0; knob[s][0] = PITCH_MID; knob[s][1] = 0; }
}
static void kit_preset(uint32_t k)                          /* a recipe: exactly its archetypes, tuned */
{
    const kit_def_t *kd = &kit_def[k];
    int8_t tune = kd->tune;
    uint32_t s;
    for (s = 0; s < NS; s++) gen_sound(s, kd->pick[s & 3u][s >> 2], 0, &tune, gen_decay_units((uint32_t)kd->decay), kd->dist, kd->noise);
}

/* a rolled kit: which archetypes may open each column (the first row is the kit's heart: a kick, a snare, a hat, something tuned) */
static const uint8_t HEART[4][5] = {{0, 1, 2, 3, 4}, {0, 1, 2, 5, 6}, {0, 1, 3, 5, 7}, {0, 1, 2, 3, 7}};
static void kit_roll(uint32_t seed)
{
    uint32_t c, r, s, taken, n;
    int8_t tune;
    int32_t decay, dist, noise;
    gen_rng = seed ? seed * 2654435761u + 0x9E3779B9u : 0x2545F491u;
    gen_rand(); gen_rand();
    tune = (int8_t)(gen_span(25) + 10);                      /* -15 .. +35 hundredths of an octave */
    decay = 75 + (int32_t)(gen_rand() % 56u);                  /* 75 .. 130 % */
    dist = (int32_t)(gen_rand() % 15u); noise = gen_span(5) + 2;
    for (c = 0; c < 4u; c++) {
        taken = 0;
        for (r = 0; r < 4u; r++) {
            uint32_t a, tries = 0;
            do {
                a = r == 0u ? HEART[c][gen_rand() % 5u] : gen_rand() % 8u;
                tries++;
            } while ((taken >> a & 1u) && tries < 40u);
            taken |= 1u << a;
            gen_sound(c + 4u * r, a, 1, &tune, gen_decay_units((uint32_t)decay), dist, noise);
        }
    }
    (void)s; (void)n;
}

/* a name for a rolled kit, made from the seed: an adjective and a noun, both short */
static const char *const KADJ[16] = {"SALTY", "MINTY", "WOBBLY", "CRISPY", "FUZZY", "TINY", "GIANT", "SLEEPY", "SPICY", "DUSTY", "SHINY", "BOUNCY", "CHEEKY", "MELLOW", "ZIPPY", "COSY"};
static const char *const KNOUN[16] = {"OTTER", "PARROT", "TOASTER", "KETTLE", "COMET", "PICKLE", "WALRUS", "ROBOT", "BISCUIT", "LLAMA", "ROCKET", "MUFFIN", "BADGER", "WAFFLE", "GECKO", "TURNIP"};
static void kit_name_of(uint32_t seed, char *out)
{
    uint32_t h = seed * 2246822519u + 3266489917u;
    const char *a = KADJ[h >> 28], *n = KNOUN[(h >> 24) & 15u];
    while (*a) *out++ = *a++;
    *out++ = ' ';
    while (*n) *out++ = *n++;
    *out = 0;
}

/* what a sound is called */
static const char *snd_name(uint32_t s) { return snm[s] ? arch_name[(snm[s] - 1u) >> 3][(snm[s] - 1u) & 7u] : kit_name[s]; }
