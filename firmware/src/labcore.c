/* SPDX-License-Identifier: GPL-3.0-only */
/* Drumlab's instrument, the way the PO-32 does it: 16 sounds, each two patches (A, B) and two knobs (pitch, morph); 16 patterns of 16 steps
 * on 4 lanes (sound % 4 is the lane: one voice, a new hit cuts the old one); accent, swing, tempo, a chain of up to 64 patterns.
 * The data is kept in the PO-32's own wire format, so a transfer is a copy. The clock and the voices run in the audio interrupt. */

#ifdef CHIPBOX_HOST
#define POOL_
#else
#define POOL_ __attribute__((section(".pool")))
#endif
#define NS 16
#define NPAT 16
#define CHAIN_MAX 64

typedef struct { uint8_t trig[4][16]; uint8_t lock[4][16][2]; uint16_t acc; uint16_t fx[16]; } lpat_t;        /* fx: the punch-in effects recorded on each step (a bit per effect); trig: bits 0-3 fill (0 = no hit), 4-5 sound / 4, 7 accent; lock: pitch, morph */

#define PITCH_MID 0x80u
static uint16_t kp[NS][2][DR_PARAMS];            /* the patches: sound, A / B */
static uint8_t snm[NS];                          /* what each sound is called: 0 = the LAB-1 kit's name, else 1 + column * 8 + archetype (labgen.c) */
static uint8_t knob[NS][2];                      /* the sound's knobs: pitch (0x80 = none), morph (0 = A .. 255 = B) */
static lpat_t pat[NPAT];
static uint8_t chain[CHAIN_MAX], chain_n;        /* empty: the current pattern loops */
static uint8_t cur_pat;                          /* the pattern on show (and the one that loops) */
static uint8_t bpm = 120, swing;                 /* 60..240, 0..12 (x 12 as the PO-32 stores it) */

/* what the audio side is doing */
static volatile uint8_t sq_run, sq_step, sq_cpos, sq_pat;
static volatile uint8_t sync_mode, fol_on;       /* sync_mode: 0 own clock, 1 send MIDI clock, 2 follow it; fol_on: an outside clock is running */
static uint32_t clk_acc, fol_s_clk, fol_s_start, fol_s_cont, fol_s_stop, fol_ph, fol_age, fol_idle;
static volatile uint32_t sq_hit;                 /* a bit per sound: it sounded since the screen last looked */
static uint32_t sq_phase, sq_len;                /* in 1/256 samples */
static uint32_t sq_swing_left;                   /* an odd step waits for its swing */
static uint8_t sq_pending;
static drum_t lv[4];
static volatile uint8_t evq_w, evq_r;
static struct { uint8_t s, acc, pitch, morph; } evq[8];
static volatile uint16_t mute_mask;              /* a bit per lane */
static volatile int8_t solo_lane = -1;

/* the 16 punch-in effects of the PO-32, on the 16 keys while the FX layer is up */
enum { FX_HALF, FX_DIST, FX_SQUASH, FX_ECHO, FX_PLFO, FX_EQSW, FX_MORPH, FX_BEND, FX_PUNCH, FX_68, FX_REP, FX_REP2, FX_FM, FX_GRAIN, FX_REV, FX_BOUNCE };
#define FXB(n) (1u << (n))
static volatile uint16_t fx_mask, fx_rec, fx_eff;       /* fx_mask: held now; fx_rec: recorded on the step that plays; fx_eff: both */
static const char *const FX_NAME[16] = {"HALF", "DIST", "SQUASH", "ECHO", "P-LFO", "EQ SW", "MORPH", "BEND", "PUNCH", "6/8", "REPEAT", "REP 2", "FM", "GRAIN", "REVRSE", "BOUNCE"};

static void core_init(void)
{
    uint32_t s, i;
    for (s = 0; s < NS; s++) {
        for (i = 0; i < DR_PARAMS; i++) { kp[s][0][i] = kit_patch[s][0][i]; kp[s][1][i] = kit_patch[s][1][i]; }
        knob[s][0] = PITCH_MID; knob[s][1] = 0;
    }
    memset(pat, 0, sizeof pat); memset(snm, 0, sizeof snm);
    chain_n = 0; cur_pat = 0; bpm = 120; swing = 0;
    sq_len = 0;
}

