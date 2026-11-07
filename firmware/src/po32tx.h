/* SPDX-License-Identifier: GPL-3.0-only */
/* PO-32 transfer (po32tx.c): builds the frame and plays it as audio. Frame format after libpo (MIT, ericlewis/libpo). */
#ifndef PO32TX_H
#define PO32TX_H
#include <stdint.h>

#define PO32TX_MAX 6144              /* frame bytes: 32 patches x 48, 16 patterns x 216, the state, the preamble and tail */

typedef struct {
    uint8_t buf[PO32TX_MAX];
    uint32_t len;
    uint16_t state;
    uint32_t bit, bits;              /* playing: next bit, total bits */
    uint32_t sym, ph;                /* symbol clock, carrier phase (Q32) */
    uint8_t flip, on;
} po32tx_t;

void po32tx_begin(po32tx_t *t);
int po32tx_patch(po32tx_t *t, int slot, int right, const uint16_t *p);      /* slot 0..15; the sound the PO-32 plays */
int po32tx_pattern(po32tx_t *t, int num, const uint8_t trig[4][16], const uint8_t lock[4][16][2], uint32_t accent);   /* a pattern: 4 lanes x 16 steps */
int po32tx_state(po32tx_t *t, const uint8_t knob[16][2], int tempo, int swing12, const uint8_t *chain, int n);       /* the 16 sounds' knobs, tempo, swing x 12, the chain */
int po32tx_finish(po32tx_t *t);                                             /* frame length, 0 if full */
void po32tx_play(po32tx_t *t);
int po32tx_render(po32tx_t *t, int32_t *out, int n, int amp);               /* mono samples, amp = peak; 0 when finished */
#endif
