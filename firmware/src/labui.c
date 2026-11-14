/* SPDX-License-Identifier: GPL-3.0-only */
/* Drumlab's controls and screens. Everything that decides how it looks is in `SK` (colours) and the draw_* functions; the logic never draws.
 * The screen is painted in three bands of 96 rows (the canvas is at most 240 x 96); coordinates are screen coordinates (cv_ox, cv_oy shift them).
 * A full repaint takes ~90 ms on the FM-1, so it happens only when something changed; the playhead and the sound map are small blits. */

enum { V_PAT, V_SND, V_SEND, NVIEW };
#define BAT_EMPTY 495u                      /* the battery divider's reading (10 bit) at 3.3 V and at 4.2 V: calibrated on the bench */
#define BAT_FULL 650u
static const uint8_t WHITE[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};      /* the white keys, F3 up */

/* ---- the sound's parameters, in pages of four knobs */
typedef struct { const char *name; uint8_t idx; } pm_t;        /* idx 0..20: a patch parameter; 100 pitch, 101 morph, 255 none */
static const struct { const char *title; pm_t p[4]; } PG[6] = {
    {"OSC",   {{"WAVE", 0}, {"FREQ", 1}, {"ATTACK", 2}, {"DECAY", 3}}},
    {"MOD",   {{"MODE", 4}, {"RATE", 5}, {"AMOUNT", 6}, {"VELOC", 20}}},
    {"NOISE", {{"FILTER", 7}, {"FREQ", 8}, {"Q", 9}, {"VELOC", 19}}},
    {"ENV",   {{"SHAPE", 10}, {"ATTACK", 11}, {"DECAY", 12}, {"MIX", 13}}},
    {"OUT",   {{"DIST", 14}, {"EQ HZ", 15}, {"EQ DB", 16}, {"LEVEL", 17}}},
    {"SOUND", {{"VELOC", 18}, {"PITCH", 100}, {"MORPH", 101}, {"", 255}}},
};
static const char *const DISC[4][3] = {{"SIN", "TRI", "SAW"}, {"DECAY", "SINE", "NOISE"}, {"LP", "BP", "HP"}, {"EXP", "LIN", "MOD"}};
static int disc_of(uint32_t idx) { return idx == 0 ? 0 : idx == 4 ? 1 : idx == 7 ? 2 : idx == 10 ? 3 : -1; }

/* ---- state */
enum { MR_CLOCK, MR_CHAN, MR_LESSONS, MR_TIPS, MR_QS, MR_PICK, MR_BAT, MR_DEMO, MR_RESET, MR_BT, MR_ABOUT, MR_GSET, MR_GSYNC, MR_GRST };      /* the menu's rows (GLO + HOME) */
#define NMENU 14
static uint8_t menu_grp;                                                                        /* 0 the top, 1 Settings, 2 Sync and Link, 3 Reset */
static uint32_t menu_rows(uint8_t *r)                                                           /* the rows of the open group (RESTORE DEMOS only once the demo was deleted) */
{
    uint32_t n = 0;
    if (menu_grp == 0u) { r[n++] = MR_LESSONS; r[n++] = MR_GSET; r[n++] = MR_GSYNC; r[n++] = MR_GRST; }
    else if (menu_grp == 1u) { r[n++] = MR_TIPS; r[n++] = MR_QS; r[n++] = MR_PICK; r[n++] = MR_BAT; r[n++] = MR_ABOUT; }
    else if (menu_grp == 2u) { r[n++] = MR_CLOCK; r[n++] = MR_CHAN; r[n++] = MR_BT; }
    else { if (demo_gone()) r[n++] = MR_DEMO; r[n++] = MR_RESET; }
    return n;
}
static uint8_t qs_pick;                                         /* Projects At Start: the picker opens when the quick start is put away (or at once when there is none) */
static uint8_t view, sel, pg, side, wr, accm, mutem, chainm, fx_layer, aud_loop;     /* mutem: 0 off, 1 mute, 2 solo */
static uint8_t fx_lamp;
static uint16_t p1_tog; static uint8_t p1_lat;                  /* the effects layer, page 1: the pads latched on, the latch */
static uint8_t bt_on, bt_sel;                                  /* the Bluetooth page of the menu: Sync, Sidechain, Patterns */
static uint8_t pick_on, pick_sel, del_ask, help_on, send_ask, menu_on, menu_sel, reset_ask, fx_latch, fx_held, kseq, kit_on, kit_sel, about_on, po_mode;      /* send_ask: the window that asks before the noise */
static int8_t held_step = -1;
static uint8_t pv_play = 1;
static uint32_t dirty = 1, map_dirty, last_step = 99, dsec;       /* dirty: repaint everything; dsec: only these sections */
enum { SEC_HDR = 1, SEC_BODY = 2, SEC_CTRL = 4, SEC_TABS = 8 };
static uint32_t kit_seed, les_pick, les_t0;
static char kit_title[24];
static uint16_t kb_patch[NS][2][DR_PARAMS];                 /* the kit as it was, while a new one is on trial */
static uint8_t kb_knob[NS][2], kb_snm[NS];
#define NKL (NKITDEF + 2)                                    /* the list: a rolled kit, LAB-1, then the recipes */
static uint32_t bat_raw, bat_pct, bat_ms, pick_t, pick_aud, pick_i = 8, pick_next, fx_t0, set_dirty;
static uint32_t aud_next, pad_t[16], pad_on, flash_ms, save_ok, edit_t0, edit_used, pv_t, pv_dirty = 1, pv_ok, flash_drawn;
static uint32_t home_t0, home_used, rec_t0, rec_used, lock_rec, lock_step = 99;
static uint16_t pv_peak[224];                                      /* the scope: the sound's loudness over its length */
static lpat_t undo_pat;                                            /* LFO: one step back for the pattern on show */
static uint8_t undo_idx, undo_ok;
static uint32_t undo_t;