/* a hit: the two patches blended by morph, the pitch knob moving the oscillator (+-12 semitones across the knob) */
static void sound_params(uint32_t s, uint32_t pitch, uint32_t morph, uint16_t *p)
{
    uint32_t i;
    int32_t d;
    for (i = 0; i < DR_PARAMS; i++) {
        int32_t a = kp[s][0][i], b = kp[s][1][i];
        p[i] = (uint16_t)(a + (b - a) * (int32_t)morph / 255);
    }
    d = ((int32_t)pitch - (int32_t)PITCH_MID) * 51;               /* 548 of 65535 a semitone, 12 across the half knob */
    d += (int32_t)p[1];
    p[1] = (uint16_t)(d < 0 ? 0 : d > 65535 ? 65535 : d);
}

static void fire(uint32_t s, uint32_t accent, uint32_t pitch, uint32_t morph)
{
    uint16_t p[DR_PARAMS];
    uint32_t lane = s & 3u;
    if ((mute_mask >> lane & 1u) || (solo_lane >= 0 && solo_lane != (int)lane)) return;
    if (fx_eff & FXB(FX_MORPH)) morph = 255u;
    sound_params(s, pitch, morph, p);
    drum_trig(&lv[lane], p, accent ? 127 : 96);
    sq_hit |= 1u << s;
}

static void step_fire(uint32_t st)
{
    const lpat_t *q = &pat[sq_pat];
    uint32_t l;
    fx_rec = q->fx[st];
    for (l = 0; l < 4u; l++) {
        uint32_t t = q->trig[l][st];
        if (t & 15u) fire(l + 4u * ((t >> 4) & 3u), (t >> 7) | (q->acc >> st & 1u), q->lock[l][st][0], q->lock[l][st][1]);
    }
}

static uint8_t pat_now(void) { return chain_n ? chain[sq_cpos % chain_n] : cur_pat; }

static void tempo_set(uint32_t b)
{
    bpm = (uint8_t)(b < 60u ? 60u : b > 240u ? 240u : b);
    sq_len = (uint32_t)((661500u * 256u) / bpm);       /* a sixteenth, in 1/256 samples */
}

static void seq_play(void)
{
    if (!sq_len) tempo_set(bpm);
    sq_step = 0; sq_cpos = 0; sq_pat = pat_now();
    sq_phase = sq_len;                                  /* the first step on the first sample */
    sq_swing_left = 0; sq_pending = 0;
    clk_acc = sq_len;                                   /* the first clock tick on the first sample */
    if (sync_mode == 1u) midi_send(0xFAu, 0, 0);
    sq_run = 1;
}
static void seq_stop(void) { if (sq_run && sync_mode == 1u) midi_send(0xFCu, 0, 0); sq_run = 0; fx_rec = 0; }

/* ---- the effects that work on the sound (the others change the sequencer: below) */
#define RB 32768u
#define EB 22528u
static int16_t fx_rb[RB] POOL_;               /* the last 0.74 s, dry: for GRAIN and REVRSE */
static int16_t fx_ec[EB] POOL_;               /* the echo */
static uint32_t rb_w, ec_w, fx_ph, fx_t;
static int32_t half_hold, lp_y, sq_env;
static uint32_t gr_start, gr_i, rv_start, rv_i, rp_start, rp_len, rp_i, bb_end, bb_len, bb_i;
static uint16_t fx_prev;

static int32_t clip16(int32_t x) { return x > 32767 ? 32767 : x < -32767 ? -32767 : x; }
static int32_t knee(int32_t x, int32_t t)        /* a soft ceiling at t */
{
    if (x > t) return t + ((x - t) >> 3);
    if (x < -t) return -t + ((x + t) >> 3);
    return x;
}

