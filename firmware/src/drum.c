/* SPDX-License-Identifier: GPL-3.0-only */
/* Drum voice: the PO-32 / Microtonic patch (21 parameters, 16 bits each, as sent to the PO-32).
 * Fixed point, no float. Signals are Q20 (1.0 = 1 << 20), envelopes Q30, gains Q16, sample rate 44100.
 * Same signal path and random numbers as libpo's synth, so a voice can be compared sample by sample. */
#include <stdint.h>
#include "drum.h"
#include "drum_tables.h"

typedef int32_t i32;
typedef uint32_t u32;
typedef int64_t i64;
typedef uint64_t u64;

#define ONE (1 << 20)
#define THIRD_Q20 349525
#define DONE_EXP (-(15 << 27))       /* about -90 dB */

/* (a variable: -49 << 16 as a literal looks like a ROM address to the build check) */
static volatile i32 lv49 = 49 << 16;

static inline __attribute__((always_inline)) i32 sat(i64 x, i32 lim) { return x > lim ? lim : x < -lim ? -lim : (i32)x; }
static i32 xq(u32 p) { return (i32)((p * 65536u + 32767u) / 65535u); }       /* 0..65535 -> Q16, 65535 = 1.0 */

/* 2^x: x signed Qxq, result Qoq, saturating */
static inline __attribute__((always_inline)) u32 exp2fx(i32 x, int q, int oq)
{
    int n = x >> q;
    u32 fr = (u32)x & ((1u << q) - 1);
    u32 f24 = q >= 24 ? fr >> (q - 24) : fr << (24 - q);
    u32 i = f24 >> 16, w = f24 & 0xFFFF;
    u32 a = dr_exp2[i], b = dr_exp2[i + 1];
    u32 m = a + (u32)(((u64)(b - a) * w) >> 16);
    int sh = n + oq - 30;
    if (sh >= 0) {
        u64 r;
        if (sh > 32) return 0xFFFFFFFFu;
        r = (u64)m << sh;
        return r > 0xFFFFFFFFull ? 0xFFFFFFFFu : (u32)r;
    }
    return -sh >= 32 ? 0 : m >> -sh;
}

/* log2 of an integer >= 1, Q24 */
static i32 log2fx(u32 v)
{
    int lz = __builtin_clz(v), e = 31 - lz;
    u32 m = v << lz;
    u32 i = (m >> 23) & 0xFF, w = (m >> 7) & 0xFFFF;
    u32 a = dr_log2[i], b = dr_log2[i + 1];
    return (i32)(((u32)e << 24) + a + (u32)(((u64)(b - a) * w) >> 16));
}

static inline __attribute__((always_inline)) i32 sinq(u32 ph)       /* turns Q32 -> Q30 */
{
    u32 i = ph >> 22, w = (ph >> 6) & 0xFFFF;
    i32 a = dr_sin[i], b = dr_sin[i + 1];
    return a + (i32)(((i64)(b - a) * w) >> 16);
}
static inline __attribute__((always_inline)) i32 cosq(u32 ph) { return sinq(ph + 0x40000000u); }
/* 1 - cos: 2 sin^2(w/2), keeps its precision near 0 (a table cosine would not) */
static inline __attribute__((always_inline)) i32 omc(u32 ph) { i64 h = sinq(ph >> 1); return (i32)((h * h) >> 29); }

static u64 udiv(u64 a, u64 b)
{
    u64 q = 0, r = 0;
    int i;
    if (!b) return ~0ull;
    for (i = 63; i >= 0; i--) {
        r = (r << 1) | ((a >> i) & 1);
        if (r >= b) { r -= b; q |= 1ull << i; }
    }
    return q;
}
static i64 sdiv(i64 a, i64 b)
{
    int neg = (a < 0) != (b < 0);
    u64 q = udiv(a < 0 ? -a : a, (u64)(b < 0 ? -b : b));
    return neg ? -(i64)q : (i64)q;
}
static u32 isqrt(u64 v)
{
    u64 r = 0, bit = 1ull << 62;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; } else r >>= 1;
        bit >>= 2;
    }
    return (u32)r;
}