static void bat_poll(void)
{
    int32_t v;
    if (fm1_ms - bat_ms < 2000u && bat_raw) return;
    bat_ms = fm1_ms;
#ifndef CHIPBOX_HOST
    v = fm1_adc_read(FM1_ADC_BATT);
#else
    v = 600;
#endif
    if (v < 0) return;
    bat_raw = bat_raw ? (bat_raw * 3u + (uint32_t)v) / 4u : (uint32_t)v;
    bat_pct = bat_raw <= BAT_EMPTY ? 0u : bat_raw >= BAT_FULL ? 100u : (bat_raw - BAT_EMPTY) * 100u / (BAT_FULL - BAT_EMPTY);
}

static uint32_t cur_step(void) { return sq_run ? (uint32_t)((sq_step + 15u) & 15u) : 0u; }

static void hit_ev(uint32_t s, uint32_t accent, uint32_t pitch, uint32_t morph)
{
    uint32_t w = evq_w;
    if ((uint8_t)(w - evq_r) >= 8u) return;
    evq[w & 7u].s = (uint8_t)s; evq[w & 7u].acc = (uint8_t)accent; evq[w & 7u].pitch = (uint8_t)pitch; evq[w & 7u].morph = (uint8_t)morph;
    evq_w = (uint8_t)(w + 1u);
}
static void pad_hit(uint32_t s, uint32_t accent) { hit_ev(s, accent, knob[s][0], knob[s][1]); }
static void aud_hit(uint32_t s) { hit_ev(s, 0, knob[s][0], side ? 255u : 0u); }       /* the sound as it is being edited: patch A or B, not the morph */

/* an edit of the pattern on show: a quiet moment before it, and LFO can take it back */
static void pat_edit(void)
{
    if (fm1_ms - undo_t > 900u) { undo_pat = pat[cur_pat]; undo_idx = cur_pat; undo_ok = 1; }
    undo_t = fm1_ms;
    lab_edited();
}
static void step_set(uint32_t s, uint32_t st)                 /* sound s on step st (a column holds one sound a step: this one wins) */
{
    lpat_t *q = &pat[cur_pat];
    uint32_t lane = s & 3u, grp = s >> 2;
    q->trig[lane][st] = (uint8_t)(1u | grp << 4); q->lock[lane][st][0] = knob[s][0]; q->lock[lane][st][1] = knob[s][1];
}
static void step_toggle(uint32_t s, uint32_t st)
{
    lpat_t *q = &pat[cur_pat];
    uint32_t lane = s & 3u, grp = s >> 2, t = q->trig[lane][st];
    pat_edit();
    if ((t & 15u) && ((t >> 4) & 3u) == grp) { q->trig[lane][st] = 0; q->lock[lane][st][0] = PITCH_MID; q->lock[lane][st][1] = 1; }
    else { step_set(s, st); pad_hit(s, (q->acc >> st) & 1u); }
}
static uint32_t step_has(uint32_t s, uint32_t st)             /* the pattern on show plays sound s on step st */
{
    uint32_t t = pat[cur_pat].trig[s & 3u][st];
    return (t & 15u) && ((t >> 4) & 3u) == (s >> 2);
}

/* a knob turn on a parameter: discrete ones step to the next third; the others move ~1.7 % a click (~5 % turned fast), a tenth of that while EDIT is held (fine) */
static uint32_t fine;
static void param_turn(uint32_t idx, int32_t d)
{
    uint16_t *v;
    int32_t x;
    if (!d || idx == 255u) return;                                    /* 255: the empty slot of the SOUND page */
    if (idx == 100u || idx == 101u) {
        uint8_t *k = &knob[sel][idx - 100u];
        if (held_step >= 0 && wr && step_has(sel, (uint32_t)held_step)) k = &pat[cur_pat].lock[sel & 3u][held_step][idx - 100u];
        x = (int32_t)*k + d * (fine ? (d > 3 || d < -3 ? 2 : 1) : (d > 3 || d < -3 ? 14 : 4));
        *k = (uint8_t)(x < 0 ? 0 : x > 255 ? 255 : x);
        dsec |= SEC_CTRL;
        lab_edited();
        return;
    }
    v = &kp[sel][side][idx];
    if (disc_of(idx) >= 0) {
        int32_t seg = *v < 21845u ? 0 : *v < 43691u ? 1 : 2;
        static const uint16_t MID[3] = {10923u, 32768u, 54613u};
        seg += d > 0 ? 1 : -1;
        *v = MID[seg < 0 ? 0 : seg > 2 ? 2 : seg];
    } else {
        x = (int32_t)*v + d * (fine ? (d > 3 || d < -3 ? 330 : 110) : (d > 3 || d < -3 ? 3400 : 1100));
        *v = (uint16_t)(x < 0 ? 0 : x > 65535 ? 65535 : x);
    }
    if (fine) gl_fineturns++;
    pv_dirty = 1; pv_t = fm1_ms; dsec |= SEC_CTRL;
    lab_edited();
}

