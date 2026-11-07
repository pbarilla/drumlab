/* SPDX-License-Identifier: GPL-3.0-only */
/* Drumlab: a drum machine that works like the PO-32 Tonic and sends to one. The PO-32 wins in all things: its 4 columns, 16 steps, 16 patterns.
 *
 * PATTERN view
 *   16 white keys     the 4 x 4 grid: sounds 1-16 (the pads), or the 16 steps while WRITE is on
 *   PRESETS           the sound (1-16)         SELECT   the pattern (1-16)          HOME   tap: next view (PATTERN, SOUND, SEND); hold: projects
 *   KNOB 1 / 2        tempo / swing            KNOB 3 / 4   pitch (A) / morph (B) of the sound; with a step held in WRITE: that step's
 *   REC               tap: WRITE; held while playing: the keys punch in, KNOBS 3 / 4 write into the steps as they pass
 *   ENV               ACCENT (keys set accents)    ARP   MUTE, then SOLO (keys pick their column)    LFO   undo
 *   SEL               chain mode (keys add patterns, an EDIT tap takes the last off)
 *   FX                hold it and press keys: the 16 punch-in effects. A tap latches the layer. With WRITE on and playing, the effects are recorded
 *                     into the steps as they pass; FX held with no key clears them
 *   EDIT hold         clear the pattern (the chain, in chain mode)      OCT+   copy the pattern to the next
 *   PLAY              play / stop              SAVE   save      GLO   help      MASTER   volume
 * SOUND view          ALGORITHM = page, KNOBS 1-4 = its four parameters; the sound plays when a turn settles; the keys play the edited patch
 *                     SEL = patch A / B       EDIT held = fine knobs       PLAY = loop the sound       OCT+ = the kits window (roll a kit, LAB-1, the preset kits; OCT+ keep, OCT- discard)
 * SEND view           PLAY asks first (OCT+ Yes), then sends the whole pack to a PO-32 as audio; PLAY again stops
 * GLO + HOME          the menu: MIDI clock (off / send / follow), MIDI channel, lessons, battery, factory reset, about
 * Lessons             the menu's LESSONS row: OCT+ starts one (KNOB 1 picks it); ghost rings show where the hits go; HOME leaves, your project comes back
 * MIDI                notes 0-15 and 36-51 play the pads (and the pads send 36-51 on channel 10); CC 102 play / stop, 103 tempo / 2, 104 = 1 send,
 *                     program change = the pattern; clock out (24 a quarter note, start, stop) when MIDI CLOCK is SEND; follows an outside clock when FOLLOW
 *                     (the tempo turns cyan); test hooks CC 106 view | page << 2, CC 107 the send window; the device sends CC 100 audio %, 101 step | 16 playing,
 *                     105 slowest redraw ms, 108 / 109 battery reading
 * USB-MIDI SysEx      projects: 0x50 info, 0x51 read a slot, 0x52 write a slot (labio.c): the site's Projects page backs up and restores
 */
#ifndef CHIPBOX_HOST
#define DRUM_RAM
#define OTA_PROJ_IO                             /* projects over USB-MIDI (labio.c): backup and restore from the website */
#define LAB_CLK                                 /* usb.c counts the MIDI clock and transport (app only) */
#endif
#include "platform.c"
#ifdef CHIPBOX_HOST
#define FM1_TICKS_PER_US 24u
static uint32_t fm1_ticks(void) { return 0; }
#endif
#include "drum.c"
#include "po32tx.c"
#include "kit_lab1.h"
#ifdef FELUCCA_ID                                            /* firmware/ble/ble.c: the brothers' Bluetooth link, its own unit */
extern volatile uint8_t brothers_mode;                      /* 0 off, 1 lead, 2 follow */
extern void brothers_set(uint32_t mode);
extern void brothers_poll(void);
extern uint32_t brothers_link(void);
extern void brothers_lead_tick(uint32_t off, uint32_t per);
extern uint32_t brothers_bpm(void);
extern void brothers_lead_set(uint32_t pat, uint32_t kick, uint32_t fx, uint32_t scale, uint32_t swing);
extern uint32_t brothers_pat(void);
extern uint32_t brothers_fx(void);
extern uint32_t brothers_swing(void);
extern uint32_t brothers_duck(uint32_t depth);
extern void brothers_lead_run(uint32_t run);
#else
static uint8_t brothers_mode;
static void brothers_set(uint32_t mode) { brothers_mode = (uint8_t)mode; }
static void brothers_poll(void) {}
static uint32_t brothers_link(void) { return 0; }
static void brothers_lead_tick(uint32_t off, uint32_t per) { (void)off; (void)per; }
static uint32_t brothers_bpm(void) { return 0; }
static void brothers_lead_set(uint32_t pat, uint32_t kick, uint32_t fx, uint32_t scale, uint32_t swing) { (void)pat; (void)kick; (void)fx; (void)scale; (void)swing; }
static uint32_t brothers_pat(void) { return 0xFFu; }
static uint32_t brothers_fx(void) { return 0u; }
static uint32_t brothers_swing(void) { return 0u; }
static uint32_t brothers_duck(uint32_t depth) { (void)depth; return 256u; }
static void brothers_lead_run(uint32_t run) { (void)run; }
#endif
#include "labcore.c"
#include "labgen.c"
#include "labproj.c"
#include "labdemo.c"
#include "labio.c"
static po32tx_t tx;
static volatile uint32_t sending, cpu_max;

