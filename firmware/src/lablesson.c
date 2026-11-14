/* SPDX-License-Identifier: GPL-3.0-only */
/* Guided lessons: the whole workflow, one instruction at a time. A card over the bottom of the screen says what to do with your hands;
 * a step finishes by itself when the panel shows it happened (a pattern has hits, the tempo moved, the view changed ...), the card goes
 * green, the next step shows. GLO puts the card away and back. The player's project is put back when the lesson ends (les_exit). */
#define NGL (sizeof GLESSON / sizeof GLESSON[0])        /* the guided lessons */
enum { GV_VIEW, GV_WR, GV_HITS, GV_ACC, GV_ACCM, GV_SEL, GV_BPM, GV_SWING, GV_K3, GV_K4, GV_CURPAT, GV_CHAINM, GV_CHAIN, GV_MUTEM, GV_RUN,
       GV_FX, GV_FXM, GV_KEYS, GV_UNDOS, GV_PG, GV_SIDE, GV_KP, GV_KITS, GV_ASK, GV_KITON,
       GV_READ, GV_PA, GV_PB, GV_PCUR, GV_PQ, GV_PSEG, GV_LOCKP, GV_LOCKM, GV_FXPAD, GV_FXREC, GV_LOOP, GV_FINET, GV_KITSEL };       /* GV_READ: a page to read (OCT+ goes on); GV_PA / GV_PB / GV_PCUR: parameter a of the sound's patch A / B / the side on show */