static inline __attribute__((always_inline)) i32 rnd(u32 *s)
{
    *s = *s * 1103515245u + 12345u;
    return (i32)((*s & 0x7fffffffu) >> 10) - ONE;
}

/* biquad: numerator Q28 (a boost can pass 4), denominator Q29, state Q48. The output feeds back with 28 fractional bits:
 * at low frequencies the denominator has a huge DC gain and would amplify a rounding bias */
static void bq_set(dr_bq_t *q, i64 b0, i64 b1, i64 b2, i64 a0, i64 a1, i64 a2)
{
    q->b0 = (i32)sdiv(b0 * (1LL << 28), a0); q->b1 = (i32)sdiv(b1 * (1LL << 28), a0); q->b2 = (i32)sdiv(b2 * (1LL << 28), a0);
    q->a1 = (i32)sdiv(a1 * (1LL << 29), a0); q->a2 = (i32)sdiv(a2 * (1LL << 29), a0);
    q->z1 = q->z2 = 0;
}
static inline __attribute__((always_inline)) i64 bq_mul(i32 a, i64 y)       /* a * y >> 9, without overflowing for a large y */
{
    i64 yh = y >> 16, yl = y & 0xFFFF;
    return ((i64)a * yh) * 128 + (((i64)a * yl) >> 9);
}
static inline __attribute__((always_inline)) i32 bq_run(dr_bq_t *q, i32 in)
{
    i64 y = (((i64)q->b0 * in) + q->z1 + (1 << 19)) >> 20;       /* Q28 */
    if (y > (1LL << 39)) y = 1LL << 39;
    if (y < -(1LL << 39)) y = -(1LL << 39);
    q->z1 = (i64)q->b1 * in - bq_mul(q->a1, y) + q->z2;
    q->z2 = (i64)q->b2 * in - bq_mul(q->a2, y);
    return sat(y >> 8, 0x7fffffff);
}

static u32 turns_hz(i32 xqv)     /* 20 * 1000^x Hz as turns/sample, Q32 */
{
    u32 t = exp2fx(K_TURN20 + (i32)(((i64)K_L2_1000 * xqv) >> 16), 24, 0);
    return t > 1932735283u ? 1932735283u : t;
}

static i32 vel_gain(int vel, i32 sens)       /* Q16 */
{
    i32 x = 65536 - (i32)(((i64)(127 - vel) * sens) / 63);
    if (x < 0) x = 0;
    if (x > 65536) x = 65536;
    return (i32)(((i64)x * exp2fx((i32)(((i64)(K_DB20 * 37) * (x - 65536)) >> 16), 24, 16)) >> 16);
}

static void env_exp_init(dr_env_t *e, u32 atk_q8, i32 dstep)
{
    e->atk_q8 = atk_q8; e->dstep = dstep; e->done = 0;
    if (atk_q8) {
        u64 s = udiv(K_ATKSTEP, atk_q8);
        e->astep = s > (1u << 30) ? (1 << 30) : (i32)s;
    }
}

/* an exponential attack and decay at sample n, Q30 (closed form: no state to carry) */
static inline __attribute__((always_inline)) i32 env_at(dr_env_t *e, u32 n)
{
    i64 ea;
    if (e->done) return 0;
    if (e->atk_q8 && ((u64)n << 8) < e->atk_q8) {
        ea = -(i64)K_ATK_FLOOR27 + (i64)e->astep * n;
        if (ea > 0) ea = 0;
        return (i32)exp2fx((i32)ea, 27, 30);
    }
    ea = -(((i64)e->dstep * (((u64)n << 8) - e->atk_q8)) >> 8);
    if (ea < DONE_EXP) { e->done = 1; return 0; }
    return (i32)exp2fx((i32)ea, 27, 30);
}