static void fx_mix(int32_t *out, uint32_t n)
{
    uint32_t i, m = fx_eff;
    for (i = 0; i < n; i++) {
        int32_t x = out[2u * i];
        fx_rb[rb_w & (RB - 1u)] = (int16_t)clip16(x); rb_w++;
        if (m & FXB(FX_HALF)) { if (i & 1u) x = half_hold; else half_hold = x; }
        if (m & FXB(FX_DIST)) x = knee(x * 4, 16000);
        if (m & FXB(FX_SQUASH)) {
            int32_t a = x < 0 ? -x : x, g;
            sq_env = a > sq_env ? a : sq_env - (sq_env >> 11);
            g = sq_env > 5000 ? (int32_t)(((int64_t)5000 << 12) / sq_env) : 4096;
            x = (int32_t)(((int64_t)x * g) >> 12) * 3;
        }
        if (m & FXB(FX_PUNCH)) x = knee(x + (x >> 1), 22000);
        if (m & FXB(FX_FM)) x = (int32_t)(((int64_t)x * sinq(fx_ph * 3u)) >> 30);
        if (m & FXB(FX_EQSW)) {
            int32_t a = 20 + (int32_t)(((sinq(fx_ph >> 2) >> 20) + 1024) * 220 >> 11);        /* a low pass that swings: 20 .. 240 of 1024 */
            lp_y += (int32_t)(((int64_t)(x - lp_y) * a) >> 10);
            x = lp_y;
        }
        if (m & (FXB(FX_REP) | FXB(FX_REP2))) { x = fx_rb[(rp_start + rp_i) & (RB - 1u)]; if (++rp_i >= rp_len) rp_i = 0; }          /* beat repeat: the last step (or half of it), looped */
        if (m & FXB(FX_BOUNCE)) {                                                                              /* the ball: a loop that shortens every time round */
            x = fx_rb[(bb_end - bb_len + bb_i) & (RB - 1u)];
            if (++bb_i >= bb_len) { bb_i = 0; bb_len = bb_len * 3u / 4u; if (bb_len < 260u) bb_len = 260u; }
        }
        if (m & FXB(FX_GRAIN)) { x = fx_rb[(gr_start + gr_i) & (RB - 1u)]; if (++gr_i >= 2646u) gr_i = 0; }
        if (m & FXB(FX_REV)) { x = fx_rb[(rv_start - rv_i) & (RB - 1u)]; if (++rv_i >= 22050u) rv_i = 0; }
        if (m & FXB(FX_ECHO)) {
            uint32_t dl = (uint32_t)(((uint64_t)sq_len * 3u) >> 8);                             /* three steps: a dotted eighth */
            int32_t d = fx_ec[(ec_w - (dl > EB - 2u ? EB - 2u : dl)) % EB];
            fx_ec[ec_w % EB] = (int16_t)clip16(x + (d * 5 >> 3));
            x += d >> 1;
        } else fx_ec[ec_w % EB] = 0;
        ec_w++;
        fx_ph += 0x00800000u / 4u;
        x = clip16(x);
        out[2u * i] = out[2u * i + 1u] = x;
    }
}

/* ---- the audio side: events, the clock, the effects that move the sequencer, the four voices */
static uint8_t last_ps;

static uint32_t fx_pitch(uint32_t n)                       /* the oscillators' pitch factor, Q20: PITCH LFO and BEND */
{
    uint32_t m = fx_eff;
    int32_t arg = 0;
    fx_t += n;
    if (!(m & (FXB(FX_PLFO) | FXB(FX_BEND)))) { if (!(m & FXB(FX_BEND))) fx_t = 0; return 1u << 20; }
    if (m & FXB(FX_PLFO)) arg += (int32_t)(((int64_t)sinq(fx_t * 780u) * 3355443) >> 30);        /* +-0.2 octave at 5 Hz */
    if (m & FXB(FX_BEND)) arg += (int32_t)(fx_t > 66000u ? 16777216 : (int32_t)((uint64_t)fx_t * 16777216u / 66000u));       /* up an octave in 1.5 s */
    return exp2fx(arg, 24, 20);
}