/* the whole pack for a PO-32: 32 patches, 16 patterns, then the state */
static int send_build(void)
{
    uint32_t s, p, i;
    uint8_t lk[4][16][2], st[16][2];
    po32tx_begin(&tx);
    for (s = 0; s < NS; s++) { po32tx_patch(&tx, (int)s, 0, kp[s][0]); po32tx_patch(&tx, (int)s, 1, kp[s][1]); }
    for (p = 0; p < NPAT; p++) {
        memcpy(lk, pat[p].lock, sizeof lk);
        po32tx_pattern(&tx, (int)p, pat[p].trig, (const uint8_t (*)[16][2])lk, pat[p].acc);
    }
    for (i = 0; i < NS; i++) { st[i][0] = knob[i][0]; st[i][1] = knob[i][1]; }
    if (chain_n) po32tx_state(&tx, st, bpm, swing, chain, chain_n);
    else { uint8_t one = cur_pat; po32tx_state(&tx, st, bpm, swing, &one, 1); }                   /* no chain: the pattern on show loops (a pack always has one entry or more) */
    return po32tx_finish(&tx);
}
static void send_start(void)
{
    seq_stop();
    if (send_build()) { po32tx_play(&tx); sending = 1; }
}

#include "labui.c"

static void app_audio(int32_t *out, uint32_t n)
{
    uint32_t t0 = fm1_ticks(), k, dt;
    if (n > 256u) n = 256u;
    if (sending) {
        int32_t tmp[256];
        sending = po32tx_render(&tx, tmp, (int)n, 20000) ? 1u : 0u;
        for (k = 0; k < n; k++) out[2u * k] = out[2u * k + 1u] = tmp[k];
    } else {
        core_audio(out, n);
        for (k = 0; k < 2u * n; k++) {                     /* a soft knee: four voices and the effects never wrap */
            int32_t x = out[k];
            if (x > 20000) x = 20000 + ((x - 20000) >> 2);
            else if (x < -20000) x = -20000 + ((x + 20000) >> 2);
            out[k] = x > 32767 ? 32767 : x < -32767 ? -32767 : x;
        }
    }
    dt = (uint32_t)(fm1_ticks() - t0);
    if (dt > cpu_max) cpu_max = dt;
}