/* ---- the scope: the sound rendered quietly in the main loop, a few slots at a time, a moment after the last turn */
static drum_t pv_v;
static uint16_t pv_slot[1024];
static uint32_t pv_n, pv_run;
static void pv_start(void)
{
    uint16_t p[DR_PARAMS];
    uint32_t i;
    for (i = 0; i < DR_PARAMS; i++) p[i] = kp[sel][side][i];
    drum_trig(&pv_v, p, 127);
    for (i = 0; i < 1024u; i++) pv_slot[i] = 0;
    pv_n = 0; pv_run = 1;
}
static void pv_step(void)
{
    int32_t blk[64];
    uint32_t c, k, i, on = 1;
    for (c = 0; c < 16u && on && pv_n < 1024u; c++) {                  /* 64 samples a slot, 1.5 s at most */
        uint32_t m = 0;
        on = (uint32_t)drum_render_fp(&pv_v, blk, 64);
        for (k = 0; k < 64u; k++) { int32_t a = blk[k] < 0 ? -blk[k] : blk[k]; if ((uint32_t)a > m) m = (uint32_t)a; }
        pv_slot[pv_n++] = (uint16_t)(m >> 5);
    }
    if (on && pv_n < 1024u) return;
    for (i = 0; i < 224u; i++) {                                     /* stretch the slots over 224 columns */
        uint32_t a = i * pv_n / 224u, b = (i + 1u) * pv_n / 224u, m = 0;
        if (b <= a) b = a + 1u;
        for (k = a; k < b && k < pv_n; k++) if (pv_slot[k] > m) m = pv_slot[k];
        pv_peak[i] = (uint16_t)m;
    }
    pv_ok = 1; pv_run = 0; dsec |= SEC_BODY;
}

static void scene_man(void);
static void draw_tip(void);
static uint8_t tip_on;
#include "lablesson.c"
#include "labdraw.c"
#include "labman.c"
#include "labtips.c"

static void do_save(void)
{
    save_ok = lab_save(lab_slot) == 0; flash_ms = fm1_ms;
    if (save_ok) lab_dirty = 0;
    dirty = 1;
}



/* ---- the Bluetooth page (GLO + HOME, BLUETOOTH): the sync and what rides on it. The mode and the switches are kept (lset.flags) */
static void bt_save(void) { lset.flags = (uint8_t)(brothers_mode | bt_side << 2 | bt_patsync << 3 | bt_fxsync << 4); set_dirty = 1; }
static void bt_input(uint32_t bdown)
{
    int32_t e = fm1_enc_take(ENC_SELECT), d;
    if (!e) e = fm1_enc_take(ENC_PRESETS);
    (void)fm1_enc_take(ENC_KNOB1); (void)fm1_enc_take(ENC_ALGORITHM); (void)fm1_enc_take(ENC_KNOB2); (void)fm1_enc_take(ENC_KNOB3); (void)fm1_enc_take(ENC_KNOB4);
    dirty = 1;
    if (bdown & (1u << BTN_HOME | 1u << BTN_GLO)) { bt_on = 0; if (set_dirty && !sq_run) { settings_save(&lset, sizeof lset); set_dirty = 0; } return; }
    if (e) bt_sel = (uint8_t)(((int32_t)bt_sel + 5 + e % 5) % 5);
    d = (int32_t)(bdown >> BTN_OCT_UP & 1u) - (int32_t)(bdown >> BTN_OCT_DOWN & 1u);
    if (!d) return;
    if (bt_sel == 0u) brothers_set(((uint32_t)brothers_mode + 3u + (uint32_t)d) % 3u);
    else if (bt_sel == 1u) bt_side ^= 1u;
    else if (bt_sel == 2u) bt_patsync ^= 1u;
    else if (bt_sel == 3u) bt_fxsync ^= 1u;
    else if (d > 0) { seq_stop(); bt_demo_load(); bt_on = 0; menu_on = 0; view = V_PAT; wr = 1; chainm = 0; pg = 0; sel = 0; home_used = 1; pv_dirty = 1; dirty = 1; return; }       /* DEMO: the locked project */
    bt_save();
}

