/* SPDX-License-Identifier: GPL-3.0-only */
/* Drum voice (drum.c): a PO-32 patch, 21 parameters of 16 bits in wire order:
 * OscWave OscFreq OscAtk OscDcy ModMode ModRate ModAmt NFilMod NFilFrq NFilQ NEnvMod
 * NEnvAtk NEnvDcy Mix DistAmt EQFreq EQGain Level OscVel NVel ModVel */
#ifndef DRUM_H
#define DRUM_H
#include <stdint.h>

#define DR_PARAMS 21

typedef struct { int32_t b0, b1, b2, a1, a2; int64_t z1, z2; } dr_bq_t;
typedef struct { uint32_t atk_q8; int32_t astep, dstep; uint8_t done; } dr_env_t;

typedef struct {
    uint8_t on, wave, mmode, nmode, use_eq, dist, ring;
    uint32_t n, phase, step, rng;
    dr_env_t oe, ne;
    int32_t og, ng, ovg, nvg, level;
    int32_t mscale, mstep, malpha, msig, mea;
    uint32_t mphase;
    dr_bq_t nf, eq;
    int32_t o_v, o_t, o_d, pm_v, pm_t, pm_d;       /* envelopes and the pitch factor: worked out every 4th sample, in between a ramp */
    int64_t n_v, n_t, n_d;
    uint32_t la_q8, ld_q8, inc_up, inc_dn, pinc, pph;
    int32_t corr, drive, dgain, dclip, ddc;
} drum_t;

void drum_trig(drum_t *v, const uint16_t *p, int vel);      /* start a hit, vel 1..127 */
int drum_render(drum_t *v, int32_t *out, int n);            /* Q20 samples; returns 0 once the hit is over */
#endif