static void app_main(void)
{
    uint32_t draw_max = 0, prev_keys = 0, prev_b = 0, cc_ms = 0, draw_ms = 0, led_ms = 0;
    core_init();
    proj_boot();
    ui_init();
    lcd_fill(0, 0, 240, 240, C_BLACK);
#if !defined(CHIPBOX_HOST) || defined(LAB_BOOT)                 /* (LAB_BOOT: the power-on picture in the host sim too) */
    {                                                           /* power on: the atom builds itself, then the name */
        uint32_t f, t0 = fm1_ms;
        lcd_fill(0, 0, 240, 240, K_BG);
        for (f = 0; fm1_ms - t0 < 1700u; f++) {
            uint32_t el = fm1_ms - t0;
            platform_poll();
            cv_begin(240, 124, K_BG);
            cv_ox = 0; cv_oy = -50;
            atom_cx = 120; atom_cy = 100; atom_k = el < 300u ? 1 : el < 700u ? 2 : 3; atom_pulse = el > 900u && el < 1000u ? 3 : 0;
            atom(0, K_BG, K_MINT, el * (el > 1100u ? 3u : 9u) / 2u);
            if (el > 900u) { uint32_t n = (el - 900u) / 90u; char nm[8] = "DRUMLAB"; if (n > 7u) n = 7u; nm[n] = 0; cv_text(120 - text_w(&FONT_L, "DRUMLAB") / 2, 124, &FONT_L, nm, K_CREAM); }
            cv_oy = 0;
            cv_blit(0, 40);
            { uint32_t w = fm1_ms; while (fm1_ms - w < 40u) platform_poll(); }
        }
        atom_cx = 23; atom_cy = 20; atom_k = 1; atom_pulse = 0;
        lcd_fill(0, 0, 240, 240, C_BLACK);
    }
#endif
    for (;;) {
        { static uint32_t paused;                                                                                                        /* the installer is talking: the Bluetooth radio goes quiet (it makes the FM-1 miss requests), and comes back 20 s after */
          if (ota_hit_ms && brothers_mode && !paused) { paused = brothers_mode; brothers_set(0); }
          if (paused && fm1_ms - ota_hit_ms > 20000u) { brothers_set(paused); paused = 0; ota_hit_ms = 0; } }
        if (ota_back) { ota_back = 0; dirty = 1; }                                                      /* the installer gave up and left its message on the screen: paint it all again */
        { static uint8_t bt_booted; if (!bt_booted && fm1_ms > 1500u) { bt_booted = 1; if (lset.flags & 3u) brothers_set(lset.flags & 3u); } }          /* the Bluetooth sync mode the player left it in */
        uint32_t keys, kdown, kup, b, bdown, bup, k;
        uint8_t st, d1, d2;
        platform_poll();
        keys = fm1_in.notes; kdown = keys & ~prev_keys; kup = prev_keys & ~keys; prev_keys = keys;
        { uint32_t rel; bdown = fm1_input_edges(&rel); bup = rel; b = fm1_in.buttons; prev_b = b; }
        ui_input(keys, kdown, kup, b, bdown, bup);
#ifndef CHIPBOX_HOST
        if (px_open_req) { uint32_t os = px_open_req - 1u; px_open_req = 0; if (os < LAB_SLOTS) { help_on = menu_on = pick_on = kit_on = 0; if (les_on) les_exit(); lab_open(os); view = V_PAT; sel = 0; pv_dirty = 1; pv_t = fm1_ms; dirty = 1; } }
#endif
        while (midi_get(&st, &d1, &d2)) {
            if (st < 0xF0u && lset.chan && (st & 0x0Fu) != (uint32_t)(lset.chan - 1u)) continue;                 /* another channel */
            if ((st & 0xF0u) == 0xC0u && d1 < 16u) { cur_pat = d1; dirty = 1; continue; }                           /* program change: the pattern */                        /* tests over USB-MIDI: note on d1 0..15 = a pad; CC 102 play / stop, CC 103 tempo / 2 */
            if ((st & 0xF0u) == 0x90u && d2 && d1 < 16u) pad_hit(d1, 0);
            if ((st & 0xF0u) == 0x90u && d2 && d1 >= 36u && d1 < 52u) pad_hit(d1 - 36u, d2 > 110u);       /* a drum machine's notes: 36 up */
            if ((st & 0xF0u) == 0xB0u && d1 == 102u) { if (d2 >= 64u) seq_play(); else seq_stop(); }
            if ((st & 0xF0u) == 0xB0u && d1 == 119u) { seq_stop(); bt_demo_load(); bt_on = 0; menu_on = 0; view = V_PAT; wr = 1; chainm = 0; pg = 0; sel = 0; pv_dirty = 1; dirty = 1; }       /* test: the Bluetooth demo */
            if ((st & 0xF0u) == 0xB0u && d1 == 103u) tempo_set(d2 * 2u);
            if ((st & 0xF0u) == 0xB0u && d1 == 104u && d2 == 1u) send_start();
            if ((st & 0xF0u) == 0xB0u && d1 == 106u) { view = (uint8_t)((d2 & 3u) % NVIEW); pg = (uint8_t)((d2 >> 2) % 6u); help_on = pick_on = send_ask = 0; dirty = 1; pv_dirty = 1; pv_t = fm1_ms; }      /* test: jump to a view (+ 4 x page) */
            if ((st & 0xF0u) == 0xB0u && d1 == 107u) { send_ask = d2 ? 1u : 0u; dirty = 1; }                                   /* test: the send window */
        }
        if ((sync_mode == 2u || brothers_mode == 2u) && fol_on && sq_run && sq_len) { uint32_t nb = (uint32_t)(((uint64_t)661500u * 256u) / sq_len); nb = nb < 30u ? 30u : nb > 255u ? 255u : nb; if (nb != bpm) { bpm = (uint8_t)nb; dsec |= SEC_HDR | SEC_CTRL; } }
        { static uint8_t fo; if (fo != fol_on) { fo = fol_on; dsec |= SEC_HDR; } }
        if (fm1_ms - draw_ms >= 33u) { uint32_t d0 = fm1_ms; draw_ms = fm1_ms; ui_draw(); if (fm1_ms - d0 > draw_max) draw_max = fm1_ms - d0; }
        if (fm1_ms - led_ms >= 30u) { led_ms = fm1_ms; ui_leds(); }
        if (fm1_ms - cc_ms >= 500u) {                             /* CC 100 = the worst audio block in % (since the last), CC 101 = the playing step | 16 playing */
            uint32_t w = cpu_max / (58u * FM1_TICKS_PER_US);
            cpu_max = 0; cc_ms = fm1_ms;
            midi_send(0xB0, 100, w > 127u ? 127u : w);
            midi_send(0xB0, 101, (sq_run ? 16u : 0u) | cur_step());
            midi_send(0xB0, 105, draw_max > 127u ? 127u : draw_max); draw_max = 0;
            midi_send(0xB0, 108, bat_raw / 8u > 127u ? 127u : bat_raw / 8u); midi_send(0xB0, 109, bat_raw % 8u);                     /* the slowest screen redraw, ms */
        }
        (void)k;
    }
}
