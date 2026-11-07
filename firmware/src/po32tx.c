/* SPDX-License-Identifier: GPL-3.0-only */
#include "po32tx.h"
#include <stdint.h>

#define SYM_STEP 117057150u          /* 1201.923 baud / 44100 Hz, Q32 */
#define CAR_STEP 351171450u          /* three carrier cycles a symbol, Q32 */

static const uint8_t pre[4] = { 0x14, 0x19, 0x9D, 0xCF };

static uint16_t crc_mix(uint16_t s, uint8_t x)
{
    return (uint16_t)((((uint32_t)s << 8) | x) ^ ((uint32_t)x << 12) ^ ((uint32_t)x << 5));
}
static uint16_t crc_upd(uint16_t s, uint8_t raw)
{
    uint8_t x = (uint8_t)(raw ^ (s >> 8));
    x ^= (uint8_t)(x >> 4);
    return crc_mix(s, x);
}
static void put(po32tx_t *t, uint8_t raw)
{
    t->buf[t->len++] = (uint8_t)(raw + (t->state & 0xFF));
    t->state = crc_upd(t->state, raw);
}
static void trailer(po32tx_t *t)
{
    uint16_t s = t->state;
    put(t, (uint8_t)s);
    put(t, (uint8_t)(s >> 8));
}

void po32tx_begin(po32tx_t *t)
{
    uint32_t i;
    t->len = 0; t->state = 0x1D0F; t->on = 0;
    for (i = 0; i < 124; i++) t->buf[t->len++] = 0x55;
    for (i = 0; i < 4; i++) t->buf[t->len++] = pre[i];
}

int po32tx_patch(po32tx_t *t, int slot, int right, const uint16_t *p)
{
    int i;
    if (t->len + 48 + 5 > PO32TX_MAX) return 0;
    put(t, 0xB2); put(t, 0x37); put(t, 43);
    put(t, (uint8_t)(((right ? 2 : 1) << 4) | (slot & 15)));
    for (i = 0; i < 21; i++) { put(t, (uint8_t)p[i]); put(t, (uint8_t)(p[i] >> 8)); }
    trailer(t);
    return 1;
}

int po32tx_pattern(po32tx_t *t, int num, const uint8_t trig[4][16], const uint8_t lock[4][16][2], uint32_t accent)
{
    int l, s, i;
    if (t->len + 216 + 5 > PO32TX_MAX) return 0;
    put(t, 0x22); put(t, 0xD0); put(t, 211);
    put(t, (uint8_t)num);
    for (l = 0; l < 4; l++) {
        for (s = 0; s < 16; s++) put(t, trig[l][s]);
        for (s = 0; s < 16; s++) {                                      /* a step's pitch and morph; libpo's default for a played step is 80 01, and a morph of 0 is not sent */
            uint8_t a = lock[l][s][0], m = lock[l][s][1];
            if ((trig[l][s] & 15u) && !m) m = 1;
            put(t, a); put(t, m);
        }
    }
    for (i = 0; i < 16; i++) put(t, 0);
    put(t, (uint8_t)accent); put(t, (uint8_t)(accent >> 8));
    trailer(t);
    return 1;
}

int po32tx_state(po32tx_t *t, const uint8_t knob[16][2], int tempo, int swing12, const uint8_t *chain, int n)
{
    int i;
    if (n > 64 || t->len + 35 + n + 5 + 5 > PO32TX_MAX) return 0;
    put(t, 0x5A); put(t, 0x50); put(t, (uint8_t)(35 + n));
    for (i = 0; i < 16; i++) { put(t, knob[i][0]); put(t, knob[i][1] ? knob[i][1] : 1u); }          /* (libpo's default pair is 80 01: no 0 morph) */
    put(t, (uint8_t)tempo); put(t, (uint8_t)swing12); put(t, (uint8_t)n);
    for (i = 0; i < n; i++) put(t, chain[i]);
    trailer(t);
    return 1;
}

int po32tx_finish(po32tx_t *t)
{
    uint16_t s;
    uint8_t x;
    if (t->len + 5 > PO32TX_MAX) return 0;
    put(t, 0xC3); put(t, 0x71);
    s = t->state;
    t->buf[t->len++] = (uint8_t)s;
    x = (uint8_t)((s >> 12) ^ (s >> 8));
    t->state = crc_mix(s, x);
    trailer(t);
    t->bits = t->len * 8u;
    return (int)t->len;
}

void po32tx_play(po32tx_t *t)
{
    t->bit = 0; t->sym = 0xFFFFFFFFu; t->ph = 0; t->flip = 0; t->on = 1;
}

/* one carrier phase flip for every 0 bit; a 1 keeps the phase */
int po32tx_render(po32tx_t *t, int32_t *out, int n, int amp)
{
    int k;
    for (k = 0; k < n; k++) {
        int32_t s;
        if (!t->on) { out[k] = 0; continue; }
        t->sym += SYM_STEP;
        if (t->sym < SYM_STEP) {
            uint32_t b = t->bit < t->bits ? (t->buf[t->bit >> 3] >> (t->bit & 7)) & 1u : 0u;
            t->flip ^= (uint8_t)(b ^ 1u);
            t->bit++;
        }
        t->ph += CAR_STEP;
        s = (int32_t)(((int64_t)sinq(t->ph) * amp) >> 30);
        out[k] = t->flip ? -s : s;
        if (t->bit > t->bits) t->on = 0;
    }
    return t->on;
}