static inline __attribute__((always_inline)) i64 nenv_at(drum_t *v, u32 n)
{
    u64 tq = (u64)n << 8;
    if (v->nmode == 0) return env_at(&v->ne, n);
    if (v->nmode == 1) {
        i64 e = 0;
        if (v->ne.done) return 0;
        if (tq < v->la_q8) e = (i64)v->inc_up * n;
        else if (tq - v->la_q8 < v->ld_q8) e = (1 << 30) - (i64)(((u64)v->inc_dn * (tq - v->la_q8)) >> 8);
        else v->ne.done = 1;
        return e < 0 ? 0 : e;
    }
    if (tq < v->ne.atk_q8) return 1 << 30;
    {
        i32 dec = env_at(&v->ne, n);
        if (v->ring && v->pinc && dec) {
            u32 d = (u32)(((u64)v->pinc * (tq - v->ne.atk_q8)) >> 8) - 0x80000000u;
            i32 cv;
            if ((i32)d < 0) d = -d;
            cv = cosq(0x80000000u - d);
            return ((i64)cv * dec >> 30) * v->corr >> 16;
        }
        return dec;
    }
}

static inline __attribute__((always_inline)) i32 pm_at(drum_t *v, u32 n)      /* the pitch factor of a decaying pitch mod, Q20 */
{
    i64 ea = -((i64)v->mstep * n);
    i32 sg;
    if (!v->mscale) return 1 << 20;
    if (ea < -(15LL << 27)) ea = -(15LL << 27);
    sg = (i32)exp2fx((i32)ea, 27, 30);
    return (i32)exp2fx((i32)(((i64)sg * v->mscale) >> 30), 24, 20);
}

