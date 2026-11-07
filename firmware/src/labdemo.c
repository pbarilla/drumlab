/* SPDX-License-Identifier: GPL-3.0-only */
/* The demo projects (written into empty slots on a fresh device and after a factory reset) and the lessons (built-in: a ghost pattern to
 * play in, in a scratch copy of the project that is put back when you leave). Data: kits/demos.py -> demos.h. */
#include "demos.h"

static void demo_kit(uint32_t k) { if (k == 0u) kit_lab1(); else kit_preset(k - 1u); }
static void demo_pat(uint32_t p, const dpat_t *d)
{
    uint32_t l, s;
    memset(&pat[p], 0, sizeof pat[p]);
    for (l = 0; l < 4u; l++)
        for (s = 0; s < 16u; s++) { pat[p].trig[l][s] = d->trig[l][s]; pat[p].lock[l][s][0] = PITCH_MID; pat[p].lock[l][s][1] = 0; }
    pat[p].acc = d->acc;
}
static void demo_load(const demo_t *d)
{
    uint32_t i;
    core_init(); demo_kit(d->kit);
    swing = d->swing;
    for (i = 0; i < d->npat; i++) demo_pat(i, &d->pat[i]);
    chain_n = d->nchain;
    for (i = 0; i < d->nchain; i++) chain[i] = d->chain[i];
    tempo_set(d->bpm);
}
/* THE BLUETOOTH DEMO: a locked project to play with all three brothers (D minor, 124 BPM): pattern 1 the groove, pattern 2 the lift. Kicks on the lane 0, so the sidechain pumps. */
static void bt_demo_load(void)
{
    static const uint8_t K[2][16] = {{1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}, {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0x11, 0}};     /* lane 0: kick (the second pattern adds a kick 2) */
    static const uint8_t S[2][16] = {{0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0}, {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1}};          /* lane 1: snare (and a fill) */
    static const uint8_t H[2][16] = {{0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0}, {0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 0x11, 0}};      /* lane 2: hats, the lift has more and an open one */
    static const uint8_t B[2][16] = {{0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x21}};
    uint32_t p, s;
    core_init(); kit_lab1();
    for (p = 0; p < 2u; p++) {
        memset(&pat[p], 0, sizeof pat[p]);
        for (s = 0; s < 16u; s++) {
            pat[p].trig[0][s] = K[p][s]; pat[p].trig[1][s] = S[p][s]; pat[p].trig[2][s] = H[p][s]; pat[p].trig[3][s] = B[p][s];
            pat[p].lock[0][s][0] = pat[p].lock[1][s][0] = pat[p].lock[2][s][0] = pat[p].lock[3][s][0] = PITCH_MID;
        }
        pat[p].acc = 0x0101u;
    }
    chain_n = 0; cur_pat = 0; swing = 0; tempo_set(124);
    bt_locked = 1; lab_dirty = 0;
}
static void demo_install(void)                                      /* the demos into the empty slots */
{
    uint32_t i;
    for (i = 0; i < NDEMO && i < LAB_SLOTS; i++) if (!lab_exists(i)) { demo_load(&demos[i]); lab_save(i); }
    core_init(); lab_dirty = 0;
}

/* ---- lessons: a scratch copy of the project, a ghost to play in */
static lpat_t les_pat[NPAT];
static uint16_t les_kp[NS][2][DR_PARAMS];
static uint8_t les_knob[NS][2], les_snm[NS], les_chain[CHAIN_MAX], les_chain_n, les_bpm, les_swing, les_cur;
static uint8_t les_on, les_i, les_done;
static void les_enter(uint32_t i)
{
    const lesson_t *l = &lessons[i];
    memcpy(les_pat, pat, sizeof pat); memcpy(les_kp, kp, sizeof kp); memcpy(les_knob, knob, sizeof knob); memcpy(les_snm, snm, sizeof snm);
    memcpy(les_chain, chain, sizeof chain); les_chain_n = chain_n; les_bpm = bpm; les_swing = swing; les_cur = cur_pat;
    demo_kit(l->kit);
    memset(&pat[0], 0, sizeof pat[0]);
    chain_n = 0; cur_pat = 0; swing = 0; tempo_set(l->bpm);
    les_on = 1; les_i = (uint8_t)i; les_done = 0;
}
static void les_exit(void)
{
    memcpy(pat, les_pat, sizeof pat); memcpy(kp, les_kp, sizeof kp); memcpy(knob, les_knob, sizeof knob); memcpy(snm, les_snm, sizeof snm);
    memcpy(chain, les_chain, sizeof chain); chain_n = les_chain_n; swing = les_swing; cur_pat = les_cur; tempo_set(les_bpm);
    les_on = 0;
}
static uint32_t les_have(uint32_t l, uint32_t s)                    /* 1: the ghost's hit is there (right sound), 0: not yet; 2: no ghost here */
{
    uint32_t g = lessons[les_i].pat.trig[l][s], t = pat[0].trig[l][s];
    if (!(g & 15u)) return 2;
    return (t & 15u) && ((t >> 4) & 3u) == ((g >> 4) & 3u);
}
static void les_count(uint32_t *have, uint32_t *total)
{
    uint32_t l, s, h = 0, n = 0;
    for (l = 0; l < 4u; l++) for (s = 0; s < 16u; s++) { uint32_t r = les_have(l, s); if (r != 2u) { n++; h += r; } }
    *have = h; *total = n;
}
