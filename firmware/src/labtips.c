/* SPDX-License-Identifier: GPL-3.0-only */
/* Tips: when nobody has touched the FM-1 for a while and nothing plays or sends, a card of lab paper shows the next tip, the next one every 20 s,
 * until a touch. Any touch (or anything starting) puts it away; HOME only puts it away. Like Chipbox's tips.c, in Drumlab's look.
 * Included by labui.c after labman.c (it uses the manual's text wrapping and the QR codes). */
typedef struct { const char *text; int8_t qr; } tip_t;       /* qr: an index into QR_ROW, or -1 */
static const tip_t TIPS[] = {
    {"Press GLO on any screen to open its help.", -1},
    {"Press GLO and HOME together to open the menu.", -1},
    {"Turn these tips off in the menu: press GLO and HOME together.", -1},
    {"Read the manual:", QR_GUIDE},
    {"Hold HOME to open your projects. Turn SELECT to pick one, then tap HOME.", -1},
    {"Hold EDIT on the demo in the projects to delete it. Its slot is yours then.", -1},
    {"Press REC to write steps. The 16 keys become the 16 steps.", -1},
    {"In WRITE, hold a step and turn KNOB 3 or 4 to lock its pitch or morph.", -1},
    {"Press ENV, then a step, to accent it.", -1},
    {"Turn KNOB 1 for the tempo and KNOB 2 for swing.", -1},
    {"Press SEL for CHAIN, then press keys to add patterns. Up to 64.", -1},
    {"Press OCT+ to copy this pattern to the next one.", -1},
    {"Hold EDIT to clear the pattern. Press LFO to undo.", -1},
    {"Press ARP to mute a column, and ARP again to solo it.", -1},
    {"Sounds 1, 5, 9 and 13 share a column: a new hit cuts the old one.", -1},
    {"Hold REC while it plays and press the keys to punch sounds in.", -1},
    {"Hold FX and press the keys for the PO-32's effects. Tap FX to keep them on.", -1},
    {"With WRITE on while it plays, the effects you punch in are written into the steps.", -1},
    {"In LAB, turn ALGORITHM to flip pages. KNOB 1 to 4 shape the sound.", -1},
    {"In LAB, press SEL to swap between patch A and patch B.", -1},
    {"In LAB, hold EDIT while you turn a knob to move in tiny steps.", -1},
    {"In LAB, press PLAY to loop the sound while you tweak it.", -1},
    {"In LAB, press OCT+ for kits. Press PLAY there to roll the dice.", -1},
    {"On SEND, press PLAY and then OCT+ to send your beat to a PO-32.", -1},
    {"Press ACC and SOUND on your PO-32 so it listens before you send.", -1},
    {"Pick LESSONS in the menu and press OCT+ to learn a beat step by step.", -1},
    {"Bring your Microtonic kits and patterns in:", QR_PROJ},
    {"Set MIDI CLOCK to Send or Follow in the menu to sync your other gear.", -1},
    {"Open BROTHERS LINK in the menu to play along with Chipbox and Smileyface.", -1},
    {"Press SAVE to save now. Stop editing for a few seconds and it saves by itself.", -1},
    {"Play it online:", QR_PLAY},
    {"Report a bug here:", QR_GIT},
    {"Get updates here:", QR_SITE},
};
#define NTIPS (sizeof TIPS / sizeof TIPS[0])
#ifndef TIP_IDLE
#define TIP_IDLE 300000u                         /* untouched for 5 minutes */
#endif
#ifndef TIP_EVERY
#define TIP_EVERY 20000u                         /* the next tip, in the same card */
#endif
static uint8_t tip_i;                            /* (tip_on is in labui.c) */
static uint32_t tip_t0;

static uint32_t tip_quiet(void)                  /* nothing is going on that a card would be in the way of */
{
    return !sq_run && !sending && !help_on && !menu_on && !pick_on && !kit_on && !les_on && !send_ask && !reset_ask && !del_ask && !about_on && !bt_on && !fx_layer && !fx_held;
}
static void tip_open(void)
{
    tip_i = (uint8_t)((tip_i + (tip_on ? 1u : 0u)) % NTIPS);        /* (the first one on the first time, then each next) */
    tip_on = 1; tip_t0 = fm1_ms; dirty = 1;
}
static void tip_close(void)
{
    if (!tip_on) return;
    tip_on = 0; tip_i = (uint8_t)((tip_i + 1u) % NTIPS); dirty = 1;
}
static void tip_tick(void)                       /* the main loop, after the input */
{
    if (tip_on) {
        if (act_ms >= tip_t0 + 1u || !tip_quiet()) tip_close();             /* a touch, or anything starting */
        else if (fm1_ms - tip_t0 >= TIP_EVERY) { tip_i = (uint8_t)((tip_i + 1u) % NTIPS); tip_t0 = fm1_ms; dirty = 1; }
    } else if (!(lset.pad[0] & QS_NOTIPS) && fm1_ms - act_ms > TIP_IDLE && tip_quiet()) tip_open();
}
static void draw_tip(void)                       /* over the body and the knob row: the picture dims, a card of lab paper sits on it */
{
    const tip_t *t = &TIPS[tip_i];
    int32_t x = 8, w = 224, y, h, tx = x + 12, tw = w - 24;
    tint(0, 44, 240, 172, C_BLACK, 150);
    if (t->qr >= 0) {
        uint32_t r, q;
        const int32_t qx = 4, qw = 232, bx = qx + qw - 6 - (QR_N + 4) * 4;      /* a wider card; the code at the right: 4 px a module and a quiet zone of two */
        tx = qx + 10; tw = bx - tx - 4; h = 22 + (QR_N + 4) * 4 + 12; y = 44 + (172 - h) / 2;
        rrect_o(qx, y, qw, h, 6, K_CORAL, K_CREAM);
        cv_rect(qx + 3, y + 3, qw - 6, 17, K_NAVY);
        txt(qx + 12, y + 5, "TIP", K_CREAM);
        man_lines(tx, y + 28, t->text, tw);
        cv_rect(bx, y + 22, (QR_N + 4) * 4, (QR_N + 4) * 4, RGB(255, 255, 255));
        for (r = 0; r < QR_N; r++) for (q = 0; q < QR_N; q++)
            if (QR_ROW[t->qr][r] >> (QR_N - 1u - q) & 1u) cv_rect(bx + 8 + (int32_t)q * 4, y + 30 + (int32_t)r * 4, 4, 4, RGB(0, 0, 0));
        txt(qx + 10, y + h - 18, "HOME  Close", K_NAVY);
        return;
    }
    man_dry = 1; h = man_lines(0, 0, t->text, tw); man_dry = 0;                 /* the card is as tall as its words */
    h += 22 + 8 + 22; y = 44 + (172 - h) / 2;
    rrect_o(x, y, w, h, 6, K_CORAL, K_CREAM);
    cv_rect(x + 3, y + 3, w - 6, 17, K_NAVY);
    txt(x + 12, y + 5, "TIP", K_CREAM);
    man_lines(tx, y + 30, t->text, tw);
    txt(x + w - 12 - text_w(&FONT_S, "HOME  Close"), y + h - 18, "HOME  Close", K_NAVY);
}