static void seq_advance(uint32_t sw, uint32_t len)          /* one step of the sequencer (swing may hold the odd ones back) */
{
    uint32_t ps;
    if (sq_step >= 16u) { sq_step = 0; if (chain_n) sq_cpos = (uint8_t)((sq_cpos + 1u) % chain_n); sq_pat = pat_now(); }
    ps = sq_step++;
    last_ps = (uint8_t)ps;
    if ((ps & 1u) && sw) {
        sq_swing_left = (uint32_t)(((uint64_t)len * sw) / (12u * 3u * 256u)) * 256u; sq_pending = 1;
        if (!sq_swing_left) { sq_pending = 0; step_fire(ps); }
    } else step_fire(ps);
}
/* an outside MIDI clock: 24 ticks a quarter note, six a step. Start arms it (the next tick is the first step), stop stops, and the tempo is the spacing of the ticks */
static void fol_arm(void)
{
    sq_step = 0; sq_cpos = 0; sq_pat = pat_now(); sq_swing_left = 0; sq_pending = 0; fol_ph = 5; fol_age = 0; sq_run = 1;
}
static uint32_t fol_ticks(uint32_t n)                              /* takes this block's clock and transport; the steps that came due */
{
    uint32_t due = 0, t;
    if (sync_mode != 2u) { fol_on = 0; fol_s_clk = mi_clk; fol_s_start = mi_start; fol_s_cont = mi_cont; fol_s_stop = mi_stop; return 0; }
    if (mi_start != fol_s_start) { fol_s_start = mi_start; fol_arm(); }
    if (mi_cont != fol_s_cont) { fol_s_cont = mi_cont; sq_run = 1; }
    if (mi_stop != fol_s_stop) { fol_s_stop = mi_stop; seq_stop(); }
    fol_idle += n; fol_age += n;
    t = mi_clk - fol_s_clk;
    if (t) { fol_s_clk += t; fol_idle = 0; fol_on = 1; }
    else if (fol_idle > 17640u) fol_on = 0;                           /* no tick for 0.4 s: back to our own clock */
    if (!sq_run) return 0;
    while (t--) {
        if (++fol_ph >= 6u) {
            fol_ph = 0; due++;
            if (fol_age > 2000u && fol_age < 90000u) { sq_len = (sq_len * 3u + fol_age * 256u) / 4u; }
            fol_age = 0;
        }
    }
    return due;
}

static void core_audio(int32_t *out, uint32_t n)
{
    uint32_t i, l, m, pitch;
    uint32_t len = sq_len, sw = swing;
    fx_eff = (uint16_t)(fx_mask | (sq_run ? fx_rec : 0u)); m = fx_eff;
    while (evq_r != evq_w) {
        uint32_t k = evq_r & 7u;
        fire(evq[k].s, evq[k].acc, evq[k].pitch, evq[k].morph);
        evq_r++;
    }
    if ((m ^ fx_prev) & (FXB(FX_REP) | FXB(FX_REP2) | FXB(FX_GRAIN) | FXB(FX_REV) | FXB(FX_BOUNCE))) {      /* an effect just went on: where it takes its material from */
        uint32_t on = m & ~fx_prev, step = (sq_len >> 8) ? (sq_len >> 8) : 5000u;
        if (step > RB - 2u) step = RB - 2u;
        if (on & (FXB(FX_REP) | FXB(FX_REP2))) { rp_len = on & FXB(FX_REP2) ? step >> 1 : step; rp_start = rb_w - rp_len; rp_i = 0; }
        if (on & FXB(FX_GRAIN)) { gr_start = rb_w - 2646u; gr_i = 0; }
        if (on & FXB(FX_REV)) { rv_start = rb_w - 1u; rv_i = 0; }
        if (on & FXB(FX_BOUNCE)) { bb_end = rb_w; bb_len = 7000u; bb_i = 0; }
    }
    fx_prev = (uint16_t)m;
    if (m & FXB(FX_68)) sw = 8u;
    {
        uint32_t due = fol_ticks(n), ext = sync_mode == 2u && fol_on;
        len = sq_len;
        while (due--) seq_advance(sw, len);
        if (sq_run)
            for (i = 0; i < n; i++) {
                if (sq_pending) {
                    if (sq_swing_left > 256u) sq_swing_left -= 256u;
                    else { sq_pending = 0; step_fire(last_ps); }
                }
                if (ext) continue;                                      /* the outside clock moves the steps */
                if (sync_mode == 1u) { clk_acc += 1536u; if (clk_acc >= len) { clk_acc -= len; midi_send(0xF8u, 0, 0); } }
                sq_phase += 256u;
                if (sq_phase >= len) { sq_phase -= len; seq_advance(sw, len); }
            }
    }
    pitch = fx_pitch(n);
    for (l = 0; l < 4u; l++)
        if (lv[l].on) drum_mix_fp(&lv[l], out, (int)n, 6, pitch);
    if (m) fx_mix(out, n); else { for (i = 0; i < n; i++) { fx_rb[rb_w & (RB - 1u)] = (int16_t)clip16(out[2u * i]); rb_w++; } }
}