enum { GO_EQ, GO_UP, GO_DOWN, GO_CHG };
enum { GS_EMPTY, GS_BEAT, GS_BEATQ, GS_CHAIN, GS_MC, GS_PATCH };          /* BEAT: a beat and WRITE on; BEATQ: the beat, WRITE off */
typedef struct { const char *t; uint8_t v, op, n, a; const char *title; } gstep_t;      /* a page to read: t is the text, title its title, n the patch to set up when it shows */
typedef struct { const char *name; uint8_t setup, n; const gstep_t *s; uint8_t mc, patch; } glesson_t;      /* mc: a masterclass (1..); patch: the start of a GS_PATCH lesson */
#define GST(t, v, o, n) {t, GV_##v, GO_##o, n, 0, 0}
#define GSTA(t, v, o, n, a) {t, GV_##v, GO_##o, n, a, 0}
#define GRD(title, text, patch) {text, GV_READ, GO_EQ, patch, 0, title}
#include "lessons_pages.h"
static const glesson_t GLESSON[] = {
#include "lessons_intro.h"
    {"SOUNDS", GS_BEAT, 4, (const gstep_t[]){GLI_0 GST("Turn PRESETS to pick sound", SEL, CHG, 0), GST("Press 3 keys to add hits", HITS, UP, 3), GST("Press PLAY to hear it", RUN, EQ, 1)}},
    {"ACCENTS", GS_BEATQ, 5, (const gstep_t[]){GLI_1 GST("Tap ENV for accent mode", ACCM, EQ, 1), GST("Press 2 keys to mark steps", ACC, UP, 2), GST("Tap ENV to leave accents", ACCM, EQ, 0),
        GST("Press PLAY to hear them", RUN, EQ, 1)}},
    {"TEMPO AND SWING", GS_BEATQ, 5, (const gstep_t[]){GLI_2 GST("Press PLAY to start", RUN, EQ, 1), GST("Turn KNOB 1 to raise tempo", BPM, UP, 8), GST("Turn KNOB 2 for swing", SWING, UP, 3),
        GST("Press PLAY again to stop", RUN, EQ, 0)}},
    {"PITCH AND MORPH", GS_BEATQ, 4, (const gstep_t[]){GLI_3 GST("Turn KNOB 3 to change pitch", K3, CHG, 0), GST("Turn KNOB 4 to change morph", K4, CHG, 0), GST("Press a key to hear it", KEYS, UP, 1)}},
    {"PATTERNS", GS_BEAT, 5, (const gstep_t[]){GLI_4 GST("Turn SELECT to pick pattern", CURPAT, CHG, 0), GST("Press 3 keys to write steps", HITS, UP, 3),
        GST("Press OCT+ to copy it on", CURPAT, CHG, 0), GST("Turn SELECT to pattern 1", CURPAT, EQ, 0)}},
    {"CHAIN PATTERNS", GS_CHAIN, 6, (const gstep_t[]){GLI_5 GST("Tap SEL for chain mode", CHAINM, EQ, 1), GST("Press 2 keys to build chain", CHAIN, UP, 2), GST("Tap SEL to leave chain mode", CHAINM, EQ, 0),
        GST("Press PLAY to hear chain", RUN, EQ, 1), GST("Press PLAY again to stop", RUN, EQ, 0)}},
    {"MUTE AND SOLO", GS_BEATQ, 6, (const gstep_t[]){GLI_6 GST("Press PLAY to start", RUN, EQ, 1), GST("Tap ARP for mute mode", MUTEM, EQ, 1), GST("Press a key to mute a lane", KEYS, UP, 1),
        GST("Tap ARP again for solo", MUTEM, EQ, 2), GST("Tap ARP to hear all again", MUTEM, EQ, 0)}},
    {"PUNCH-IN FX", GS_BEATQ, 5, (const gstep_t[]){GLI_7 GST("Press PLAY to start", RUN, EQ, 1), GST("Tap FX for the effects", FX, EQ, 1), GST("Hold a key for an effect", FXM, UP, 1),
        GST("Tap FX to leave", FX, EQ, 0)}},
    {"UNDO AND CLEAR", GS_BEAT, 4, (const gstep_t[]){GLI_8 GST("Press 3 keys to add hits", HITS, UP, 3), GST("Tap LFO to undo", UNDOS, UP, 1), GST("Hold EDIT to clear pattern", HITS, EQ, 0)}},
    {"A BEAT FROM ZERO", GS_EMPTY, 7, (const gstep_t[]){GLI_14 GST("Tap REC to turn WRITE on", WR, EQ, 1), GST("Press 8 keys to place hits", HITS, UP, 8), GST("Tap ENV for accent mode", ACCM, EQ, 1),
        GST("Press 2 keys to mark steps", ACC, UP, 2), GST("Turn KNOB 1 to set tempo", BPM, CHG, 0), GST("Press PLAY to hear it", RUN, EQ, 1)}},
    {"SOUND LAB", GS_BEATQ, 5, (const gstep_t[]){GLI_9 GST("Tap HOME until LAB shows", VIEW, EQ, 1), GST("Turn ALGORITHM to flip page", PG, CHG, 0), GST("Turn KNOB 1 to tweak sound", KP, CHG, 0),
        GST("Press a key to hear it", KEYS, UP, 1)}},
    {"PATCH A AND B", GS_BEATQ, 7, (const gstep_t[]){GLI_10 GST("Tap HOME until LAB shows", VIEW, EQ, 1), GST("Tap SEL to edit patch B", SIDE, EQ, 1), GST("Turn a knob to shape B", KP, CHG, 0),
        GST("Tap SEL to return to A", SIDE, EQ, 0), GST("Tap HOME until PTN shows", VIEW, EQ, 0), GST("Turn KNOB 4 to morph A-B", K4, CHG, 0)}},
    {"ROLL A KIT", GS_BEATQ, 4, (const gstep_t[]){GLI_11 GST("Tap HOME until LAB shows", VIEW, EQ, 1), GST("Press OCT+ to open the kits", KITON, EQ, 1), GST("Press OCT+ to keep a kit", KITS, UP, 1)}},
    {"SOUND ENGINES", GS_BEATQ, 10, (const gstep_t[]){GLI_12 GST("Tap HOME until LAB shows", VIEW, EQ, 1), GST("Turn KNOB 1 for WAVE", KP, CHG, 0), GST("Turn ALGORITHM to MOD page", PG, EQ, 1),
        GST("Turn KNOB 3 for AMOUNT", KP, CHG, 0), GST("Turn ALGORITHM to NOISE", PG, EQ, 2), GST("Turn KNOB 1 for FILTER", KP, CHG, 0),
        GST("Turn ALGORITHM to OUT page", PG, EQ, 4), GST("Turn KNOB 1 for DIST", KP, CHG, 0), GST("Press a key to hear it", KEYS, UP, 1)}},
#include "lessons_guided.h"

#include "lessons_mc_1.h"
#include "lessons_mc_2.h"
#include "lessons_mc_3.h"
    {"SEND TO THE PO-32", GS_BEATQ, 4, (const gstep_t[]){GLI_13 GST("Tap HOME until SEND shows", VIEW, EQ, 2), GST("Press PLAY: a window opens", ASK, EQ, 1), GST("Press OCT- to cancel", ASK, EQ, 0)}},
};
static const char *const GL_PRAISE[9] = {"NICE!", "WELL DONE!", "WAY TO GO!", "GOOD!", "GOT IT!", "YES!", "SWEET!", "NAILED IT!", "EASY!"};
static void gl_patch(uint32_t mc, uint32_t id);                       /* the sound a masterclass chapter starts from (below) */
static void les_mark(uint32_t i);
static uint8_t gl_hide, gl_step, gl_ok, gl_pundo;
static uint32_t gl_kbase, gl_ok_t, gl_snap, gl_keys, gl_kits, gl_undos;
#define LES_ALL (NLESSON + NGL)
#define GL_A 2u                                                      /* the lessons written first (lessons_intro.h): the list shows them, then the copy-the-beat lessons, then the rest */
static uint32_t les_at(uint32_t p) { return p < GL_A ? NLESSON + p : p < GL_A + NLESSON ? p - GL_A : p; }       /* list position -> lesson */
static uint32_t les_pos(uint32_t i) { return i >= NLESSON && i - NLESSON < GL_A ? i - NLESSON : i < NLESSON ? GL_A + i : i; }
static uint32_t gl_active(void) { return les_on && les_i >= NLESSON; }
static uint32_t les_ismc(uint32_t i) { return i >= NLESSON && GLESSON[i - NLESSON].mc != 0u; }
static const char *les_name(uint32_t i) { return i < NLESSON ? lessons[i].name : GLESSON[i - NLESSON].name; }
static uint32_t gl_pop(uint32_t x) { uint32_t n = 0; while (x) { n += x & 1u; x >>= 1; } return n; }
static uint32_t gl_val(uint32_t v, uint32_t a)
{
    uint32_t l, s, n = 0, ax;
    switch (v) {
    case GV_VIEW: return view;
    case GV_WR: return wr;
    case GV_HITS: for (l = 0; l < 4u; l++) for (s = 0; s < 16u; s++) n += (pat[cur_pat].trig[l][s] & 15u) != 0u; return n;
    case GV_ACC: return gl_pop(pat[cur_pat].acc);
    case GV_ACCM: return accm;
    case GV_SEL: return sel;
    case GV_BPM: return bpm;
    case GV_SWING: return swing;
    case GV_K3: return knob[sel][0];
    case GV_K4: return knob[sel][1];
    case GV_CURPAT: return cur_pat;
    case GV_CHAINM: return chainm;
    case GV_CHAIN: return chain_n;
    case GV_MUTEM: return mutem;
    case GV_RUN: return sq_run;
    case GV_FX: return fx_layer;
    case GV_FXM: return fx_mask != 0u;
    case GV_KEYS: return gl_keys;
    case GV_UNDOS: return gl_undos;
    case GV_PG: return pg;
    case GV_SIDE: return side;
    case GV_KP: ax = 0; for (s = 0; s < NS; s++) for (l = 0; l < 2u; l++) for (n = 0; n < DR_PARAMS; n++) ax = ax * 31u + kp[s][l][n]; return ax & 0xFFFFu;
    case GV_KITS: return gl_kits;
    case GV_ASK: return send_ask;
    case GV_KITON: return kit_on;
    case GV_PA: return sel < NS ? kp[sel][0][a % DR_PARAMS] : 0u;
    case GV_PB: return sel < NS ? kp[sel][1][a % DR_PARAMS] : 0u;
    case GV_PCUR: return sel < NS ? kp[sel][side ? 1u : 0u][a % DR_PARAMS] : 0u;
    case GV_PQ: return sel < NS ? (uint32_t)kp[sel][side ? 1u : 0u][a % DR_PARAMS] >> 4 : 0u;                        /* a parameter in 12 bits: UP / DOWN n then mean about n / 70 clicks */
    case GV_PSEG: { const uint32_t x = sel < NS ? kp[sel][side ? 1u : 0u][a % DR_PARAMS] : 0u; return x < 21845u ? 0u : x < 43691u ? 1u : 2u; }       /* the third a switch-like parameter is in */
    case GV_LOCKP: for (l = 0; l < 4u; l++) for (s = 0; s < 16u; s++) n += (pat[cur_pat].trig[l][s] & 15u) && pat[cur_pat].lock[l][s][0] != PITCH_MID; return n;     /* steps with a pitch of their own */
    case GV_LOCKM: for (l = 0; l < 4u; l++) for (s = 0; s < 16u; s++) n += (pat[cur_pat].trig[l][s] & 15u) && pat[cur_pat].lock[l][s][1] != 0u; return n;          /* steps with a morph of their own */
    case GV_FXPAD: return fx_mask >> (a & 15u) & 1u;                                                                  /* an effect pad is down */
    case GV_FXREC: for (s = 0; s < 16u; s++) n += pat[cur_pat].fx[s] != 0u; return n;                               /* steps with an effect recorded */
    case GV_LOOP: return aud_loop;
    case GV_FINET: return gl_fineturns;                                                                                 /* parameter turns made with EDIT held */
    case GV_KITSEL: return kit_sel;
    }
    return 0;
}
static uint32_t gl_met(const gstep_t *st)
{
    uint32_t v = gl_val(st->v, st->a);
    switch (st->op) {
    case GO_EQ: return v == st->n;
    case GO_UP: return v >= gl_snap + st->n;
    case GO_DOWN: return v + st->n <= gl_snap;
    default: return v != gl_snap && (st->t[5] != 'K' || les_kclk - gl_kbase >= 4u);        /* a knob step wants a few clicks of a knob, not one */
    }
}
static uint8_t gl_arm = 1;
static void gl_snapshot(void)
{
    const gstep_t *st = &GLESSON[les_i - NLESSON].s[gl_step];
    gl_snap = gl_val(st->v, st->a); gl_kbase = les_kclk;
    gl_arm = !(st->op == GO_EQ && gl_snap == st->n);                  /* a step that is already true when it shows waits until it has been undone */
#ifdef CHIPBOX_HOST
    if (!gl_arm && st->v != GV_READ) { printf("GLESSON %u STEP %u INSTANT\n", (unsigned)(les_i - NLESSON), (unsigned)gl_step); fflush(stdout); }
#endif
}
static uint32_t gl_reading(void)                                    /* a page to read is up (GLO or HOME puts it away: gl_hide) */
{
    return gl_active() && !gl_hide && !les_done && !gl_ok && GLESSON[les_i - NLESSON].s[gl_step].v == GV_READ && !menu_on && !pick_on && !help_on && !kit_on;
}
static void gl_enter_step(void)                                     /* a step became the current one: a read page sets its sound up */
{
    const gstep_t *st = &GLESSON[les_i - NLESSON].s[gl_step];
    if (st->v == GV_READ && st->n) gl_patch(GLESSON[les_i - NLESSON].mc, st->n);
    dirty = 1;
}
static void gl_read_next(void)                                      /* OCT+ on a read page */
{
    const glesson_t *g = &GLESSON[les_i - NLESSON];
#ifdef CHIPBOX_HOST
    printf("GLESSON %u STEP %u OK\n", (unsigned)(les_i - NLESSON), (unsigned)gl_step); fflush(stdout);
#endif
    gl_ok = 0;
    if ((uint32_t)gl_step + 1u < g->n) { gl_step++; gl_enter_step(); gl_snapshot(); } else { les_done = 1; les_mark(les_i); }
    dirty = 1;
}
static void gl_begin(uint32_t i)                                    /* the scratch project for lesson i (les_enter has saved the player's) */
{
    const glesson_t *g = &GLESSON[i - NLESSON];
    uint32_t s;
    demo_kit(0);
    memset(pat, 0, sizeof pat);
    if (g->setup != GS_EMPTY && g->setup != GS_MC && g->setup != GS_PATCH) {
        dpat_t d;
        memset(&d, 0, sizeof d);
        for (s = 0; s < 16u; s += 4u) d.trig[0][s] = 1;                              /* kick on the beats */
        for (s = 4; s < 16u; s += 8u) d.trig[1][s] = 1;                              /* snare on 2 and 4 */
        for (s = 2; s < 16u; s += 4u) d.trig[2][s] = 1;                              /* hats off the beat */
        demo_pat(0, &d);
        if (g->setup == GS_CHAIN) { d.trig[3][6] = 1; d.trig[3][14] = 1; demo_pat(1, &d); }
    }
    chain_n = 0; cur_pat = 0; swing = 0; tempo_set(120);
    view = V_PAT; sel = 0; pg = 0; side = 0; wr = g->setup == GS_BEAT; accm = 0; mutem = 0; chainm = 0; fx_layer = 0; fx_latch = 0; aud_loop = 0; kit_on = 0;
    mute_mask = 0; solo_lane = -1; seq_stop(); gl_keys = 0; gl_kits = 0; gl_undos = 0; gl_pundo = 0;
    if (g->setup == GS_PATCH) gl_patch(99u, g->patch);
    gl_hide = 0; gl_step = 0; gl_ok = 0; les_done = 0; gl_snapshot(); gl_enter_step();
}
static void les_leave(void)                                         /* the player's project back, the panel at rest */
{
    if (!les_on) return;
    seq_stop(); les_exit();
    view = V_PAT; wr = 0; accm = 0; mutem = 0; chainm = 0; fx_layer = 0; fx_latch = 0; aud_loop = 0; kit_on = 0; side = 0;
    mute_mask = 0; solo_lane = -1; sel = 0; pg = 0; send_ask = 0;
    dirty = 1;
}
static void les_begin(uint32_t i)                                   /* start lesson i (ghost or guided) */
{
    if (les_on) les_leave();
    les_enter(i < NLESSON ? i : 0u);                                  /* (the scratch copy; a guided lesson overrides the contents) */
    les_i = (uint8_t)i;
    if (i >= NLESSON) gl_begin(i);
    else { view = V_PAT; wr = 1; chainm = 0; pg = 0; sel = 0; }
    home_used = 1; les_t0 = fm1_ms; dirty = 1;
}
static uint32_t gl_card(void) { return gl_active() && !gl_hide && !menu_on && !pick_on && !help_on && !kit_on && !gl_reading(); }
static uint32_t gl_over(void) { return gl_card() || gl_reading(); }          /* something of the lesson is painted over the view */
static void gl_tick(void)
{
    const glesson_t *g;
    if (!gl_active()) return;
    g = &GLESSON[les_i - NLESSON];
    if (gl_pundo && !undo_ok) gl_undos++;
    gl_pundo = undo_ok;
    if (gl_ok) {
        if (fm1_ms - gl_ok_t > 900u) {
            gl_ok = 0;
            if ((uint32_t)gl_step + 1u < g->n) { gl_step++; gl_enter_step(); } else { les_done = 1; les_mark(les_i); }
            dirty = 1;
        }
        return;
    }
    if (les_done) return;
    if (g->s[gl_step].v == GV_READ) return;                         /* a page to read: OCT+ goes on (gl_read_next) */
    if (!gl_arm && !gl_met(&g->s[gl_step])) gl_arm = 1;
    if (gl_arm && gl_met(&g->s[gl_step])) {
        gl_ok = 1; gl_ok_t = fm1_ms;
        if ((uint32_t)gl_step + 1u < g->n) { /* the next step counts what is done from now (the green moment too) */ uint8_t keep = gl_step; gl_step++; gl_snapshot(); gl_step = keep; }
#ifdef CHIPBOX_HOST
        printf("GLESSON %u STEP %u OK\n", (unsigned)(les_i - NLESSON), (unsigned)gl_step); fflush(stdout);
#endif
        dirty = 1;
    }
}