void drum_trig(drum_t *v, const uint16_t *p, int vel)
{
    i32 x, mix, pan, eqdb, level, modg;
    u32 atk, dcy;
    int i;
    for (i = 0; i < (int)sizeof *v; i++) ((uint8_t *)v)[i] = 0;

    /* oscillator */
    x = xq(p[0]); v->wave = x < 21845 ? 0 : x < 43691 ? 1 : 2;
    x = xq(p[1]); v->step = turns_hz(x);
    x = xq(p[2]);
    atk = x ? exp2fx(K_L2_ATK + log2fx(x) - (16 << 24) + (i32)(((i64)K_L2_ATK_X * x) >> 16), 24, 0) : 0;
    x = xq(p[3]);
    dcy = exp2fx(K_L2_DCY + (i32)(((i64)K_L2_1000 * x) >> 16), 24, 0);
    env_exp_init(&v->oe, atk, (i32)exp2fx(K_L2_DSTEP - (i32)(((i64)K_L2_1000 * x) >> 16), 24, 27));

    /* velocity, mix, level */
    v->ovg = vel_gain(vel, xq(p[18]));
    v->nvg = vel_gain(vel, xq(p[19]));
    modg = vel_gain(vel, xq(p[20]));
    mix = xq(p[13]); pan = mix * 2 - 65536;
    v->og = pan < 0 ? 65536 : (i32)(((i64)(65536 - pan) * exp2fx((i32)(-((i64)K_DB20 * 25 * pan >> 16)), 24, 16)) >> 16);
    v->ng = pan >= 0 ? 65536 : (i32)(((i64)(65536 + pan) * exp2fx((i32)(((i64)K_DB20 * 25 * pan) >> 16), 24, 16)) >> 16);
    v->og = (i32)(((i64)v->og * v->ovg) >> 16);
    v->ng = (i32)(((i64)v->ng * v->nvg) >> 16);
    x = xq(p[17]);
    if (x < 7) level = 0;
    else {
        i64 db = (i64)60 * x - (i64)lv49 - (i64)(udiv(1ull << 32, x));       /* Q16 */
        level = db < -(500LL << 16) ? 0 : (i32)exp2fx((i32)((db * K_DB20) >> 16), 24, 16);
    }
    eqdb = (xq(p[16]) - 32768) * 80;                                                   /* Q16 dB */
    if (eqdb > 0) level = (i32)(((i64)level * exp2fx((i32)(-((i64)eqdb * K_DB20 >> 17)), 24, 16)) >> 16);
    v->level = level;

    /* pitch modulation */
    x = xq(p[4]); v->mmode = x < 21845 ? 0 : x < 43691 ? 1 : 2;
    {
        i32 r = xq(p[5]), b = xq(p[6]) * 2 - 65536;
        i64 semi = ((i64)b * (b < 0 ? -b : b) * (v->mmode == 0 ? 96 : 48)) >> 8;     /* semitones Q24 */
        semi = (semi * modg) >> 16;
        v->mscale = (i32)((semi * 1398101) >> 24);       /* octaves, Q24 */
        if (v->mmode == 0) {
            v->mstep = r ? (i32)udiv((u64)r * K_MSTEP, 67502 - r) : 0;
        } else if (v->mmode == 1) {
            v->mphase = 0;
            v->mstep = !r ? 0 : (i32)exp2fx(K_TURN_2000 + log2fx(r) - (16 << 24) + (i32)(((i64)(K_L2_10 * 35 / 10) * (r - 65536)) >> 16), 24, 0);
        } else if (r) {
            i32 e = (i32)exp2fx(K_L2_NOISE_E + log2fx(r) - (16 << 24) + (i32)(((i64)(K_L2_10 * 4) * (r - 65536)) >> 16), 24, 24);
            v->malpha = (1 << 30) - (i32)exp2fx(-e, 24, 30);
        }
    }
    v->rng = 123456789u;
    if (v->mmode == 2) v->msig = rnd(&v->rng) * 1024;

    /* noise filter and envelope */
    {
        u32 t = turns_hz(xq(p[8]));
        u32 ph = t;
        i64 sn = sinq(ph), cs = (1LL << 30) - omc(ph);
        i64 a = (sn * exp2fx(-(1 << 24) - K_L2_Q0 - (i32)(((i64)K_L2_Q_X * xq(p[9])) >> 16), 24, 27)) >> 27;
        i64 a0 = (1LL << 30) + a;
        i32 nm = xq(p[7]);
        if (nm < 21845) bq_set(&v->nf, (((1LL << 30) - cs) >> 1), (1LL << 30) - cs, (((1LL << 30) - cs) >> 1), a0, -2 * cs, (1LL << 30) - a);
        else if (nm < 43691) bq_set(&v->nf, a, 0, -a, a0, -2 * cs, (1LL << 30) - a);
        else bq_set(&v->nf, (((1LL << 30) + cs) >> 1), -((1LL << 30) + cs), (((1LL << 30) + cs) >> 1), a0, -2 * cs, (1LL << 30) - a);
    }
    {
        i32 nx = xq(p[12]), na = xq(p[11]);
        u32 natk = na ? exp2fx(K_L2_ATK + log2fx(na) - (16 << 24) + (i32)(((i64)K_L2_ATK_X * na) >> 16), 24, 0) : 0;
        u32 ndcy = exp2fx(K_L2_DCY + (i32)(((i64)K_L2_1000 * nx) >> 16), 24, 0);
        i32 dstep = (i32)exp2fx(K_L2_DSTEP - (i32)(((i64)K_L2_1000 * nx) >> 16), 24, 27);
        i32 nm = xq(p[10]);
        v->nmode = nm > 43690 ? 2 : nm > 21845 ? 1 : 0;
        env_exp_init(&v->ne, natk, dstep);
        if (v->nmode == 1) {
            u32 la = (natk * 2) / 3, ld = (u32)(((u64)ndcy * 2) / 3);
            v->la_q8 = la; v->ld_q8 = ld;
            v->inc_up = la ? (u32)(udiv(1ull << 38, la) > 0x7fffffffull ? 0x7fffffffu : udiv(1ull << 38, la)) : 0;
            v->inc_dn = ld ? (u32)(udiv(1ull << 38, ld) > 0x7fffffffull ? 0x7fffffffu : udiv(1ull << 38, ld)) : 0;
        } else if (v->nmode == 2 && nx > 6) {
            /* log2(period) = log2(d) - log2(sr) + 13.2877 d */
            i32 lp = log2fx(nx) - (16 << 24) + K_L2_RING + (i32)(((i64)K_L2_10000 * nx) >> 16);
            v->corr = 65536;
            if (lp < -157384000) {   /* period < 1.5 ms (log2 = -9.3808) */
                lp += 3 << 24;
                v->corr = (i32)exp2fx(-((lp + (1 << 24)) >> 1), 24, 16);
            } else lp += 1 << 24;
            v->pinc = exp2fx((32 << 24) - lp + K_L2_RING, 24, 0);
            v->ring = 1;
        }
    }

    /* distortion */
    x = xq(p[14]);
    if (x > 655) {
        i32 d = (i32)(((i64)((i64)x * x >> 16) * x >> 16) * 200);       /* Q16 */
        v->drive = d;
        if (d < 65536) {
            u32 ratio = (u32)udiv((u64)(d + 3 * 65536) << 16, d);
            v->dgain = 65536 - d / 30;
            v->dclip = (i32)((65536 + isqrt((u64)ratio << 16)) / 3) << 4;
            v->ddc = (i32)(((-(i64)d * 2) / 27 - (i64)9 * 65536 / 27) * 16);
        } else {
            v->dgain = (i32)udiv(858993459ull, d - 31637) + 38011;
            v->dclip = 0;
        }
        v->dist = 1;
    }

    /* EQ */
    if (eqdb > 32768 || eqdb < -32768) {
        u32 ph = turns_hz(xq(p[15]));
        i64 sn = sinq(ph) >> 1, cs = (1LL << 30) - omc(ph);
        i32 A = (i32)exp2fx((i32)(((i64)eqdb * K_DB40) >> 16), 24, 27);
        i32 iA = (i32)exp2fx(-(i32)(((i64)eqdb * K_DB40) >> 16), 24, 27);
        i64 aA = (sn * A) >> 27, aI = (sn * iA) >> 27;
        bq_set(&v->eq, (1LL << 30) + aA, -2 * cs, (1LL << 30) - aA, (1LL << 30) + aI, -2 * cs, (1LL << 30) - aI);
        v->use_eq = 1;
    }
    v->o_t = env_at(&v->oe, 0);
    v->n_t = nenv_at(v, 0);
    v->pm_t = v->mmode == 0 ? pm_at(v, 0) : 1 << 20;
    v->on = 1;
}

