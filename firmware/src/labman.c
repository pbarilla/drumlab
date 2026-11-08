/* SPDX-License-Identifier: GPL-3.0-only */
/* The manual (GLO): the same pages as Chipbox's and Smileyface's, in Drumlab's lab-notebook look. A page is the FM-1 with the controls to use
 * ringed, a topic and a few lines. GLO opens the chapter for the screen you are on; SELECT turns pages (on into the next chapter), PRESETS
 * jumps chapters, GLO again is the contents, HOME leaves. `help_on` is "the manual is open". Included by labui.c after labdraw.c. */
enum {
    M_MASTER = 1u << 0, M_SELECT = 1u << 1, M_PRESETS = 1u << 2, M_ALGO = 1u << 3, M_K1 = 1u << 4, M_K2 = 1u << 5, M_K3 = 1u << 6, M_K4 = 1u << 7,
    M_OCTD = 1u << 8, M_OCTU = 1u << 9, M_FX = 1u << 10, M_SEL = 1u << 11, M_ENV = 1u << 12, M_LFO = 1u << 13, M_EDIT = 1u << 14, M_GLO = 1u << 15,
    M_HOME = 1u << 16, M_SAVE = 1u << 17, M_ARP = 1u << 18, M_SEQ = 1u << 19, M_PLAY = 1u << 20, M_REC = 1u << 21, M_KEYS = 1u << 22,
    M_KNOBS = M_K1 | M_K2 | M_K3 | M_K4,
    M_SMALL = 1u << 24, M_NOPANEL = 1u << 25                                    /* the picture small (long text), no picture */
};
typedef struct { const char *topic; uint32_t ring; const char *text; } mpage_t;
typedef struct { const char *name; uint16_t col; const mpage_t *page; uint8_t n; } mchap_t;