/* ---- the system menu (GLO + HOME): MIDI clock, MIDI channel, the battery, factory reset, about */
static void menu_input(uint32_t bdown)
{
    int32_t e, d;
    (void)fm1_enc_take(ENC_ALGORITHM); (void)fm1_enc_take(ENC_KNOB2); (void)fm1_enc_take(ENC_KNOB3); (void)fm1_enc_take(ENC_KNOB4);
    dirty = 1;
    if (bt_on) { bt_input(bdown); return; }
    if (about_on) { if (bdown & (1u << BTN_OCT_UP | 1u << BTN_OCT_DOWN | 1u << BTN_HOME | 1u << BTN_GLO | 1u << BTN_PLAY)) about_on = 0; (void)fm1_enc_take(ENC_SELECT); (void)fm1_enc_take(ENC_PRESETS); return; }
    if (reset_ask) {                                                    /* a window: the buttons may be named inside it */
        if (bdown >> BTN_OCT_UP & 1u) {
            uint32_t k;
            seq_stop();
            for (k = 0; k < LAB_SLOTS; k++) lab_clear(k);
            lset.pad[1] = DG_OK; core_init(); demo_install(); lab_slot = 0; lab_loaded = lab_load(0) == 0; lab_dirty = 0; lab_slot_note(); reset_ask = 0; menu_on = 0; view = V_PAT; sel = 0; pv_dirty = 1;
            man_open(MC_QUICK, 0u); man_lock = 1; home_used = 1;                                     /* like a new device: the quick start again */
        } else if (bdown & (1u << BTN_OCT_DOWN | 1u << BTN_HOME | 1u << BTN_GLO)) reset_ask = 0;
        (void)fm1_enc_take(ENC_SELECT); (void)fm1_enc_take(ENC_PRESETS);
        return;
    }
    if (bdown & (1u << BTN_HOME | 1u << BTN_GLO)) {                     /* leave: what changed is kept (inside a group: HOME goes up a level first) */
        if (menu_grp) { menu_sel = (uint8_t)(MR_GSET + menu_grp - 1u); menu_grp = 0; (void)fm1_enc_take(ENC_SELECT); (void)fm1_enc_take(ENC_PRESETS); return; }
        menu_on = 0; home_used = 1;
        if (set_dirty && !sq_run) { settings_save(&lset, sizeof lset); set_dirty = 0; }
        (void)fm1_enc_take(ENC_SELECT); (void)fm1_enc_take(ENC_PRESETS);
        return;
    }
    {   uint8_t rows[8];
        const uint32_t rn = menu_rows(rows);
        uint32_t at = 0, k;
        for (k = 0; k < rn; k++) if (rows[k] == menu_sel) at = k;
        e = fm1_enc_take(ENC_SELECT); if (!e) e = fm1_enc_take(ENC_PRESETS);
        if (e) { int32_t n = e < 0 ? -e : e; if (n > (int32_t)rn) n = (int32_t)rn; while (n-- > 0) at = (at + rn + (e < 0 ? -1u : 1u)) % rn; }
        menu_sel = rows[at];
    }
    d = (int32_t)(bdown >> BTN_OCT_UP & 1u) - (int32_t)(bdown >> BTN_OCT_DOWN & 1u);
    if (d) {
        if (menu_sel >= MR_GSET && d > 0) { uint8_t rows[8]; menu_grp = (uint8_t)(menu_sel - MR_GSET + 1u); menu_rows(rows); menu_sel = rows[0]; return; }       /* a group */
        if (menu_sel == MR_LESSONS && d > 0) { les_list_open(); return; }
        if (menu_sel == MR_CLOCK) { lset.sync = (uint8_t)((lset.sync + 3 + d) % 3); sync_mode = lset.sync; set_dirty = 1; }
        else if (menu_sel == MR_CHAN) { lset.chan = (uint8_t)((lset.chan + 17 + d) % 17); set_dirty = 1; }
        else if (menu_sel == MR_TIPS) { lset.pad[0] ^= QS_NOTIPS; set_dirty = 1; }                      /* the switches: either button flips the row */
        else if (menu_sel == MR_QS) { lset.pad[0] ^= QS_ALWAYS; set_dirty = 1; }
        else if (menu_sel == MR_PICK) { lset.pad[0] ^= QS_PICK; set_dirty = 1; }
        else if (menu_sel == MR_DEMO && d > 0) {                                                       /* RESTORE DEMOS: the deleted demo comes back (the open project is kept) */
            seq_stop(); if (lab_dirty) { lab_save(lab_slot); lab_dirty = 0; }
            if (demo_can_restore()) {                                                                  /* (project 1 must be empty: a project of the player's is never overwritten) */
                demo_restore();
                if (lab_load(lab_slot) != 0) core_init();
                lab_loaded = lab_exists(lab_slot); lab_dirty = 0; pv_dirty = 1; set_dirty = 0;
                menu_sel = MR_RESET;                                                                   /* (the row hides itself again) */
            }
        }
        else if (menu_sel == MR_RESET && d > 0) reset_ask = 1;
        else if (menu_sel == MR_BT && d > 0) { bt_on = 1; bt_sel = 0; }                                /* BROTHERS LINK: its page */
        else if (menu_sel == MR_ABOUT && d > 0) about_on = 1;
    }
}

/* ---- the project picker hears the project it points at: it is loaded into the core, a few of its sounds play, and the open project
 * comes back when the picker closes without choosing (an unsaved open project is saved when the picker opens) */
static uint8_t pick_core = 0xFFu;
static void pick_show(uint32_t slot)
{
    if (lab_load(slot) != 0) core_init();
    pick_core = (uint8_t)slot; pick_i = 0; pick_next = fm1_ms;
}
static void pick_leave(void)
{
    if (pick_core != 0xFFu && pick_core != lab_slot) { if (lab_load(lab_slot) != 0) core_init(); }
    pick_core = 0xFFu; pick_aud = 0; pick_i = 8;
}