/* ---- the list: every lesson, a tick on the ones you finished (kept in the settings) */
static uint8_t les_list_on, les_saved;
static uint32_t les_ltop, les_saved_t;
static uint32_t les_isdone(uint32_t i) { return les_dn[i >> 3] >> (i & 7u) & 1u; }
static uint32_t les_ndone(void) { uint32_t i, n = 0; for (i = 0; i < LES_ALL; i++) n += les_isdone(i); return n; }
static void les_mark(uint32_t i)                                    /* finished: the settings keep it (saved a moment later, when it is quiet) */
{
    les_dn[i >> 3] |= (uint8_t)(1u << (i & 7u));
    memcpy(lset.les, les_dn, sizeof lset.les);
    les_saved = 1; les_saved_t = fm1_ms;
}
static void les_list_open(void)                                     /* it opens on the first lesson you have not done */
{
    uint32_t i = 0;
    while (i < LES_ALL && les_isdone(les_at(i))) i++;
    les_pick = i < LES_ALL ? i : 0u;
    les_list_on = 1;
}
static void les_list_input(uint32_t bdown)
{
    int32_t e = fm1_enc_take(ENC_SELECT) + fm1_enc_take(ENC_PRESETS) + fm1_enc_take(ENC_KNOB1);
    (void)fm1_enc_take(ENC_ALGORITHM); (void)fm1_enc_take(ENC_KNOB2); (void)fm1_enc_take(ENC_KNOB3); (void)fm1_enc_take(ENC_KNOB4);
    if (e) { int32_t v = (int32_t)les_pick + (e < -8 ? -8 : e > 8 ? 8 : e); les_pick = (uint32_t)(v < 0 ? 0 : v >= (int32_t)LES_ALL ? (int32_t)LES_ALL - 1 : v); }
    if (bdown & (1u << BTN_HOME | 1u << BTN_GLO)) { les_list_on = 0; menu_on = 0; home_used = 1; }
    else if (bdown >> BTN_OCT_UP & 1u) { les_list_on = 0; menu_on = 0; les_begin(les_at(les_pick)); }
    else if (bdown >> BTN_OCT_DOWN & 1u) les_list_on = 0;
    dirty = 1;
}