static const mpage_t MP_START[] = {
    {"Play", M_KEYS | M_PLAY, "Press a key to hear a sound. Press PLAY to start the beat."},
    {"Views", M_HOME, "Tap HOME to flip between PTN, LAB and SEND. Hold HOME for your projects."},
    {"Save", M_SAVE, "Drumlab saves by itself a few seconds after you stop editing. Press SAVE to be sure."},
    {"Help", M_GLO, "Press GLO on any screen for its help. Turn SELECT to read on. Press HOME to leave."},
};
static const mpage_t MP_PTN[] = {
    {"Sounds", M_PRESETS | M_KEYS, "Turn PRESETS to pick one of the 16 sounds. The 16 keys are pads that play them."},
    {"Write", M_REC | M_KEYS, "Press REC to turn WRITE on. The 16 keys are now the 16 steps. Press a step again to take it away."},
    {"Patterns", M_SELECT, "Turn SELECT to pick one of the 16 patterns."},
    {"Accent", M_ENV | M_KEYS, "Press ENV to turn ACCENT on. Press a step to accent it, and again to take it away."},
    {"Mute", M_ARP | M_KEYS, "Press ARP to mute, again to solo. Press a key to pick its column. Sounds 1, 5, 9 and 13 share one."},
    {"Chain", M_SEL | M_KEYS | M_EDIT, "Press SEL for CHAIN. Press keys to add patterns, up to 64. Tap EDIT to take the last one off."},
    {"Tempo", M_K1 | M_K2, "Turn KNOB 1 for the tempo, 60 to 240. Turn KNOB 2 for swing."},
    {"Pitch", M_K3 | M_K4, "KNOB 3 is pitch and KNOB 4 morphs A to B. In WRITE, hold a step and turn them to lock them to it."},
    {"Live", M_REC | M_PLAY | M_KEYS, "Hold REC while it plays and press the keys. The hits land on the step nearest the playhead."},
    {"Tidy", M_OCTU | M_EDIT | M_LFO, "Press OCT+ to copy this pattern to the next one. Hold EDIT to clear it. Press LFO to undo the last change."},
};
static const mpage_t MP_LAB[] = {
    {"Hear", M_KEYS | M_PRESETS, "Press a key to hear the sound you are shaping. Turn PRESETS to pick another sound."},
    {"Pages", M_ALGO | M_KNOBS, "Turn ALGORITHM to flip pages. KNOB 1 to 4 turn its four settings. The sound plays when you stop."},
    {"A and B", M_SEL, "Every sound is two patches, A and B. Press SEL to swap. KNOB 4 on PTN morphs between them."},
    {"Loop", M_PLAY, "Press PLAY to loop the sound while you tweak it."},
    {"Fine", M_EDIT | M_KNOBS, "Hold EDIT while you turn a knob to move in tiny steps."},
    {"Kits", M_OCTU | M_PLAY | M_OCTD, "Press OCT+ for kits. Press PLAY to roll the dice. OCT+ keeps the kit, OCT- puts the old one back."},
};
static const mpage_t MP_SND[] = {
    {"OSC", M_ALGO | M_KNOBS | M_SMALL, "The oscillator is the body of the sound. WAVE picks a sine, a triangle or a saw. FREQ is its pitch in Hz. ATTACK fades it in. DECAY sets how long it rings."},
    {"MOD", M_ALGO | M_KNOBS | M_SMALL, "MOD bends the oscillator's pitch. MODE: DECAY slides it, SINE wobbles it, NOISE jitters it. RATE sets the speed."},
    {"AMOUNT", M_ALGO | M_KNOBS | M_SMALL, "AMOUNT sets how far MOD bends. Up from the middle, the pitch starts high and falls to FREQ. Down from the middle, it starts low and rises. VELOC lets harder hits bend more."},
    {"NOISE", M_ALGO | M_KNOBS | M_SMALL, "NOISE is a burst of static. FILTER: LP keeps the lows, BP a band, HP the highs. FREQ is where it filters. Q sharpens it. VELOC lets harder hits make more noise."},
    {"ENV", M_ALGO | M_KNOBS | M_SMALL, "ENV shapes the noise. SHAPE: EXP falls away, LIN ramps down, MOD flutters like a clap. ATTACK and DECAY set its length. MIX blends oscillator (left) and noise (right)."},
    {"OUT", M_ALGO | M_KNOBS | M_SMALL, "DIST turns the sound from clean to crushed. EQ HZ picks a frequency and EQ DB boosts or cuts it up to 40 dB. The middle is flat. LEVEL is the volume."},
    {"SOUND", M_ALGO | M_KNOBS | M_SMALL, "VELOC lets harder hits make the oscillator louder. PITCH moves the whole sound up or down 12 semitones. MORPH slides from patch A to patch B."},
    {"Kick", M_ALGO | M_KNOBS | M_SMALL, "OSC: a sine, FREQ 45 to 60, a long DECAY. MOD: DECAY, AMOUNT well up so it starts high and falls. MIX: a little noise for the click. OUT: a touch of DIST for punch."},
    {"Snare", M_ALGO | M_KNOBS | M_SMALL, "OSC: a triangle around 180 Hz, a short DECAY. NOISE: HP or BP at about 2K. ENV: a medium DECAY. MIX: toward the noise, about 60."},
    {"Hat", M_ALGO | M_KNOBS | M_SMALL, "MIX all the way to noise. NOISE: HP at 8K or more. ENV: a very short DECAY for a closed hat, a longer one for an open hat."},
    {"Clap", M_ALGO | M_KNOBS | M_SMALL, "MIX all noise. NOISE: BP at about 1.2K with some Q. ENV: SHAPE MOD and a medium DECAY. Turn DECAY to change the flutter."},
    {"Tom", M_ALGO | M_KNOBS | M_SMALL, "OSC: a sine around 120 to 200 Hz, a medium DECAY. MOD: DECAY with a gentle AMOUNT. MIX all oscillator."},
    {"Zap", M_ALGO | M_KNOBS | M_SMALL, "OSC: a saw high up, a long DECAY. MOD: DECAY with a big AMOUNT. For metal, try MOD in SINE mode at a fast RATE, then add DIST."},
};
static const mpage_t MP_FX[] = {
    {"Punch in", M_FX | M_KEYS | M_SMALL, "Tap FX to open the effects, and tap it again to put them away. Or hold FX while you press keys. The 16 keys are 16 pads. Hold one for its effect."},
    {"PO-32", M_FX | M_KEYS | M_SMALL, "The 16 pads are the PO-32's effects: HALF, DIST, SQUASH, ECHO, P-LFO, EQ SW, MORPH, BEND, PUNCH, 6/8, REPEAT, REP 2, FM, GRAIN, REVRSE and BOUNCE. With WRITE on while it plays, they are written into the steps."},
    {"Brothers Link", M_FX | M_KEYS | M_SMALL, "With Brothers Link on, the pads change. Pads 1 to 8 are STOP, REPEAT, LPF, HPF, ECHO, REVERB, CRUSH and GATE. They are the same on Chipbox and Smileyface, and they travel to the others."},
    {"Link pads", M_FX | M_KEYS | M_SMALL, "Pads 9 to 15 are HALF, DIST, P-LFO, BEND, GRAIN, REVRSE and FM, and are written into the steps. Pad 16 is the latch: with it on, tap a pad to keep it on, and tap it again to let it go."},
};
static const mpage_t MP_SEND[] = {
    {"Cable", M_PLAY | M_SMALL, "Plug a cable from the FM-1's output into your PO-32's input and turn MASTER up. Press ACC and SOUND on the PO-32 so it listens."},
    {"Send", M_PLAY | M_OCTU, "Press PLAY, then OCT+ to say yes. All 16 sounds and 16 patterns go over, with the tempo and the swing."},
    {"Stop", M_PLAY | M_OCTD | M_SMALL, "Press OCT- to back out before it starts. Do not stop a send halfway: it can leave the PO-32 with a broken pack."},
};
static const mpage_t MP_PROJ[] = {
    {"Open", M_HOME | M_SELECT | M_SMALL, "Hold HOME for your 12 projects. Turn SELECT to pick one. Tap HOME to open it. Hold HOME to leave without opening."},
    {"Delete", M_EDIT | M_OCTU | M_SMALL, "Hold EDIT on a project to delete it. Press OCT+ to say yes. Project 1 is a demo and stays."},
    {"Demo", M_SELECT | M_SMALL, "Project 1 is a locked demo. Play it, tweak it, then open another project. Nothing in it is saved. A new project starts from the LAB-1 kit."},
};
static const mpage_t MP_MENU[] = {
    {"Menu", M_GLO | M_HOME | M_SMALL, "Press GLO and HOME together to open the menu. Turn SELECT to pick a row. Press OCT+ or OCT- to change it. HOME closes it."},
    {"MIDI Clock", M_OCTU | M_SMALL, "Press OCT+ or OCT- to switch between Off, Send and Follow. Send gives your tempo to other gear. Follow takes theirs."},
    {"MIDI Channel", M_OCTU | M_SMALL, "Press OCT+ or OCT- to pick ANY, or a channel from 1 to 16. Anything on other channels is ignored. The pads send notes 36 to 51 on channel 10."},
    {"Lessons", M_KNOBS | M_OCTU | M_SMALL, "Turn KNOB 1 to pick a lesson and press OCT+ to start it. Rings show where the hits go. HOME leaves and your project comes back."},
    {"Reset", M_OCTU | M_SMALL, "Press OCT+ on Factory Reset to ask. OCT+ again says yes, OCT- says no. Yes wipes your projects for good."},
};
static const mpage_t MP_BT[] = {
    {"Sync", M_OCTU | M_SMALL, "Open BROTHERS LINK in the menu. Set Sync to Lead on one FM-1 and Follow on the others with OCT+ or OCT-. It is always Off after power-up."},
    {"Options", M_OCTU | M_SMALL, "Turn SELECT to pick a row. Sidechain ducks a follower on every kick of the leader. Patterns follows the leader's pattern."},
    {"Punch-in FX", M_FX | M_SMALL, "With Punch-in FX On, pads 1 to 8 held on the leader play on every follower too."},
    {"Demo", M_OCTU | M_SMALL, "Demo loads a locked beat to play along with. Nothing in it is saved. Open any project to leave it."},
};
#define MC(n, c, p) {n, c, p, sizeof p / sizeof p[0]}
static const mchap_t MCH[] = {
    MC("Start", RGB(240, 100, 70), MP_START), MC("Pattern", RGB(250, 190, 60), MP_PTN), MC("Lab", RGB(110, 220, 185), MP_LAB),
    MC("Sounds", RGB(170, 140, 240), MP_SND), MC("Effects", RGB(240, 100, 70), MP_FX), MC("Send", RGB(250, 190, 60), MP_SEND),
    MC("Projects", RGB(110, 220, 185), MP_PROJ), MC("Menu", RGB(170, 140, 240), MP_MENU), MC("Brothers Link", RGB(240, 100, 70), MP_BT),
};
#define MCHN (sizeof MCH / sizeof MCH[0])
enum { MC_START, MC_PTN, MC_LAB, MC_SND, MC_FX, MC_SEND, MC_PROJ, MC_MENU, MC_BT };