/* ---- kits: a window with a rolled kit (ROLL), LAB-1 and the preset kits; each is heard at once, OCT+ keeps it, OCT- puts the old kit back */
static void kit_apply(void)
{
    const char *n;
    char *e = kit_title;
    if (kit_sel == 0u) { kit_roll(kit_seed); kit_roll_t = fm1_ms; atom_boost += 50u; }
    else if (kit_sel == 1u) kit_lab1();
    else kit_preset(kit_sel - 2u);
    kit_name_of(kit_seed, kit_title);
    if (kit_sel == 1u) { n = "LAB-1"; for (e = kit_title; *n; ) *e++ = *n++; *e = 0; }
    else if (kit_sel > 1u) { n = kit_def[kit_sel - 2u].name; for (e = kit_title; *n; ) *e++ = *n++; *e = 0; }
    pick_i = 0; pick_next = fm1_ms + 40u; dirty = 1;
}
static void kit_open(void)
{
    memcpy(kb_patch, kp, sizeof kp); memcpy(kb_knob, knob, sizeof knob); memcpy(kb_snm, snm, sizeof snm);
    kit_on = 1; kit_sel = 0; kit_seed = (fm1_ms * 2654435761u >> 12) & 0xFFFFu; if (!kit_seed) kit_seed = 1;
    seq_stop();
    kit_apply();
}
static void kit_close(int keep)
{
    kit_on = 0; pick_i = 8;
    if (keep) { lab_edited(); gl_kits++; }
    else { memcpy(kp, kb_patch, sizeof kp); memcpy(knob, kb_knob, sizeof knob); memcpy(snm, kb_snm, sizeof snm); }
    pv_dirty = 1; pv_t = fm1_ms; pv_play = 0; dirty = 1;
}
static void kit_input(uint32_t kdown, uint32_t bdown)
{
    int32_t e;
    uint32_t i;
    (void)fm1_enc_take(ENC_ALGORITHM); (void)fm1_enc_take(ENC_KNOB1); (void)fm1_enc_take(ENC_KNOB2); (void)fm1_enc_take(ENC_KNOB3); (void)fm1_enc_take(ENC_KNOB4);
    dirty = 1;
    e = fm1_enc_take(ENC_SELECT); if (!e) e = fm1_enc_take(ENC_PRESETS);
    if (e) { kit_sel = (uint8_t)(((int32_t)kit_sel + (int32_t)NKL + e % (int32_t)NKL) % (int32_t)NKL); if (kit_sel == 0u) kit_seed = (kit_seed * 40503u + 12345u) & 0xFFFFu; kit_apply(); }
    if ((bdown >> BTN_PLAY & 1u)) { if (kit_sel == 0u) { kit_seed = (kit_seed * 40503u + fm1_ms) & 0xFFFFu; if (!kit_seed) kit_seed = 1; } kit_apply(); }
    for (i = 0; i < 16u; i++) if (kdown >> WHITE[i] & 1u) { sel = (uint8_t)i; pad_hit(i, 0); }
    if (bdown >> BTN_OCT_UP & 1u) kit_close(1);
    else if (bdown & (1u << BTN_OCT_DOWN | 1u << BTN_HOME | 1u << BTN_GLO)) { kit_close(0); if (bdown >> BTN_HOME & 1u) home_used = 1; }
    if (pick_i < 8u && fm1_ms >= pick_next) { static const uint8_t AS[8] = {0, 2, 1, 2, 0, 2, 1, 3}; pad_hit(AS[pick_i++], 0); pick_next = fm1_ms + 125u; }
}