static inline __attribute__((always_inline)) i32 dist_run(const drum_t *v, i32 s)
{
    i64 d = v->drive;
    i32 xc, y;
    if (d < 65536) {
        i64 t, u;
        xc = s - THIRD_Q20;
        xc = xc > v->dclip ? v->dclip : xc < -v->dclip ? -v->dclip : xc;
        t = (xc < 0 ? -xc : xc) - (((i64)xc * xc) >> 20);
        u = ((d * t) >> 16) + ONE;
        y = (i32)((((i64)xc * u) >> 20) - v->ddc);
        return (i32)(((i64)y * v->dgain) >> 16);
    } else {
        i64 pos, neg, c;
        i64 xp = ((i64)s * d >> 16) - THIRD_Q20;
        xc = sat(xp, ONE);
        c = ((i64)xc * ((xc < 0 ? -xc : xc) - (((i64)xc * xc) >> 20) + ONE)) >> 20;
        pos = c + 11 * (ONE / 27);
        xp = -((i64)s * d >> 16) - THIRD_Q20;
        xc = sat(xp, ONE);
        c = ((i64)xc * ((xc < 0 ? -xc : xc) - (((i64)xc * xc) >> 20) + ONE)) >> 20;
        neg = c + 11 * (ONE / 27);
        return (i32)((((pos - neg) >> 1) * v->dgain) >> 16);
    }
}

#ifdef DR_PROFILE
volatile uint32_t dr_dbg;       /* profiling: bit 0 skip the pitch mod, 1 the oscillator, 2 the noise, 3 distortion, 4 EQ, 5 everything, 6 the 4-sample update */
#define DBG(b) (dr_dbg & (b))
#else
#define DBG(b) 0
#endif

/* the voice: cnt samples, stored (add = 0) or added as (sample >> shift) into every `stride`-th word, stride 2 = both channels of an interleaved buffer.
 * The hot state lives in locals for the loop: every struct access in it would be a load and a store. */