/* ---- masterclass patches: the sound and pattern a chapter (mc 99: a lesson's start) begins with (tools/masterclasses.py makes the table) */
typedef struct { uint8_t mc, id, sel, side, pg, view, bpm, pitch, morph, hasb, hasp; uint16_t a[DR_PARAMS], b[DR_PARAMS]; uint8_t trig[4][16]; uint16_t acc; } dlpatch_t;
static const dlpatch_t DLP[] = {
#include "lessons_mc_patches.h"
    {0}
};
static void gl_patch(uint32_t mc, uint32_t id)
{
    uint32_t i;
    for (i = 0; i + 1u < sizeof DLP / sizeof DLP[0]; i++) {
        const dlpatch_t *p = &DLP[i];
        uint32_t k;
        if (p->mc != mc || p->id != id) continue;
        seq_stop();
        for (k = 0; k < DR_PARAMS; k++) { kp[p->sel][0][k] = p->a[k]; kp[p->sel][1][k] = p->hasb ? p->b[k] : p->a[k]; }
        knob[p->sel][0] = p->pitch; knob[p->sel][1] = p->morph;
        if (p->hasp) { memset(&pat[0], 0, sizeof pat[0]); memcpy(pat[0].trig, p->trig, sizeof pat[0].trig); pat[0].acc = p->acc; for (k = 0; k < 64u; k++) { uint8_t *t = &pat[0].trig[k >> 4][k & 15u]; if (*t & 15u) { pat[0].lock[k >> 4][k & 15u][0] = PITCH_MID; pat[0].lock[k >> 4][k & 15u][1] = 0; } } cur_pat = 0; }
        if (p->bpm) tempo_set(p->bpm);
        sel = p->sel; side = p->side; pg = p->pg; view = p->view;
        break;
    }
    dirty = 1;
}