static void pick_open_now(void)                                     /* the projects: what is unsaved is saved, the picker hears the one it points at */
{
    seq_stop(); if (lab_dirty) { lab_save(lab_slot); lab_dirty = 0; }
    pick_on = 1; pick_sel = lab_slot; del_ask = 0; help_on = 0; man_lock = 0; fx_layer = 0; fx_latch = 0; pick_aud = 1; pick_t = fm1_ms; dirty = 1;
}
static void ui_input(uint32_t keys, uint32_t kdown, uint32_t kup, uint32_t b, uint32_t bdown, uint32_t bup)
{
    uint32_t i;
    int32_t e;
    if (bdown | bup | kdown | kup) { act_ms = fm1_ms; if (bdown | bup) dirty = 1; }
    if (!help_on) man_lock = 0;                                          /* (something else closed the manual: the quick start is over) */

    if (send_ask) {                                                     /* the window over SEND: OCT+ sends; OCT-, PLAY or HOME put it away */
        if (bdown >> BTN_OCT_UP & 1u) { send_ask = 0; send_start(); }
        else if (bdown & (1u << BTN_OCT_DOWN | 1u << BTN_PLAY | 1u << BTN_HOME)) { send_ask = 0; if (bdown >> BTN_HOME & 1u) home_used = 1; }
        (void)fm1_enc_take(ENC_SELECT); (void)fm1_enc_take(ENC_PRESETS); (void)fm1_enc_take(ENC_ALGORITHM);
        (void)fm1_enc_take(ENC_KNOB1); (void)fm1_enc_take(ENC_KNOB2); (void)fm1_enc_take(ENC_KNOB3); (void)fm1_enc_take(ENC_KNOB4);
        dirty = 1;
        return;
    }

    if (menu_on) { if (les_list_on) les_list_input(bdown); else menu_input(bdown); return; }
    if (les_on && !gl_active() && (bdown >> BTN_HOME & 1u)) { les_exit(); home_used = 1; view = V_PAT; wr = 0; dirty = 1; return; }                      /* HOME leaves a lesson */
    if (les_on && les_done && (bdown >> BTN_OCT_UP & 1u)) { les_begin(les_at((les_pos(les_i) + 1u) % LES_ALL)); return; }
    if (gl_reading()) {                                                                                                                          /* a masterclass page: OCT+ goes on, HOME puts it away */
        if (bdown >> BTN_OCT_UP & 1u) { gl_read_next(); return; }
        if (bdown >> BTN_HOME & 1u) { gl_hide = 1; home_used = 1; dirty = 1; return; }
    }
    if (les_on && les_done && (bdown >> BTN_OCT_DOWN & 1u)) { les_exit(); home_used = 1; view = V_PAT; wr = 0; dirty = 1; return; }       /* the last card: OCT- ends it */
    if (kit_on) { kit_input(kdown, bdown); return; }
    if ((!les_on || gl_active()) && (b >> BTN_GLO & 1u) && (b >> BTN_HOME & 1u) && (bdown & (1u << BTN_GLO | 1u << BTN_HOME))) {
        les_leave();        /* GLO + HOME, either first: the menu */
        menu_on = 1; les_list_on = 0; menu_grp = 0; menu_sel = MR_LESSONS; reset_ask = 0; help_on = 0; man_lock = 0; qs_pick = 0; home_used = 1; fx_layer = 0; fx_latch = 0; dirty = 1;
        if (pick_on) { pick_on = 0; pick_leave(); }
        return;
    }

    /* HOME: a tap is the next view (the picker's: open), a hold is the projects (the picker's: close) */
    if (bdown >> BTN_HOME & 1u) { home_t0 = fm1_ms; home_used = tip_on; }                                   /* (a tip is up: HOME only puts it away) */
    if (!les_on && (b >> BTN_HOME & 1u) && !home_used && fm1_ms - home_t0 > 600u) {
        home_used = 1;
        if (pick_on) { pick_on = 0; pick_leave(); } else pick_open_now();
        dirty = 1;
    }
    if (help_on && man_lock && (bdown >> BTN_OCT_DOWN & 1u)) {                                      /* the quick start, OCT-: hide it (it will not come back by itself) */
        help_on = 0; man_lock = 0; lset.pad[0] &= (uint8_t)~QS_ALWAYS; set_dirty = 1; dirty = 1;
        if (qs_pick) { qs_pick = 0; pick_open_now(); }
        (void)fm1_enc_take(ENC_SELECT); return;
    }
    if (help_on && (bup >> BTN_HOME & 1u) && !home_used) {
        help_on = 0; home_used = 1; dirty = 1;
        if (man_lock) { set_dirty = 1; man_lock = 0; if (qs_pick) { qs_pick = 0; pick_open_now(); } }                 /* the quick start is put away: the projects, if the switch says so */
    }
    if ((bup >> BTN_HOME & 1u) && !home_used) {
        if (pick_on) {
            if (del_ask) del_ask = 0;
            else { lab_open(pick_sel); pick_core = 0xFFu; pick_on = 0; view = V_PAT; sel = 0; pv_dirty = 1; pv_t = fm1_ms; }
        } else { view = (uint8_t)((view + 1u) % NVIEW); chainm = 0; fx_layer = 0; fx_latch = 0; aud_loop = 0; help_on = 0; }
        dirty = 1;
    }
    if ((bdown >> BTN_GLO & 1u) && gl_active() && !help_on) { gl_hide ^= 1u; dirty = 1; }
    else if (bdown >> BTN_GLO & 1u) { if (man_lock) {} else if (!help_on) man_open(man_for_screen(), 0u); else man_toc ^= 1u; dirty = 1; }       /* (GLO does nothing inside the quick start) */
    if (help_on) { man_input(bdown); return; }
    if (pick_on) {
        if ((e = fm1_enc_take(ENC_SELECT)) != 0) { pick_sel = (uint8_t)((pick_sel + 12 + (e % 12)) % 12u); dirty = 1; pick_aud = 1; pick_t = fm1_ms; }
        if ((e = fm1_enc_take(ENC_PRESETS)) != 0) { pick_sel = (uint8_t)((pick_sel + 12 + (e % 12)) % 12u); dirty = 1; pick_aud = 1; pick_t = fm1_ms; }
        if (pick_aud && fm1_ms - pick_t > 140u) { pick_aud = 0; pick_show(pick_sel); }
        if (pick_i < 8u && fm1_ms >= pick_next) { static const uint8_t AS[8] = {0, 2, 1, 2, 0, 2, 1, 3}; pad_hit(AS[pick_i++], 0); pick_next = fm1_ms + 125u; }
        if (bdown >> BTN_EDIT & 1u) { edit_t0 = fm1_ms; edit_used = 0; }
        if ((b >> BTN_EDIT & 1u) && !edit_used && !del_ask && fm1_ms - edit_t0 > 600u && lab_exists(pick_sel)) { edit_used = 1; del_ask = 1; dirty = 1; }
        if (del_ask && (bdown >> BTN_OCT_UP & 1u)) {
            lab_clear(pick_sel);
            if (pick_sel == 0u && demo0) {                                                  /* the demo goes for good: it is not made again at start */
                demo0 = 0; lset.pad[1] = DG_OK | DG_GONE; settings_save(&lset, sizeof lset);               /* (its slot is an ordinary one from now on) */
            }
            if (pick_sel == lab_slot) { core_init(); lab_dirty = 0; }
            pick_aud = 1; pick_t = fm1_ms;
            del_ask = 0; dirty = 1;
        }
        if (del_ask && (bdown >> BTN_OCT_DOWN & 1u)) { del_ask = 0; dirty = 1; }
        (void)fm1_enc_take(ENC_ALGORITHM); (void)fm1_enc_take(ENC_KNOB1); (void)fm1_enc_take(ENC_KNOB2); (void)fm1_enc_take(ENC_KNOB3); (void)fm1_enc_take(ENC_KNOB4);
        return;
    }

    if (bdown >> BTN_PLAY & 1u) {
        if (view == V_SEND) { if (sending) sending = 0; else { send_ask = 1; ask_t = fm1_ms; } }
        else if (view == V_SND) { aud_loop ^= 1u; aud_next = fm1_ms; }                  /* LAB: PLAY loops the sound, to tweak it while it repeats */
        else if (sq_run) seq_stop(); else seq_play();
    }
    if (bdown >> BTN_REC & 1u) { rec_t0 = fm1_ms; rec_used = 0; }                       /* REC: a tap is WRITE; held while playing, the keys punch in */
    if ((bup >> BTN_REC & 1u) && !rec_used && fm1_ms - rec_t0 < 400u) wr ^= 1u;
    if (bdown >> BTN_ENV & 1u) accm ^= 1u;
    if (bdown >> BTN_ARP & 1u) { mutem = (uint8_t)((mutem + 1u) % 3u); if (mutem != 2u) solo_lane = -1; }
    if (bdown >> BTN_SEL & 1u) { if (view == V_SND) { side ^= 1u; pv_dirty = 1; pv_t = fm1_ms; } else chainm ^= 1u; }
    if ((bdown >> BTN_SAVE & 1u) && !sq_run && !sending && !les_on) do_save();
    if ((bdown >> BTN_LFO & 1u) && undo_ok) {                                           /* LFO: undo */
        pat[undo_idx] = undo_pat; undo_ok = 0; cur_pat = undo_idx; lab_edited();
    }
    if ((bdown >> BTN_OCT_UP & 1u) && view == V_SND) kit_open();
    if ((bdown >> BTN_OCT_UP & 1u) && view == V_PAT && !fx_layer) { uint32_t nx = (cur_pat + 1u) & 15u; pat_edit(); pat[nx] = pat[cur_pat]; cur_pat = (uint8_t)nx; }
    if (bdown >> BTN_EDIT & 1u) { edit_t0 = fm1_ms; edit_used = 0; }
    fine = view == V_SND && (b >> BTN_EDIT & 1u);                                       /* LAB: EDIT held = fine knobs */
    if ((b >> BTN_EDIT & 1u) && view == V_PAT && !edit_used && fm1_ms - edit_t0 > 600u) {          /* hold: the pattern is cleared (the chain too in chain mode) */
        edit_used = 1;
        if (chainm) chain_n = 0; else { pat_edit(); memset(&pat[cur_pat], 0, sizeof pat[cur_pat]); }
        lab_edited(); dirty = 1;
    }
    if (chainm && (bup >> BTN_EDIT & 1u) && !edit_used && chain_n) { chain_n--; lab_edited(); }       /* a tap in chain mode: take the last one off */

    if ((e = fm1_enc_take(ENC_PRESETS)) != 0) { sel = (uint8_t)((sel + 16 + (e % 16)) & 15u); pv_dirty = 1; pv_t = fm1_ms; dirty = 1; }
    if ((e = fm1_enc_take(ENC_SELECT)) != 0) { cur_pat = (uint8_t)((cur_pat + 16 + (e % 16)) & 15u); dirty = 1; }
    if ((e = fm1_enc_take(ENC_ALGORITHM)) != 0 && view == V_SND) { pg = (uint8_t)((pg + 6 + (e % 6)) % 6u); dsec |= SEC_BODY | SEC_CTRL; }
    {
        int32_t k1 = fm1_enc_take(ENC_KNOB1), k2 = fm1_enc_take(ENC_KNOB2), k3 = fm1_enc_take(ENC_KNOB3), k4 = fm1_enc_take(ENC_KNOB4);
        if (k1 | k2 | k3 | k4) dsec |= view == V_SND || !k1 ? SEC_CTRL : SEC_CTRL | SEC_HDR;
        if ((k3 | k4) && sq_run && (b >> BTN_REC & 1u) && view == V_PAT) { lock_rec = 1; rec_used = 1; }          /* REC held while playing: the knobs write into the steps as they pass */
        if (view == V_SND) {
            param_turn(PG[pg].p[0].idx, k1); param_turn(PG[pg].p[1].idx, k2); param_turn(PG[pg].p[2].idx, k3); param_turn(PG[pg].p[3].idx, k4);
        } else {
            if (k1) { int32_t t2 = (int32_t)bpm + k1 * (k1 > 3 || k1 < -3 ? 4 : 1); tempo_set((uint32_t)(t2 < 60 ? 60 : t2 > 240 ? 240 : t2)); lab_edited(); }
            if (k2) { int32_t s2 = (int32_t)swing + k2; swing = (uint8_t)(s2 < 0 ? 0 : s2 > 12 ? 12 : s2); lab_edited(); }
            param_turn(100, k3); param_turn(101, k4);
        }
    }
    if (!(b >> BTN_REC & 1u) || !sq_run) lock_rec = 0;
    if (lock_rec) {
        uint32_t st = cur_step();
        if (st != lock_step && step_has(sel, st)) { pat[cur_pat].lock[sel & 3u][st][0] = knob[sel][0]; pat[cur_pat].lock[sel & 3u][st][1] = knob[sel][1]; lab_edited(); }
        lock_step = st;
    }
    if (bdown >> BTN_FX & 1u) { fx_t0 = fm1_ms; fx_held = 1; }
    if (bup >> BTN_FX & 1u) { fx_held = 0; if (fm1_ms - fx_t0 < 300u && view == V_PAT) fx_latch ^= 1u; }          /* a tap opens the layer, another tap puts it away; held, it is the layer while you hold it */
    { uint8_t f = (uint8_t)(view == V_PAT && (fx_held || fx_latch)); if (f != fx_layer) dirty = 1; fx_layer = f; }
    if (fx_layer && view != V_SEND) {                                  /* the punch-in layer: a key held is its effect on */
        uint32_t mk = 0, fm;
        static uint32_t pmk;
        for (i = 0; i < 16u; i++) if (keys >> WHITE[i] & 1u) mk |= 1u << i;
        if (brothers_mode) {                                           /* Brothers Link is on: the shared pads, seven of the PO-32's, the latch (pad 16: a tap turns a pad on and the next tap off) */
            uint32_t dn = mk & ~pmk, pad, held = mk & 0x7FFFu;
            for (pad = 0; pad < 15u; pad++) if (p1_lat && (dn >> pad & 1u)) p1_tog ^= 1u << pad;
            if (dn >> 15 & 1u) { p1_lat ^= 1u; if (!p1_lat) p1_tog = 0; }
            held = p1_lat ? p1_tog : held;
            bfx_local = (uint8_t)(held & 0xFFu);
            fm = 0;
            for (pad = 0; pad < 7u; pad++) if (held >> (8u + pad) & 1u) fm |= 1u << FX_PAD1[pad];
            mk = fm | (p1_lat ? 1u << 16 : 0u);                          /* (bit 16: the latch lamp) */
        } else { bfx_local = 0; p1_lat = 0; p1_tog = 0; }              /* Brothers Link is off: the PO-32's own 16 effects, pad n is effect n (mk is the keys held) */
        pmk = keys ? (uint32_t)0 : 0u;
        { uint32_t k2 = 0; for (i = 0; i < 16u; i++) if (keys >> WHITE[i] & 1u) k2 |= 1u << i; pmk = k2; }
        { static uint8_t lbl; if (bfx_local != lbl) { lbl = bfx_local; dirty = 1; } }          /* (pads 1-8 lit too) */
        if ((mk & 0xFFFFu) != fx_mask || (uint8_t)(mk >> 16) != fx_lamp) dirty = 1;
        fx_lamp = (uint8_t)(mk >> 16);
        fx_mask = (uint16_t)mk;
        kdown = 0; kup = 0;
    } else { if (fx_mask || bfx_local) dirty = 1; fx_mask = 0; bfx_local = 0; p1_lat = 0; p1_tog = 0; fx_lamp = 0; fx_layer = fx_layer && view != V_SEND; }
    if (fx_layer && wr && sq_run && sq_pat == cur_pat) {              /* WRITE on: the PO-32's effects on the pads land on the step that plays (the shared ones are not recorded) */              /* WRITE on: the effects land on the step that plays; FX held with no key clears */
        uint32_t st = cur_step();
        if (fx_mask) { if ((pat[cur_pat].fx[st] | fx_mask) != pat[cur_pat].fx[st]) { pat[cur_pat].fx[st] |= fx_mask; lab_edited(); dsec |= SEC_BODY; } }
        else if (fx_held && !fx_latch && pat[cur_pat].fx[st]) { pat[cur_pat].fx[st] = 0; lab_edited(); dsec |= SEC_BODY; }
    }
    for (i = 0; i < 16u; i++) {
        uint32_t kb = 1u << WHITE[i];
        if (kup & kb) { if (held_step == (int8_t)i) held_step = -1; }
        if (!(kdown & kb)) continue;
        dirty = 1; gl_keys++;
        if (view == V_PAT && !wr && !chainm && !accm && mutem == 0u && !les_on && !fx_layer) {
            static const uint8_t SQ[4] = {0, 3, 12, 15};
            if (i == SQ[kseq]) kseq++; else kseq = (uint8_t)(i == SQ[0] ? 1u : 0u);
            if (kseq >= 4u) { kseq = 0; po_mode ^= 1u; atom_boost = 90u; }
        }
        if (view == V_PAT && chainm) { if (chain_n < CHAIN_MAX) { chain[chain_n++] = (uint8_t)i; lab_edited(); } }
        else if (view == V_PAT && mutem == 1u) mute_mask ^= (uint16_t)(1u << (i & 3u));
        else if (view == V_PAT && mutem == 2u) solo_lane = solo_lane == (int)(i & 3u) ? -1 : (int8_t)(i & 3u);
        else if (view == V_PAT && accm) { pat_edit(); pat[cur_pat].acc ^= (uint16_t)(1u << i); }
        else if (view == V_PAT && sq_run && (b >> BTN_REC & 1u)) {                       /* punch in: the sound lands on the step nearest the playhead */
            uint32_t ps = cur_step(), at = sq_phase * 2u >= sq_len ? (ps + 1u) & 15u : ps;
            rec_used = 1; pat_edit(); sel = (uint8_t)i; step_set(i, at); pad_hit(i, 0);
        }
        else if (view == V_PAT && wr) { step_toggle(sel, i); held_step = (int8_t)i; }
        else if (view == V_SND) { sel = (uint8_t)i; aud_hit(i); pv_dirty = 1; pv_t = fm1_ms; pv_play = 0; }
        else { sel = (uint8_t)i; pad_hit(i, 0); pv_dirty = 1; pv_t = fm1_ms; }
    }
    {
        uint32_t hits = sq_hit;                                       /* what the audio side played */
        if (hits) {
            sq_hit = 0;
            for (i = 0; i < 16u; i++) if (hits >> i & 1u) { pad_t[i] = fm1_ms; pad_on |= 1u << i; midi_send(0x99, 36u + i, 100u); }
            atom_hit_t = fm1_ms; if (atom_boost < 40u) atom_boost += 8u;                                       /* the atom feels it */
            map_dirty = 1;
        }
        if (pad_on) { for (i = 0; i < 16u; i++) if ((pad_on >> i & 1u) && fm1_ms - pad_t[i] > 170u) { pad_on &= ~(1u << i); map_dirty = 1; } }
    }
    if (view == V_SND && pv_dirty && fm1_ms - pv_t > 250u) { pv_dirty = 0; pv_start(); if (pv_play) aud_hit(sel); pv_play = 1; }
    if (view == V_SND && pv_run) pv_step();       /* a moment after the last turn: the sound plays, as it now is */
    if (view == V_SND && aud_loop && fm1_ms >= aud_next) { aud_hit(sel); aud_next = fm1_ms + 60000u / bpm; }
    { uint32_t o = bat_pct; bat_poll(); if (o != bat_pct) dsec |= SEC_HDR; }
    gl_tick();
    if (les_on && !gl_active()) { uint32_t h, n; les_count(&h, &n); if (n && h == n && !les_done) { les_done = 1; les_mark(les_i); les_t0 = fm1_ms; dirty = 1; } else if (!les_done && fm1_ms - les_t0 > 400u) { les_t0 = fm1_ms; dsec |= SEC_BODY; } lab_dirty = 0; }
    if (les_saved && !sq_run && fm1_ms - les_saved_t > 2000u) { settings_save(&lset, sizeof lset); les_saved = 0; }
    if (gl_active()) lab_dirty = 0;
    if (lab_dirty && !sq_run && !sending && !pad_on && fm1_ms - lab_t_edit > 3000u) { do_save(); lab_t_edit = fm1_ms; }
}