/* the FM-1 as the Play online page draws it (1040 x 664, cut off under the buttons): knob and button centres */
static const int16_t MKN[8][2] = {{139, 187}, {253, 185}, {139, 295}, {253, 295}, {665, 185}, {772, 185}, {880, 185}, {988, 185}};
static const int16_t MBT[14][3] = {{147, 380, 84}, {249, 380, 84}, {663, 287, 58}, {729, 287, 58}, {794, 287, 58}, {860, 287, 58}, {925, 287, 58}, {990, 287, 58},
                                   {663, 352, 58}, {729, 352, 58}, {794, 352, 58}, {860, 352, 58}, {925, 352, 58}, {990, 352, 58}};
static int32_t man_k = 13, man_x0 = 16, man_y0 = 34;      /* the picture's scale in 64ths of a unit (13 = 0.2, 7 = small) and where it sits */
#define MPX(v) (man_x0 + (((v) - 30) * man_k) / 64)
#define MPY(v) (man_y0 + (((v) - 82) * man_k) / 64)
#define MZ(n) (((n) * man_k + 6) / 13)

static uint8_t man_ch, man_pg, man_toc;
static void man_ring(int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
    cv_rect(x1, y1, x2 - x1, 2, K_CORAL); cv_rect(x1, y2 - 2, x2 - x1, 2, K_CORAL); cv_rect(x1, y1, 2, y2 - y1, K_CORAL); cv_rect(x2 - 2, y1, 2, y2 - y1, K_CORAL);
}
static void man_panel(uint32_t ring)
{
    uint32_t i;
    cv_rect(MPX(32), MPY(84), MZ(207), MZ(68 + 14), RGB(24, 24, 32));
    cv_rect(MPX(342), MPY(127), MZ(49), MZ(49), K_BG);
    for (i = 0; i < 8u; i++) { cv_rect(MPX(MKN[i][0]) - MZ(5), MPY(MKN[i][1]) - MZ(5), MZ(11), MZ(11), RGB(14, 14, 18)); cv_rect(MPX(MKN[i][0]) - 1, MPY(MKN[i][1]) - MZ(5), 2, MZ(5), K_CHR); }
    for (i = 0; i < 14u; i++) cv_rect(MPX(MBT[i][0]) - MZ(MBT[i][2] / 10), MPY(MBT[i][1]) - MZ(4), MZ(MBT[i][2] / 5), MZ(8), RGB(60, 60, 78));
    for (i = 0; i < 16u; i++) cv_rect(MPX(80 + (int32_t)i * 587 / 10), MPY(424) + MZ(4), MZ(9), MZ(7), RGB(74, 74, 90));
    for (i = 0; i < 8u; i++)
        if (ring >> i & 1u) man_ring(MPX(MKN[i][0]) - MZ(9), MPY(MKN[i][1]) - MZ(9), MPX(MKN[i][0]) + MZ(10), MPY(MKN[i][1]) + MZ(10));
    for (i = 0; i < 14u; i++)
        if (ring >> (i + 8u) & 1u) man_ring(MPX(MBT[i][0]) - MZ(MBT[i][2] / 10 + 4), MPY(MBT[i][1]) - MZ(8), MPX(MBT[i][0]) + MZ(MBT[i][2] / 10 + 4), MPY(MBT[i][1]) + MZ(9));
    if (ring & M_KEYS) man_ring(MPX(60), MPY(424) + MZ(1), MPX(1040), MPY(424) + MZ(14));
}
static int32_t man_lines(int32_t x, int32_t y, const char *s, int32_t maxw)       /* wrapped on spaces, 17 px a line; returns the next y */
{
    const char *ls = s, *le = s, *p = s;
    char t[64];
    for (;;) {
        const char *we;
        uint32_t n;
        while (*p == ' ') p++;
        we = p;
        while (*we && *we != ' ') we++;
        if (we == p) break;
        n = (uint32_t)(we - ls);
        if (n > 63u) n = 63u;
        memcpy(t, ls, n); t[n] = 0;
        if (le != ls && text_w(&FONT_S, t) > maxw) {
            n = (uint32_t)(le - ls); memcpy(t, ls, n); t[n] = 0;
            txt(x, y, t, K_NAVY); y += 17;
            ls = p;
        }
        le = we; p = we;
    }
    if (le != ls) { uint32_t n = (uint32_t)(le - ls); memcpy(t, ls, n); t[n] = 0; txt(x, y, t, K_NAVY); y += 17; }
    return y;
}
static void man_num(char *d, uint32_t a, uint32_t b)         /* "a/b", both under 100 */
{
    uint32_t n = 0;
    if (a >= 10u) d[n++] = (char)('0' + a / 10u);
    d[n++] = (char)('0' + a % 10u); d[n++] = '/';
    if (b >= 10u) d[n++] = (char)('0' + b / 10u);
    d[n++] = (char)('0' + b % 10u); d[n] = 0;
}
static void man_paper(void)                              /* graph paper: cream with faint blue-green lines, a margin rule */
{
    int32_t i;
    uint16_t g = mixc(K_CREAM, K_BODY, 34);
    cv_rect(0, 0, 240, 240, K_CREAM);
    for (i = 38; i < 240; i += 12) cv_rect(0, i, 240, 1, g);
    for (i = 6; i < 240; i += 12) cv_rect(i, 30, 1, 210, g);
}
static void scene_man(void)
{
    uint32_t i;
    char nb[8];
    const mchap_t *c = &MCH[man_ch];
    const mpage_t *p = &c->page[man_pg];
    man_paper();
    cv_rect(0, 0, 240, 26, K_NAVY);
    cv_rect(0, 26, 240, 3, c->col);
    if (man_toc) {
        txt(8, 4, "Manual", K_CREAM);
        for (i = 0; i < MCHN; i++) {
            int32_t y = 34 + (int32_t)i * 19;
            char cn[4];
            if (i == man_ch) cv_rect(0, y, 240, 18, K_NAVY);
            cv_rect(8, y + 3, 5, 12, MCH[i].col);
            txt(22, y + 1, MCH[i].name, i == man_ch ? K_CREAM : K_NAVY);
            cn[0] = (char)('0' + MCH[i].n / 10u); cn[1] = (char)('0' + MCH[i].n % 10u); cn[2] = 0;
            txt(226 - text_w(&FONT_S, MCH[i].n >= 10u ? cn : cn + 1), y + 1, MCH[i].n >= 10u ? cn : cn + 1, i == man_ch ? K_CREAM : K_NAVY);
        }
        cv_rect(8, 214, 224, 2, K_NAVY);
        txt(8, 219, "OCT+  Open", K_NAVY);
        txt(232 - text_w(&FONT_S, "HOME  Close"), 219, "HOME  Close", K_NAVY);
        return;
    }
    man_num(nb, (uint32_t)man_pg + 1u, c->n);
    txt(8, 4, c->name, c->col);
    txt(232 - text_w(&FONT_S, nb), 4, nb, c->col);
    if (p->ring & M_NOPANEL) man_lines(8, 70, p->text, 224);
    else {
        int32_t ty = 142, small = (p->ring & M_SMALL) != 0;
        man_k = small ? 7 : 13; man_x0 = small ? 64 : 16; man_y0 = small ? 31 : 34;
        man_panel(p->ring);
        if (small) { ty = 74; if (p->topic[0]) { txt(8, 72, p->topic, K_CORAL); ty = 90; } }
        else if (p->topic[0]) { txt(8, 120, p->topic, K_CORAL); ty = 138; }
        else ty = 120;
        man_lines(8, ty, p->text, 224);
        man_k = 13; man_x0 = 16;
    }
    {   int32_t st = c->n > 9u ? 7 : 10, d0 = 232 - (int32_t)c->n * st + 4;
        for (i = 0; i < c->n; i++) cv_rect(d0 + (int32_t)i * st, 228, st - 4, 4, i <= man_pg ? K_NAVY : mixc(K_CREAM, K_NAVY, 80));
        txt(8, 218, "GLO  Contents", K_NAVY);
        if (man_pg > 0u || man_ch > 0u) txt(d0 - 14, 216, "<", K_NAVY);
        if ((uint32_t)man_pg + 1u < c->n || (uint32_t)man_ch + 1u < MCHN) txt(228, 216, ">", K_NAVY);
    }
}
static void man_open(uint32_t ch, uint32_t pg) { man_ch = (uint8_t)ch; man_pg = (uint8_t)pg; man_toc = 0; help_on = 1; dirty = 1; }
static void man_input(uint32_t bdown)                    /* SELECT turns pages (on into the next chapter), PRESETS jumps chapters, OCT+ opens a chapter from the contents */
{
    int32_t e = fm1_enc_take(ENC_SELECT), f = fm1_enc_take(ENC_PRESETS);
    (void)fm1_enc_take(ENC_ALGORITHM);
    (void)fm1_enc_take(ENC_KNOB1); (void)fm1_enc_take(ENC_KNOB2); (void)fm1_enc_take(ENC_KNOB3); (void)fm1_enc_take(ENC_KNOB4);
    if (e && !man_toc) {
        if (e > 0) { if ((uint32_t)man_pg + 1u < MCH[man_ch].n) man_pg++; else if ((uint32_t)man_ch + 1u < MCHN) { man_ch++; man_pg = 0; } }
        else { if (man_pg > 0u) man_pg--; else if (man_ch > 0u) { man_ch--; man_pg = (uint8_t)(MCH[man_ch].n - 1u); } }
        dirty = 1;
    } else if (e) f = e;
    if (f) {
        if (f > 0 && (uint32_t)man_ch + 1u < MCHN) man_ch++;
        else if (f < 0 && man_ch > 0u) man_ch--;
        man_pg = 0; dirty = 1;
    }
    if (man_toc && (bdown & (1u << BTN_OCT_UP))) { man_toc = 0; man_pg = 0; dirty = 1; }
}
static uint32_t man_for_screen(void)                      /* the chapter for where you are */
{
    return pick_on ? MC_PROJ : fx_layer ? MC_FX : view == V_PAT ? MC_PTN : view == V_SND ? MC_LAB : MC_SEND;
}