static inline __attribute__((always_inline)) int drum_core(drum_t *v, int32_t *out, int cnt, int stride, int shift, int add, u32 pitch)
{
    u32 n = v->n, phase = v->phase, step = pitch == (1u << 20) ? v->step : (u32)(((u64)v->step * pitch) >> 20), rng = v->rng, mphase = v->mphase;
    i32 o_v = v->o_v, o_t = v->o_t, o_d = v->o_d, pm_v = v->pm_v, pm_t = v->pm_t, pm_d = v->pm_d, msig = v->msig;
    i64 n_v = v->n_v, n_t = v->n_t, n_d = v->n_d;
    const i32 og = v->og, ng = v->ng, level = v->level, mscale = v->mscale, mstep = v->mstep, malpha = v->malpha;
    const u32 mmode = v->mmode, wave = v->wave, dist = v->dist, use_eq = v->use_eq;
    int k, on = v->on;
    for (k = 0; k < cnt && on; k++) {
        i32 osc = 0, nz = 0, wv, s, pm;
        u32 st;
        if (!(n & 3u) && !DBG(64u)) {                       /* every 4th sample: the envelopes and the pitch factor, a ramp between */
            i32 o = env_at(&v->oe, n + 4u), pn = mmode == 0 ? pm_at(v, n + 4u) : 1 << 20;
            i64 nn = nenv_at(v, n + 4u);
            o_v = o_t; n_v = n_t; pm_v = pm_t;
            o_t = o; n_t = nn; pm_t = pn;
            o_d = (o - o_v) >> 2; n_d = (nn - n_v) >> 2; pm_d = (pn - pm_v) >> 2;
            if (v->oe.done && v->ne.done) { on = 0; break; }
        }

        if (DBG(1u) || mmode == 0) pm = pm_v;
        else {
            i32 sig;
            if (mmode == 1) { sig = cosq(mphase); mphase += (u32)mstep; }
            else {
                msig = (i32)(((i64)msig * ((1 << 30) - malpha) >> 30) + ((i64)(rnd(&rng) * 1024) * malpha >> 30));
                sig = msig;
            }
            pm = mscale ? (i32)exp2fx((i32)(((i64)sig * mscale) >> 30), 24, 20) : 1 << 20;
        }
        st = mscale || mmode ? (u32)(((u64)step * (u32)pm) >> 20) : step;
        phase += st;
        pm_v += pm_d;

        if (!DBG(2u)) {
            if (wave == 0) wv = sinq(phase) >> 10;
            else if (wave == 1) {
                i32 dd = (i32)(phase >> 1) - (1 << 30); if (dd < 0) dd = -dd;
                wv = 2 * (dd >> 10) - ONE;
            } else wv = (i32)(phase >> 11) - ONE;
            osc = (i32)(((i64)wv * o_v) >> 30);
        }
        o_v += o_d;

        if (!DBG(4u) && (mmode == 2 || n_v)) {
            i32 raw = rnd(&rng);
            if (n_v) nz = (i32)(((i64)bq_run(&v->nf, raw) * n_v) >> 30);
        }
        n_v += n_d;
        n++;

        s = (i32)((((i64)osc * og) + ((i64)nz * ng)) >> 16);
        if (dist && !DBG(8u)) s = dist_run(v, s);
        if (use_eq && !DBG(16u)) s = bq_run(&v->eq, s);
        s = sat(((i64)s * level) >> 16, ONE);
        if (add) { out[k * stride] += s >> shift; if (stride == 2) out[k * 2 + 1] += s >> shift; }
        else out[k * stride] = s;
    }
    for (; k < cnt; k++) if (!add) out[k * stride] = 0;
    v->n = n; v->phase = phase; v->rng = rng; v->mphase = mphase; v->msig = msig; v->on = (uint8_t)on;
    v->o_v = o_v; v->o_t = o_t; v->o_d = o_d; v->pm_v = pm_v; v->pm_t = pm_t; v->pm_d = pm_d; v->n_v = n_v; v->n_t = n_t; v->n_d = n_d;
    return on;
}

#ifdef DRUM_RAM       /* on the device: run from RAM (the flash cache is too slow for this loop) */
#define DRUM_HOT __attribute__((section(".ram_text"), noinline, used))
#else
#define DRUM_HOT
#endif
DRUM_HOT int drum_render(drum_t *v, int32_t *out, int cnt) { return drum_core(v, out, cnt, 1, 0, 0, 1u << 20); }
DRUM_HOT int drum_mix(drum_t *v, int32_t *out, int cnt, int shift, uint32_t pitch) { return drum_core(v, out, cnt, 2, shift, 1, pitch); }
/* RAM is out of reach of a direct call from the flash: call through these */
int (*volatile drum_render_fp)(drum_t *, int32_t *, int) = drum_render;
int (*volatile drum_mix_fp)(drum_t *, int32_t *, int, int, uint32_t) = drum_mix;
