/* SPDX-License-Identifier: GPL-3.0-only */
/* Drumlab's look: a 1950s atomic lab. Everything that decides how it looks is in this file; the logic (labui.c) never draws.
 *   PATTERN  dark: toggle switches, four sheets of dot-matrix paper, a seek head, aircraft dials, a radio dial for tabs
 *   LAB      light: needle meters, lamp selectors, a CRT or a chart recorder for the sound's loudness
 *   SEND     a CRT monitor
 * The screen is painted in three bands of 96 rows (the canvas is at most 240 x 96); coordinates are screen coordinates (cv_ox, cv_oy shift them).
 * A full repaint happens only when something changed; the atom, the seek head and the key map are small blits. */

/* ---- colours */
#define K_BG    RGB(26, 24, 34)
#define K_PLATE RGB(38, 36, 48)
#define K_RAIL  RGB(46, 44, 58)
#define K_CHR   RGB(205, 209, 216)
#define K_CHRD  RGB(100, 104, 116)
#define K_CREAM RGB(238, 228, 198)
#define K_CORAL RGB(240, 100, 70)
#define K_MINT  RGB(110, 220, 185)
#define K_AMB   RGB(255, 160, 60)
#define K_NAVY  RGB(20, 38, 64)
#define K_BAR   RGB(214, 230, 205)
#define K_BODY  RGB(66, 160, 148)
#define K_BODY2 RGB(52, 130, 120)
#define K_SHAD  RGB(10, 9, 14)
#define K_GLASS RGB(52, 38, 18)
#define K_RED   RGB(226, 76, 52)
#define K_TEAL  RGB(24, 120, 110)
static const uint16_t INKC[4] = {RGB(190, 40, 30), RGB(185, 115, 10), RGB(15, 115, 85), RGB(95, 55, 175)};
static const uint16_t LANEC[4] = {RGB(240, 100, 70), RGB(250, 190, 60), RGB(110, 220, 185), RGB(170, 140, 240)};

/* what the help / project screens (and lab.c's power-on) use */
static struct {
    uint16_t bg, panel, line, dim, text, accent, hot, rec, amber;
    uint16_t lane[4];
} SK;
static void skin_init(void)
{
    uint32_t i;
    SK.bg = K_BG; SK.panel = K_PLATE; SK.line = K_CHRD; SK.dim = RGB(130, 126, 120); SK.text = K_CREAM; SK.accent = K_MINT;
    SK.hot = RGB(255, 240, 205); SK.rec = K_CORAL; SK.amber = K_AMB;
    for (i = 0; i < 4u; i++) SK.lane[i] = LANEC[i];
}
static uint16_t shade(uint16_t c, uint32_t k)             /* k of 256 */
{
    uint32_t r = (c >> 11) * k >> 8, g = ((c >> 5) & 63u) * k >> 8, b = (c & 31u) * k >> 8;
    return (uint16_t)(r << 11 | g << 5 | b);
}
static uint16_t mixc(uint16_t a, uint16_t b, uint32_t t)  /* t of 256 of the way from a to b */
{
    int32_t ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31, br = b >> 11, bg = (b >> 5) & 63, bb = b & 31, k = (int32_t)t;
    return (uint16_t)((uint32_t)(ar + (br - ar) * k / 256) << 11 | (uint32_t)(ag + (bg - ag) * k / 256) << 5 | (uint32_t)(ab + (bb - ab) * k / 256));
}

/* ---- a band of the screen is on the canvas: sections outside it are not drawn */
static int32_t band_y0, band_y1;
static int vis(int32_t a, int32_t b) { return b > band_y0 && a < band_y1; }

/* ---- primitives */
static int32_t isq(int32_t v) { int32_t r = 0; while ((r + 1) * (r + 1) <= v) r++; return r; }
static void fcirc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t dy;
    for (dy = -r; dy <= r; dy++) {
        int32_t y = cy + dy, dx;
        if (y + cv_oy < 0 || y + cv_oy >= (int32_t)cv_h) continue;
        dx = isq(r * r + r - dy * dy);
        cv_rect(cx - dx, y, 2 * dx + 1, 1, c);
    }
}
static void ring(int32_t cx, int32_t cy, int32_t r, uint16_t c)       /* a one pixel ring */
{
    int32_t dy, ri = r - 1;
    for (dy = -r; dy <= r; dy++) {
        int32_t y = cy + dy, ox, q;
        if (y + cv_oy < 0 || y + cv_oy >= (int32_t)cv_h) continue;
        ox = isq(r * r + r - dy * dy);
        q = ri * ri + ri - dy * dy;
        if (q < 0) cv_rect(cx - ox, y, 2 * ox + 1, 1, c);
        else { int32_t ix = isq(q); cv_rect(cx - ox, y, ox - ix, 1, c); cv_rect(cx + ix + 1, y, ox - ix, 1, c); }
    }
}
static void rrect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    int32_t i;
    for (i = 0; i < h; i++) {
        int32_t d = i < r ? r - i : (i >= h - r ? i - (h - r) + 1 : 0), in = d ? r - isq(r * r - (d - 1) * (d - 1)) : 0;
        if (y + i + cv_oy < 0 || y + i + cv_oy >= (int32_t)cv_h) continue;
        cv_rect(x + in, y + i, w - 2 * in, 1, c);
    }
}
static void rrect_o(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t line, uint16_t fill)
{
    rrect(x, y, w, h, r, line); rrect(x + 1, y + 1, w - 2, h - 2, r > 0 ? r - 1 : 0, fill);
}
static void tint(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c, uint32_t a)       /* c over what is there, a of 256 */
{
    int32_t i, j;
    int32_t cr = c >> 11, cg = (c >> 5) & 63, cb = c & 31, k = (int32_t)a;
    x += cv_ox; y += cv_oy;
    for (j = y; j < y + h; j++) {
        if (j < 0 || j >= (int32_t)cv_h) continue;
        for (i = x; i < x + w; i++) {
            uint32_t p;
            int32_t r, g, b;
            if (i < 0 || i >= (int32_t)cv_w) continue;
            p = swap16(cv_px[(uint32_t)j * cv_w + (uint32_t)i]);
            r = (int32_t)(p >> 11); g = (int32_t)((p >> 5) & 63u); b = (int32_t)(p & 31u);
            r += (cr - r) * k / 256; g += (cg - g) * k / 256; b += (cb - b) * k / 256;
            cv_px[(uint32_t)j * cv_w + (uint32_t)i] = swap16((uint32_t)(r << 11 | g << 5 | b));
        }
    }
}
static void scanlines(int32_t x, int32_t y, int32_t w, int32_t h)                       /* every other row a little darker: a tube */
{
    int32_t j;
    for (j = y + 1; j < y + h; j += 2) tint(x, j, w, 1, C_BLACK, 80);
}
static void phpt(int32_t cx, int32_t cy, int32_t r, uint32_t ph, int32_t *x, int32_t *y)       /* a point at r from (cx, cy); ph in turns, 0 = up, clockwise */
{
    *x = cx + (int32_t)(((int64_t)r * sinq(ph)) >> 30);
    *y = cy - (int32_t)(((int64_t)r * cosq(ph)) >> 30);
}

/* ---- the small pixel font: 5 x 7, capitals, digits and a few marks; k = size */
static const char TFC[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-.%/|?>";
static const uint8_t TF[44][7] = {
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}, {0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E},
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}, {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}, {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}, {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}, {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, {0x11,0x19,0x15,0x13,0x11,0x11,0x11}, {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}, {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}, {0x11,0x11,0x11,0x15,0x15,0x1B,0x11}, {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}, {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
    {0x00,0x04,0x04,0x1F,0x04,0x04,0x00}, {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C}, {0x19,0x1A,0x02,0x04,0x08,0x0B,0x13},
    {0x01,0x02,0x02,0x04,0x08,0x08,0x10}, {0x04,0x04,0x04,0x04,0x04,0x04,0x04}, {0x0E,0x11,0x01,0x02,0x04,0x00,0x04}, {0x10,0x08,0x04,0x02,0x04,0x08,0x10},
};
static int32_t tw(const char *s, int32_t k) { int32_t n = 0; for (; *s; s++) n++; return n ? n * 6 * k - k : 0; }
static void ttx(int32_t x, int32_t y, const char *s, uint16_t c, int32_t k)
{
    for (; *s; s++, x += 6 * k) {
        char ch = *s;
        const char *p;
        uint32_t gi = 0, r;
        if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
        if (ch == ' ') continue;
        for (p = TFC; *p && *p != ch; p++) gi++;
        if (!*p) gi = 42u;
        for (r = 0; r < 7u; r++) {
            uint32_t row = TF[gi][r], b = 0;
            while (b < 5u) {
                if (row >> (4u - b) & 1u) {
                    uint32_t e = b;
                    while (e < 5u && (row >> (4u - e) & 1u)) e++;
                    cv_rect(x + (int32_t)b * k, y + (int32_t)r * k, (int32_t)(e - b) * k, k, c);
                    b = e;
                } else b++;
            }
        }
    }
}
static void tt(int32_t x, int32_t y, const char *s, uint16_t c) { ttx(x, y, s, c, 1); }
static void ttc(int32_t cx, int32_t y, const char *s, uint16_t c) { ttx(cx - tw(s, 1) / 2, y, s, c, 1); }

/* ---- Chipbox's helpers (the help and project screens, the effects layer) */
static char *num2(char *s, uint32_t v) { *s++ = (char)('0' + v / 10u % 10u); *s++ = (char)('0' + v % 10u); *s = 0; return s; }
static void txt(int32_t x, int32_t y, const char *s, uint16_t c) { cv_text(x, y, &FONT_S, s, c); }
static void txtc(int32_t cx, int32_t y, const char *s, uint16_t c) { cv_text(cx - text_w(&FONT_S, s) / 2, y, &FONT_S, s, c); }
static void frame(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    cv_rect(x, y, w, 1, c); cv_rect(x, y + h - 1, w, 1, c); cv_rect(x, y, 1, h, c); cv_rect(x + w - 1, y, 1, h, c);
}
static void dframe(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c) { frame(x, y, w, h, c); frame(x + 2, y + 2, w - 4, h - 4, shade(c, 110)); }
static void dot(int32_t x, int32_t y, int32_t s, uint16_t c) { cv_rect(x, y, s, s, c); }
/* a number as text, no leading zeros: returns the end of the string */
static char *numu(char *s, uint32_t v)
{
    char t[11];
    uint32_t n = 0;
    do { t[n++] = (char)('0' + v % 10u); v /= 10u; } while (v);
    while (n) *s++ = t[--n];
    *s = 0;
    return s;
}

static void flask(int32_t x, int32_t y, uint32_t t)           /* the lab's icon (the power-on picture); t moves its bubbles */
{
    int32_t r;
    cv_rect(x + 12, y, 10, 3, SK.line);
    cv_rect(x + 14, y + 3, 6, 9, SK.line);
    for (r = 0; r < 16; r++) { int32_t w = 8 + r, x0 = x + 17 - w / 2 - 1; cv_rect(x0, y + 12 + r, w + 2, 1, SK.line); if (r > 5) cv_rect(x0 + 1, y + 12 + r, w, 1, SK.accent); }
    cv_rect(x + 3, y + 27, 28, 2, SK.line);
    dot(x + 14, y + 20 - (int32_t)(t & 3u), 3, SK.hot); dot(x + 20, y + 24 - (int32_t)((t >> 1) & 3u), 2, SK.hot);
}

/* ---- the light screens are the sound lab */
static uint32_t lite(void) { return view == V_SND && !help_on && !pick_on && !menu_on; }

static void smiley(int32_t x, int32_t y)                            /* 11 x 11: the acid face */
{
    fcirc(x + 5, y + 5, 5, RGB(250, 220, 40));
    cv_rect(x + 3, y + 3, 1, 2, K_BG); cv_rect(x + 7, y + 3, 1, 2, K_BG);
    cv_rect(x + 2, y + 6, 1, 1, K_BG); cv_rect(x + 3, y + 7, 5, 1, K_BG); cv_rect(x + 8, y + 6, 1, 1, K_BG);
}
static void chip_icon(int32_t x, int32_t y)                         /* 11 x 11: a chip with legs (Chipbox) */
{
    int32_t i;
    cv_rect(x + 2, y + 1, 7, 9, RGB(60, 70, 90)); cv_rect(x + 3, y + 2, 5, 7, RGB(30, 36, 50)); cv_rect(x + 4, y + 2, 3, 1, RGB(110, 220, 185));
    for (i = 0; i < 4; i++) { cv_rect(x, y + 2 + i * 2, 2, 1, K_CHR); cv_rect(x + 9, y + 2 + i * 2, 2, 1, K_CHR); }
}
static int name_is(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }

/* ---- the secret: keys 1, 4, 13, 16 in a row turn the PATTERN view into a pocket operator's LCD */
static uint32_t po_active(void) { return po_mode && view == V_PAT && !help_on && !pick_on && !menu_on && !kit_on && !les_on && !fx_layer; }
static void seg7(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t d, uint16_t on, uint16_t off)
{
    static const uint8_t M[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};       /* bits a b c d e f g */
    int32_t t = 3, hh = h / 2;
    uint32_t m = M[d % 10u];
    cv_rect(x + t, y, w - 2 * t, t, m & 1u ? on : off);
    cv_rect(x + w - t, y + t, t, hh - t, m & 2u ? on : off);
    cv_rect(x + w - t, y + hh, t, hh - t, m & 4u ? on : off);
    cv_rect(x + t, y + h - t, w - 2 * t, t, m & 8u ? on : off);
    cv_rect(x, y + hh, t, hh - t, m & 16u ? on : off);
    cv_rect(x, y + t, t, hh - t, m & 32u ? on : off);
    cv_rect(x + t, y + hh - 1, w - 2 * t, t, m & 64u ? on : off);
}
static void draw_po(void)
{
    const uint16_t LB = RGB(176, 190, 152), INK = RGB(34, 44, 32), GH = RGB(160, 174, 138);
    uint32_t i, l, p = (uint32_t)cur_pat + 1u, st = cur_step();
    int32_t x;
    rrect_o(2, 2, 236, 213, 10, RGB(70, 70, 64), LB);
    ttx(14, 9, "PTN", INK, 1); ttx(150, 9, "BPM", INK, 1);
    seg7(14, 18, 16, 30, p / 10u, INK, GH); seg7(36, 18, 16, 30, p % 10u, INK, GH);
    seg7(150, 18, 16, 30, bpm / 100u % 10u, INK, GH); seg7(172, 18, 16, 30, bpm / 10u % 10u, INK, GH); seg7(194, 18, 16, 30, bpm % 10u, INK, GH);
    for (l = 0; l < 4u; l++)
        for (i = 0; i < 16u; i++) {
            uint32_t t = pat[cur_pat].trig[l][i];
            x = 16 + (int32_t)i * 13;
            if (t & 15u) {
                cv_rect(x, 62 + (int32_t)l * 15, 11, 12, INK);
                if (((t >> 7) & 1u) || (pat[cur_pat].acc >> i & 1u)) cv_rect(x + 4, 66 + (int32_t)l * 15, 3, 4, LB);
            } else cv_rect(x + 4, 66 + (int32_t)l * 15, 3, 3, GH);
        }
    if (sq_run) { x = 16 + (int32_t)st * 13; frame(x - 2, 60, 15, 62, INK); }
    for (i = 0; i < 12u; i++) {                                         /* the bars that dance while it plays */
        uint32_t hgt = sq_run ? 6u + (((i * 2654435761u) ^ (fm1_ms / 110u * 40503u)) >> 7) % 40u : 4u;
        cv_rect(16 + (int32_t)i * 17, 188 - (int32_t)hgt, 13, (int32_t)hgt, INK);
    }
    ttc(120, 196, "DRUMLAB", INK);
    { static const char *const AN[4] = {"WRITE", "ACC", "FX", "PLAY"}; uint32_t on[4] = {wr, accm, fx_layer, sq_run};
      for (i = 0; i < 4u; i++) { int32_t ax = 14 + (int32_t)i * 54; cv_rect(ax, 128, 50, 11, on[i] ? INK : GH); ttc(ax + 25, 130, AN[i], on[i] ? LB : INK); } }
}

/* ---- little animations: everything moves a few frames after you touch it */
static uint32_t atom_ph, atom_boost, atom_hit_t, kit_roll_t, ask_t, anim_ms;     /* the atom's own clock; extra speed; when a sound last played; when a kit was rolled / the send window opened */
static int32_t tab_x = 28;                                                       /* the radio needle on its way */
static int32_t dv[4];                                                            /* the dial needles on show (0..255, x16) easing to their values */
static uint8_t tg_prev[8], tg_t[8] = {6, 6, 6, 6, 6, 6, 6, 6}, tg_view = 9;       /* the toggles: last state, frame of the throw (6 = still) */
static const int16_t BOUNCE[7] = {0, 52, 108, 94, 103, 99, 100};                  /* how far the lever has gone (percent): it overshoots and settles */

/* ---- the atom: three tilted orbits, an electron on each at its own speed and direction, behind the nucleus when it passes it */
static int32_t atom_cx = 23, atom_cy = 20, atom_k = 1, atom_pulse, atom_z;   /* where it is, how big (atom_z, in eighths, when it is not 0: the power-on zoom), and a beat of the nucleus */
static void atom(uint32_t n, uint16_t hdr, uint16_t orb, uint32_t t)
{
    static const uint32_t TILT[3] = {0u, 0x2AAAAAABu, 0x55555555u};
    uint32_t o, k;
    int32_t cx = atom_cx, cy = atom_cy, x, y, Z = atom_z ? atom_z : 8 * atom_k, K = (Z + 4) / 8, A = 19 * Z / 8, Bm = 7 * Z / 8, R = 11 * Z / 8 + atom_pulse;
    char b[4];
    for (o = 0; o < 3u; o++) {
        int32_t lx = 0, ly = 0;
        for (k = 0; k <= 36u; k++) {
            int64_t ex = ((int64_t)A * cosq(k * 119304647u)) >> 30, ey = ((int64_t)Bm * sinq(k * 119304647u)) >> 30;
            x = cx + (int32_t)((ex * cosq(TILT[o]) - ey * sinq(TILT[o])) >> 30);
            y = cy + (int32_t)((ex * sinq(TILT[o]) + ey * cosq(TILT[o])) >> 30);
            if (k) cv_line(lx, ly, x, y, orb);
            lx = x; ly = y;
        }
    }
    fcirc(cx, cy, R, K_CREAM); fcirc(cx, cy, R - 1, K_CORAL);
    if (n && fol_on) {
        cv_rect(cx - 4 * K, cy - 4 * K, K + 1, 2 * K + 1, hdr); cv_rect(cx + 3 * K, cy - 4 * K, K + 1, 2 * K + 1, hdr);
        cv_line(cx - 4 * K, cy + 1 * K, cx - 2 * K, cy + 4 * K, hdr); cv_line(cx - 2 * K, cy + 4 * K, cx + 3 * K, cy + 4 * K, hdr); cv_line(cx + 3 * K, cy + 4 * K, cx + 5 * K, cy + 1 * K, hdr);
    } else if (n) {
        numu(b, n);
        if (n < 10u) ttx(cx - 4 * K, cy - 6 * K, b, hdr, 2 * K); else ttx(cx - tw(b, 1) / 2, cy - 3, b, hdr, 1);
    }
    for (o = 0; o < 3u; o++) {
        uint32_t ph = o == 0 ? t * 1789569u : o == 1 ? 0x30000000u - t * 2863311u : 0x80000000u + t * 4294967u;
        int64_t ex = ((int64_t)A * cosq(ph)) >> 30, ey = ((int64_t)Bm * sinq(ph)) >> 30;
        x = cx + (int32_t)((ex * cosq(TILT[o]) - ey * sinq(TILT[o])) >> 30);
        y = cy + (int32_t)((ex * sinq(TILT[o]) + ey * cosq(TILT[o])) >> 30);
        if ((x - cx) * (x - cx) + (y - cy) * (y - cy) > (13 * Z / 8) * (13 * Z / 8)) { fcirc(x, y, 1 + K, orb); cv_rect(x - 1, y - 1, 1, 1, K_CREAM); }
    }
}
static uint32_t atom_n(void) { return pick_on ? (uint32_t)pick_sel + 1u : view == V_SND ? sel + 1u : view == V_SEND ? 32u : (uint32_t)cur_pat + 1u; }

/* ---- header pieces */
static void toggle(int32_t x, uint32_t ti, int on, const char *label, uint16_t lc, uint16_t loff)      /* a toggle switch: lever up is on; it throws with a bounce */
{
    int32_t from = on ? 23 : 5, to = on ? 5 : 23, tip = to, f = 100;
    if (tg_t[ti] < 6u) { f = BOUNCE[tg_t[ti]]; tip = from + (to - from) * f / 100; }
    fcirc(x, 14, 6, K_CHR); ring(x, 14, 6, K_CHRD); fcirc(x, 14, 3, RGB(60, 62, 72));
    if (tip < 14) cv_rect(x - 1, tip, 2, 14 - tip, K_CHR); else cv_rect(x - 1, 14, 2, tip - 14 + 1, K_CHR);
    { int lit = on ? f > 45 : f < 55; fcirc(x, tip, 3, lit ? K_CORAL : RGB(120, 70, 60)); if (lit) ring(x, tip, 3, K_CREAM); }
    ttc(x, 27, label, on ? lc : loff);
}
static void nixie(uint32_t v)                                       /* the tempo in a glass tube, the battery under it */
{
    char b[5];
    int32_t x, n, i, bx = 205, by = 35;
    uint16_t bc = lite() ? K_NAVY : K_CHR;
    rrect_o(199, 2, 40, 31, 7, K_CHRD, RGB(14, 12, 16));
    numu(b, v);
    for (n = 0; b[n]; n++) {}
    x = 219 - (n * 11 - 1) / 2;
    for (i = 0; i < n; i++) { char c[2]; c[0] = b[i]; c[1] = 0; ttx(x + i * 11 + 1, 7, c, fol_on ? RGB(20, 70, 90) : RGB(110, 50, 20), 2); ttx(x + i * 11, 6, c, fol_on ? RGB(110, 225, 255) : K_AMB, 2); }
    ttc(219, 23, "BPM", RGB(200, 130, 70));
    cv_rect(bx, by, 29, 7, bc); cv_rect(bx + 1, by + 1, 27, 5, lite() ? K_CREAM : K_BG); cv_rect(bx + 29, by + 2, 2, 3, bc);
    n = (int32_t)((bat_pct + 12u) / 25u);
    for (i = 0; i < 4; i++) cv_rect(bx + 2 + i * 6, by + 2, 5, 3, i < n && !(bat_pct < 12u && (fm1_ms / 400u & 1u)) ? (n <= 1 ? RGB(230, 60, 50) : RGB(80, 220, 110)) : mixc(bc, lite() ? K_CREAM : K_BG, 200));
}
static void keymap(int32_t x, int32_t y, uint16_t off, uint16_t flash)       /* the PO-32's 4 x 4 keys: the sound is the lit one */
{
    uint32_t s;
    for (s = 0; s < 16u; s++) {
        uint16_t c = s == sel ? LANEC[s & 3u] : mixc(LANEC[s & 3u], off, off == K_CREAM ? 120u : 190u);
        int32_t kx = x + (int32_t)(s & 3u) * 8, ky = y + (int32_t)(s >> 2) * 8;
        if ((pad_on >> s & 1u) && fm1_ms - pad_t[s] < 160u) c = flash;
        cv_rect(kx, ky, 7, 7, c);
        if (s == sel && off == K_CREAM) frame(kx - 1, ky - 1, 9, 9, K_NAVY);
    }
}
static int32_t map_x(void) { return view == V_SND ? 132 : 60; }
static void draw_header(void)
{
    uint32_t lt = lite();
    uint16_t hb = lt ? K_CREAM : K_BG, lc = lt ? K_NAVY : K_CREAM, lo = lt ? RGB(130, 138, 150) : RGB(130, 126, 120);
    cv_rect(0, 0, 240, 42, hb);
    if (lt) { cv_rect(0, 42, 240, 2, K_CHR); cv_rect(0, 44, 240, 1, K_CHRD); }
    else { cv_rect(0, 42, 240, 2, K_CORAL); rrect_o(46, 2, 150, 38, 6, K_CHRD, K_PLATE); }
    atom(atom_n(), lt ? K_NAVY : K_BG, lt ? K_TEAL : K_MINT, atom_ph);
    if (menu_on) { txt(60, 12, menu_grp == 0u ? "MENU" : menu_grp == 1u ? "SETTINGS" : menu_grp == 2u ? "SYNC AND LINK" : "RESET", K_CREAM); nixie(bpm); return; }
    if (pick_on) { txt(60, 12, "PROJECTS", K_CREAM); nixie(bpm); return; }
    if (help_on) { nixie(bpm); return; }
    if (view == V_PAT) {
        static const char *const NM[5] = {"WRITE", "ACCENT", "MUTE", "CHAIN", "FX"};
        uint32_t on[5], i;
        int32_t x = 51;
        on[0] = wr; on[1] = accm; on[2] = mutem != 0u; on[3] = chainm; on[4] = fx_layer;
        for (i = 0; i < 5u; i++) {
            const char *nm = i == 2u && mutem == 2u ? "SOLO" : NM[i];
            int32_t w = tw(nm, 1);
            if (w < 14) w = 14;
            toggle(x + w / 2, i, (int)on[i], nm, lc, lo);
            x += w + 3;
        }
    } else if (view == V_SND) {
        toggle(66, 5u, side == 0u, "A | B", lc, lo); toggle(98, 6u, aud_loop != 0u, "LOOP", lc, lo);
        keymap(map_x(), 7, K_CREAM, K_NAVY);
    } else {
        uint16_t c = RGB(230, 60, 50);
        keymap(map_x(), 7, K_PLATE, K_CREAM);
        fcirc(122, 14, 10, K_CHR); ring(122, 14, 10, K_CHRD); fcirc(122, 14, 8, RGB(40, 44, 56));
        fcirc(122, 14, 7, sending ? c : mixc(c, RGB(30, 30, 40), 185));
        if (sending) { ring(122, 14, 9, mixc(c, K_CREAM, 128)); cv_rect(120, 11, 2, 2, K_CREAM); }
        ttc(122, 27, "SENDING", sending ? K_CREAM : RGB(130, 126, 120));
        (void)lo;
    }
    nixie(bpm);
}
static void draw_tabs(void)                                         /* a radio dial: the red needle is the view */
{
    static const char *const T[3] = {"PTN", "LAB", "SEND"};
    uint32_t i;
    int32_t k;
    uint16_t hi, lo, bgc;
    if (!vis(216, 240)) return;
    if (pick_on) { cv_rect(0, 216, 240, 24, SK.panel); txt(150, 220, del_ask ? "" : "HOME Open", SK.text); return; }
    bgc = lite() ? RGB(226, 214, 180) : K_GLASS;
    hi = lite() ? RGB(30, 28, 40) : RGB(255, 214, 130); lo = lite() ? RGB(110, 98, 74) : RGB(196, 134, 62);
    rrect_o(3, 217, 234, 22, 4, K_CHR, bgc);
    for (k = 8; k < 232; k += 4) cv_rect(k, 219, 1, k % 16 ? 3 : 5, lite() ? RGB(120, 110, 90) : RGB(120, 100, 60));
    for (i = 0; i < 3u; i++) txtc(52 + (int32_t)i * 80, 223, T[i], i == view ? hi : lo);
    k = tab_x;
    cv_rect(k - 1, 217, 2, 22, RGB(230, 50, 40)); cv_rect(k - 3, 217, 6, 3, RGB(230, 50, 40));
}

/* ---- dials */
#define GSTART 0xA0000000u                                           /* -135 degrees */
#define GSPAN  0xC0000000u                                           /* 270 degrees */
static void gauge(int32_t cx, int32_t cy, int32_t R, uint32_t v)     /* the black aircraft dial of radius R; v 0..255 */
{
    uint32_t k;
    int32_t x0, y0, x1, y1, o = R - 4;
    fcirc(cx, cy, R, K_CHR); ring(cx, cy, R, K_CHRD); fcirc(cx, cy, R - 3, RGB(16, 16, 22));
    for (k = 0; k <= 10u; k++) {
        uint32_t ph = GSTART + (uint32_t)((uint64_t)GSPAN * k / 10u);
        phpt(cx, cy, k % 5u ? o - 3 : o - 5, ph, &x0, &y0); phpt(cx, cy, o, ph, &x1, &y1);
        cv_line(x0, y0, x1, y1, k >= 8u ? RGB(230, 60, 50) : K_CREAM);
    }
    phpt(cx, cy, R - 7, GSTART + (uint32_t)((uint64_t)GSPAN * v / 255u), &x1, &y1);
    cv_line(cx, cy, x1, y1, RGB(255, 150, 40)); cv_line(cx + 1, cy, x1 + 1, y1, RGB(255, 150, 40));
    fcirc(cx, cy, 2, K_CHR);
}
#define VSTART 0xE6666666u                                           /* -36 degrees */
#define VSPAN  0x33333333u                                           /* 72 degrees */
static void vu(int32_t x, uint32_t v)                                /* a needle meter on a card at x; v 0..255 */
{
    uint32_t k;
    int32_t px = x + 28, py = 208, x0, y0, x1, y1, cs;
    uint32_t ph;
    cv_rect(x + 1, 168, 55, 29, K_CHRD); cv_rect(x + 2, 169, 53, 27, K_CHR); cv_rect(x + 3, 170, 51, 25, K_CREAM);
    for (k = 0; k <= 10u; k++) {
        ph = VSTART + (uint32_t)((uint64_t)VSPAN * k / 10u);
        phpt(px, py, 30, ph, &x0, &y0); phpt(px, py, k % 5u ? 35 : 33, ph, &x1, &y1);
        cv_line(x0, y0, x1, y1, k >= 8u ? K_RED : K_NAVY);
    }
    ph = VSTART + (uint32_t)((uint64_t)VSPAN * v / 255u);
    cs = cosq(ph);
    phpt(px, py, (int32_t)(((int64_t)14 << 30) / (cs > 1000 ? cs : 1000)), ph, &x0, &y0); phpt(px, py, 33, ph, &x1, &y1);
    cv_line(x0, y0, x1, y1, K_NAVY);
}

/* ---- PATTERN ---- */
static int32_t cell_x(uint32_t s) { return 31 + (int32_t)s * 12 + (int32_t)(s >> 2) * 2; }
static int32_t lane_y(uint32_t l) { return 51 + (int32_t)l * 24; }
static uint16_t paper_of(uint32_t l, uint32_t muted) { return muted ? RGB(150, 146, 130) : (l & 1u) ? K_CREAM : K_BAR; }
static uint32_t lane_muted(uint32_t l) { return (mute_mask >> l & 1u) || (solo_lane >= 0 && solo_lane != (int)l); }

static void draw_hole(uint32_t l, uint32_t s, uint32_t muted)       /* one step of lane l: a punched hole, its pips say which of the column's four */
{
    int32_t x = cell_x(s) + 5, mid = lane_y(l) + 10;
    uint32_t t = pat[cur_pat].trig[l][s];
    if (!(t & 15u)) { cv_rect(x, mid - 2, 1, 5, RGB(170, 162, 134)); return; }
    {
        uint32_t g = (t >> 4) & 3u, k, acc = ((t >> 7) & 1u) || (pat[cur_pat].acc >> s & 1u);
        int32_t r = acc ? 7 : 5;
        uint16_t rim = (l + 4u * g == sel && !muted) ? LANEC[l] : mixc(LANEC[l], K_CREAM, 128);
        fcirc(x, mid, r, rim); fcirc(x, mid, r - 1, muted ? RGB(70, 70, 70) : K_BG);
        for (k = 0; k <= g; k++) cv_rect(x - 3 + (int32_t)(k & 1u) * 3, mid - 2 + (int32_t)(k >> 1) * 3, 2, 2, muted ? RGB(110, 110, 110) : LANEC[l]);
        if (acc) ring(x, mid, 9, K_CORAL);
    }
}
static void les_ghost(uint32_t l, uint32_t s)                        /* a lesson's target: a ring that breathes until the hit is there */
{
    int32_t x = cell_x(s) + 5, mid = lane_y(l) + 10;
    uint32_t g = (lessons[les_i].pat.trig[l][s] >> 4) & 3u, k;
    if (les_have(l, s) != 0u) return;
    ring(x, mid, (fm1_ms / 400u) & 1u ? 7 : 6, LANEC[l] == K_CREAM ? K_CREAM : INKC[l]);
    for (k = 0; k <= g; k++) cv_rect(x - 3 + (int32_t)(k & 1u) * 3, mid - 2 + (int32_t)(k >> 1) * 3, 2, 2, INKC[l]);
}
static void draw_lanes(int32_t xa, int32_t xb)                      /* the four sheets, the steps that touch xa .. xb */
{
    uint32_t l, s;
    for (l = 0; l < 4u; l++) {
        int32_t y = lane_y(l), yy;
        uint32_t m = lane_muted(l), on = (sel & 3u) == l;                    /* on: the lane of the selected sound */
        uint16_t paper = paper_of(l, m), tab = m ? mixc(LANEC[l], RGB(120, 120, 120), 150) : LANEC[l];
        char b[2];
        if (!vis(y, y + 23)) continue;
        cv_rect(3, y + 3, 237, 20, K_SHAD);                                   /* the sheet's shadow */
        cv_rect(2, y, 236, 20, paper);
        cv_rect(2, y, 8, 20, mixc(paper, RGB(120, 110, 80), 46)); cv_rect(230, y, 8, 20, mixc(paper, RGB(120, 110, 80), 46));
        fcirc(6, y + 4, 2, K_BG); fcirc(6, y + 15, 2, K_BG); fcirc(234, y + 4, 2, K_BG); fcirc(234, y + 15, 2, K_BG);
        for (yy = y; yy < y + 20; yy += 3) { cv_rect(9, yy, 1, 1, RGB(150, 140, 110)); cv_rect(230, yy, 1, 1, RGB(150, 140, 110)); }
        if (on) tint(10, y, 220, 20, LANEC[l], 34);                             /* the selected lane: a glow on its sheet */
        b[0] = (char)('1' + l); b[1] = 0;
        if (on) { rrect_o(10, y, 22, 20, 5, K_CREAM, tab); ttx(16, y + 3, b, K_BG, 2); }          /* a bigger, lit tab */
        else { rrect(12, y + 2, 18, 16, 4, tab); ttx(18, y + 6, b, K_BG, 1); }
        for (s = 0; s < 16u; s++) if (cell_x(s) + 16 > xa && cell_x(s) - 6 < xb) { draw_hole(l, s, m); if (les_on && !gl_active()) les_ghost(l, s); }
        if (on) {                                                                 /* a frame round the whole sheet in the lane's colour */
            cv_rect(0, y - 2, 240, 2, tab); cv_rect(0, y + 20, 240, 2, tab); cv_rect(0, y - 2, 2, 24, tab); cv_rect(238, y - 2, 2, 24, tab);
        } else tint(2, y, 236, 20, C_BLACK, 70);                                  /* the others sit back, a little darker */
    }
}
static void draw_head(int32_t s, int run)                           /* the seek head: a laser line with a yellow glow; parked at step 1 when stopped */
{
    int32_t x = cell_x((uint32_t)s);
    if (run) {
        tint(x - 4, 51, 18, 94, RGB(255, 225, 110), 26); tint(x - 2, 51, 14, 94, RGB(255, 225, 110), 41); tint(x + 1, 51, 8, 94, RGB(255, 225, 110), 66);
        cv_rect(x + 4, 51, 3, 94, RGB(255, 236, 150)); cv_rect(x + 5, 51, 1, 94, K_CORAL);
    }
    rrect_o(x - 1, 44, 13, 10, 3, K_CHRD, K_CHR); cv_rect(x + 3, 46, 5, 5, K_CORAL);
    if (run) {                                                          /* a step with hits: sparks */
        uint32_t l, hit = 0;
        for (l = 0; l < 4u; l++) hit |= pat[cur_pat].trig[l][s] & 15u;
        if (hit) { cv_rect(x - 4, 47, 2, 2, K_AMB); cv_rect(x + 13, 45, 2, 2, K_AMB); cv_rect(x + 15, 51, 2, 2, K_CORAL); cv_rect(x - 5, 53, 2, 2, K_CORAL); cv_rect(x + 3, 41, 2, 2, K_AMB); }
    }
}
static void draw_rail(void)                                         /* the seek head's rail; a lamp on it where a step holds a recorded effect */
{
    uint32_t i;
    cv_rect(0, 46, 240, 4, K_RAIL); cv_rect(0, 47, 240, 1, RGB(120, 118, 134));
    for (i = 0; i < 16u; i++) if (pat[cur_pat].fx[i]) cv_rect(cell_x(i) + 3, 47, 5, 3, K_AMB);
}

static void fmt_signed(char *b, int32_t v) { *b++ = v < 0 ? '-' : '+'; numu(b, (uint32_t)(v < 0 ? -v : v)); }
static void draw_chain(void)                                        /* the chain as numbers; the one playing is lit */
{
    uint32_t i, from = chain_n > 16u ? chain_n - 16u : 0u;
    cv_rect(3, 160, 234, 54, SK.panel); dframe(3, 160, 234, 54, SK.line);
    if (!chain_n) { txtc(120, 178, "KEYS ADD PATTERNS", SK.dim); return; }
    for (i = from; i < chain_n; i++) {
        char b[4];
        int32_t x = 10 + (int32_t)((i - from) & 7u) * 28, y = 166 + (int32_t)((i - from) >> 3) * 22;
        int on = sq_run && (uint32_t)(sq_cpos % chain_n) == i;
        cv_rect(x, y, 26, 19, on ? SK.hot : shade(SK.rec, 160));
        num2(b, chain[i] + 1u); txtc(x + 13, y + 1, b[0] == '0' ? b + 1 : b, on ? SK.bg : SK.hot);
    }
}
static void draw_fx(void)                                          /* the 16 punch-in effects on the 16 keys: the PO-32's own, or with Brothers Link on the shared eight, seven of the PO-32's and the latch */
{
    static const char *const P1[16] = {"STOP", "REPEAT", "LPF", "HPF", "ECHO", "REVERB", "CRUSH", "GATE", "HALF", "DIST", "P-LFO", "BEND", "GRAIN", "REVRSE", "FM", "LATCH"};
    uint32_t s, link = brothers_mode != 0u;
    cv_rect(0, 44, 240, 172, SK.bg);
    for (s = 0; s < 16u; s++) {
        int32_t x = 3 + (int32_t)(s & 3u) * 59, y = 47 + (int32_t)(s >> 2) * 42;
        uint32_t hot = !link ? (uint32_t)(fx_mask >> s & 1u) : s < 8u ? (uint32_t)(bfx_local >> s & 1u) : s == 15u ? (uint32_t)(fx_lamp & 1u) : (uint32_t)(fx_mask >> FX_PAD1[s - 8u] & 1u);
        rrect_o(x, y, 57, 39, 5, hot ? K_CREAM : K_CHRD, hot ? K_AMB : K_PLATE);
        txtc(x + 28, y + 11, link ? P1[s] : FX_NAME[s], hot ? SK.bg : SK.text);
    }
}
static void pat_body(void)
{
    if (fx_layer) { draw_fx(); return; }
    draw_rail(); draw_lanes(0, 240); draw_head(sq_run ? (int32_t)cur_step() : 0, sq_run);
}
static void txt_ml(int32_t x, int32_t y, const char *t, uint16_t col)     /* a text of up to three lines, split at \n */
{
    char ln[34];
    while (*t) {
        uint32_t n = 0;
        while (*t && *t != '\n' && n < 33u) ln[n++] = *t++;
        ln[n] = 0;
        if (*t == '\n') t++;
        txt(x, y, ln, col); y += 10;
    }
}
static void draw_lesson(void)                                       /* the lesson: its name, what to do, how far you are */
{
    const lesson_t *l = &lessons[les_i];
    uint32_t i, h, n, seg;
    char t[16], *e;
    les_count(&h, &n);
    rrect_o(6, 152, 228, 62, 6, les_done ? K_MINT : K_CHRD, K_PLATE);
    e = t; *e++ = (char)('0' + les_i + 1u); *e++ = '/'; *e++ = (char)('0' + NLESSON); *e = 0;
    txt(18, 156, l->name, K_AMB); txt(222 - text_w(&FONT_S, t), 156, t, RGB(160, 154, 134));
    if (les_done) { txt(18, 173, "NICE!", K_MINT); txt(222 - text_w(&FONT_S, "OCT+ Next  OCT- Done"), 173, "OCT+ Next  OCT- Done", K_CREAM); }
    else txt_ml(18, 168, l->tip, K_CREAM);
    seg = n ? h * 20u / n : 0u;
    for (i = 0; i < 20u; i++) cv_rect(18 + (int32_t)i * 10, 203, 8, 8, i < seg ? K_MINT : RGB(46, 44, 58));
}
static void draw_gltab(void)                                        /* the lesson is put away (GLO): a small tab at the top */
{
    char t[16], *e = t;
    const glesson_t *g = &GLESSON[les_i - NLESSON];
    const char *w = "LESSON ";
    while (*w) *e++ = *w++;
    e = numu(e, (uint32_t)gl_step + (les_done ? 0u : 1u)); *e++ = '/'; e = numu(e, g->n); *e = 0;
    rrect_o(116, 2, 118, 18, 4, K_AMB, K_PLATE);
    txt(124, 5, t, K_AMB);
}
static void draw_read(void)                                         /* a masterclass page: a window over the view, the title, the text, OCT+ next */
{
    const glesson_t *g = &GLESSON[les_i - NLESSON];
    const gstep_t *st = &g->s[gl_step];
    char ct[12], ln[34], *e = ct;
    const char *p = st->t;
    int32_t y = 68;
    e = numu(e, (uint32_t)gl_step + 1u); *e++ = '/'; e = numu(e, g->n); *e = 0;
    rrect_o(6, 46, 228, 168, 6, K_AMB, K_PLATE);
    txt(18, 50, st->title, K_AMB);
    txt(222 - text_w(&FONT_S, ct), 50, ct, RGB(160, 154, 134));
    cv_rect(16, 64, 208, 1, K_CHRD);
    while (*p && y < 190) {
        uint32_t n = 0;
        while (*p && *p != '\n' && n < 32u) ln[n++] = *p++;
        ln[n] = 0;
        if (*p == '\n') p++;
        txt(18, y, ln, K_CREAM);
        y += 12;
    }
    txt(18, 196, "OCT+ NEXT", K_AMB); txt(222 - text_w(&FONT_S, "HOME HIDE"), 196, "HOME HIDE", RGB(160, 154, 134));
}
static void draw_glcard(void)                                       /* the guided lesson's card, at the top so the knobs stay in view: name, what to do, how far you are */
{
    const glesson_t *g = &GLESSON[les_i - NLESSON];
    uint32_t i, seg, done = (uint32_t)gl_step + (gl_ok || les_done ? 1u : 0u);
    char t[16], *e = t;
    rrect_o(6, 0, 228, 62, 6, les_done || gl_ok ? K_MINT : K_CHRD, K_PLATE);
    { uint32_t k = les_i - NLESSON + 1u; if (k >= 10u) *e++ = (char)('0' + k / 10u); *e++ = (char)('0' + k % 10u); *e++ = '/'; *e++ = (char)('0' + NGL / 10u); *e++ = (char)('0' + NGL % 10u); *e = 0; }
    txt(222 - text_w(&FONT_S, t), 4, t, RGB(160, 154, 134));
    txt(18, 4, g->name, K_AMB);
    if (les_done) { txt(18, 21, "THAT'S IT!", K_MINT); txt(222 - text_w(&FONT_S, "OCT+ Next  OCT- Done"), 21, "OCT+ Next  OCT- Done", K_CREAM); }
    else if (gl_ok) txt(18, 21, GL_PRAISE[(les_i * 5u + gl_step * 3u) % 9u], K_MINT);
    else txt_ml(18, 20, g->s[gl_step].t, K_CREAM);
    seg = done * 20u / g->n;
    for (i = 0; i < 20u; i++) cv_rect(18 + (int32_t)i * 10, 49, 8, 8, i < seg ? K_MINT : RGB(46, 44, 58));
}
static void pat_ctrl(void)
{
    uint32_t i;
    static const char *const NM[4] = {"TEMPO", "SWING", "PITCH", "MORPH"};
    uint32_t v[4];
    char b[8];
    if (fx_layer) { draw_fx(); return; }
    if (les_on && !gl_active()) { draw_lesson(); return; }
    if (chainm) { draw_chain(); return; }
    v[0] = (bpm - 60u) * 255u / 180u; v[1] = (uint32_t)swing * 255u / 12u; v[2] = knob[sel][0]; v[3] = knob[sel][1];
    for (i = 0; i < 4u; i++) {
        int32_t x = 31 + (int32_t)i * 59;
        if (i == 0) numu(b, bpm); else if (i == 1) numu(b, swing);
        else if (i == 2) fmt_signed(b, ((int32_t)knob[sel][0] - 128) * 12 / 128);
        else { char *e = numu(b, knob[sel][1] * 100u / 255u); *e++ = '%'; *e = 0; }
        txtc(x, 148, b, K_AMB);
        gauge(x, 182, 18, (uint32_t)(dv[i] >> 4));
        txtc(x, 201, NM[i], K_CREAM);
    }
}

/* ---- LAB ---- */
static void param_text(char *b, uint32_t idx, uint32_t v)
{
    if (idx == 1u || idx == 8u || idx == 15u) {                       /* a frequency: 20 Hz * 1000 ^ x */
        uint32_t hz = 20u * exp2fx((int32_t)(((int64_t)K_L2_1000 * v) >> 16), 24, 8) >> 8;
        if (hz >= 10000u) { char *e = numu(b, (hz + 500u) / 1000u); *e++ = 'K'; *e = 0; }
        else if (hz >= 1000u) { char *e = numu(b, hz / 1000u); *e++ = '.'; *e++ = (char)('0' + hz % 1000u / 100u); *e++ = 'K'; *e = 0; }
        else numu(b, hz);
    } else if (idx == 16u) { char *e; fmt_signed(b, ((int32_t)v - 32768) * 40 / 32768); for (e = b; *e; e++) {} *e++ = 'D'; *e++ = 'B'; *e = 0; }
    else if (idx == 100u) fmt_signed(b, ((int32_t)knob[sel][0] - 128) * 12 / 128);
    else if (idx == 101u) { char *e = numu(b, knob[sel][1] * 100u / 255u); *e++ = '%'; *e = 0; }
    else numu(b, v * 99u / 65535u);
}
static int is_freq(uint32_t idx) { return idx == 1u || idx == 5u || idx == 8u || idx == 15u || idx == 100u; }

static uint32_t pv_col(uint32_t x)                                  /* the sound's loudness at column x of 112 (two of the 224 together) */
{
    uint32_t a = pv_peak[2u * x], b = pv_peak[2u * x + 1u];
    return a > b ? a : b;
}
static void draw_scope(int chart)
{
    uint32_t x;
    if (!chart) {                                                       /* a CRT */
        rrect(3, 64, 234, 84, 9, K_CREAM); rrect(4, 65, 232, 82, 8, RGB(200, 190, 160)); rrect(8, 69, 224, 74, 6, RGB(4, 16, 10));
        for (x = 8; x < 232u; x += 28u) cv_rect((int32_t)x, 70, 1, 72, RGB(20, 70, 40));
        cv_rect(9, 88, 222, 1, RGB(20, 70, 40)); cv_rect(9, 124, 222, 1, RGB(20, 70, 40)); cv_rect(9, 106, 222, 1, RGB(40, 130, 70));
        if (pv_ok)
            for (x = 0; x < 112u; x++) {
                int32_t h = (int32_t)pv_col(x) * 33 / 32768;
                if (h > 33) h = 33;
                if (h < 1) h = pv_col(x) ? 1 : 0;
                if (!h) continue;
                cv_rect(8 + (int32_t)x * 2, 106 - h, 2, 2 * h + 1, RGB(60, 210, 105));
                cv_rect(8 + (int32_t)x * 2, 106 - h, 2, 1, RGB(190, 255, 205)); cv_rect(8 + (int32_t)x * 2, 106 + h, 2, 1, RGB(190, 255, 205));
            }
    } else {                                                           /* a chart recorder: ink on graph paper */
        int32_t yy;
        cv_rect(3, 64, 234, 84, K_CREAM);
        for (x = 3; x < 237u; x += 14u) cv_rect((int32_t)x, 65, 1, 82, RGB(200, 218, 190));
        for (yy = 64; yy < 148; yy += 14) cv_rect(4, yy, 232, 1, RGB(200, 218, 190));
        cv_rect(3, 64, 8, 84, RGB(222, 212, 180)); cv_rect(229, 64, 8, 84, RGB(222, 212, 180));
        for (yy = 71; yy < 146; yy += 14) { fcirc(7, yy, 2, K_BODY); fcirc(233, yy, 2, K_BODY); }
        cv_rect(11, 106, 218, 1, RGB(130, 150, 130));
        if (pv_ok)
            for (x = 0; x < 112u; x++) {
                int32_t h = (int32_t)pv_col(x) * 33 / 32768;
                if (h > 33) h = 33;
                if (h < 1) h = pv_col(x) ? 1 : 0;
                if (!h) continue;
                cv_rect(8 + (int32_t)x * 2, 106 - h, 2, 2 * h + 1, RGB(120, 135, 205));
                cv_rect(8 + (int32_t)x * 2, 106 - h, 2, 1, RGB(20, 36, 150)); cv_rect(8 + (int32_t)x * 2, 106 + h, 2, 1, RGB(20, 36, 150));
            }
    }
}
static void draw_page_bar(void)                                     /* the sound's name, and which of the six pages: a red lamp */
{
    static const char *const P[6] = {"OSC", "MOD", "NOISE", "ENV", "OUT", "SOUND"};
    uint32_t i;
    char nm[20], *e;
    rrect_o(3, 46, 84, 17, 3, K_CHRD, RGB(16, 16, 20));
    e = nm; *e++ = (char)('0' + (sel + 1u) / 10u % 10u); *e++ = (char)('0' + (sel + 1u) % 10u); *e++ = ' ';
    { const char *k = snd_name(sel); uint32_t n = 0; while (*k && n < 7u) { *e++ = *k++; n++; } *e = 0; }
    txt(9, 48, nm, K_CREAM);
    rrect_o(90, 46, 147, 17, 3, K_CHR, RGB(226, 214, 180));
    txt(98, 48, P[pg], RGB(20, 22, 36));
    for (i = 0; i < 6u; i++) {
        int32_t x = 156 + (int32_t)i * 13;
        cv_rect(x, 50, 9, 9, i == pg ? RGB(230, 50, 40) : RGB(176, 164, 132));
        if (i == pg) frame(x - 1, 49, 11, 11, RGB(120, 20, 16));
    }
}
static void snd_card(uint32_t i, const pm_t *m, int paint_ctrl)    /* paint_ctrl is the card row; the name plate is part of it */
{
    int32_t x = 3 + (int32_t)i * 59, cx = x + 28;
    uint32_t idx = m->idx;
    char b[10];
    (void)paint_ctrl;
    if (idx == 255u) { cv_rect(x, 150, 57, 65, K_BODY2); frame(x, 150, 57, 65, RGB(44, 110, 102)); return; }
    rrect_o(x, 150, 57, 17, 3, K_CHRD, K_CREAM);
    txtc(cx, 152, m->name, RGB(20, 22, 36));
    rrect_o(x + 3, 198, 51, 17, 3, K_CHRD, K_NAVY);
    if (disc_of(idx) >= 0) {
        uint32_t v = kp[sel][side][idx], seg = v < 21845u ? 0u : v < 43691u ? 1u : 2u, k;
        for (k = 0; k < 3u; k++) {
            int32_t lx = cx - 17 + (int32_t)k * 17;
            fcirc(lx, 182, 7, K_CHR); ring(lx, 182, 7, K_CHRD);
            fcirc(lx, 182, 5, k == seg ? RGB(60, 220, 100) : RGB(20, 60, 44));
            if (k == seg) cv_rect(lx - 2, 179, 2, 2, RGB(230, 255, 235));
        }
        txtc(cx, 200, DISC[disc_of(idx)][seg], K_CREAM);
        return;
    }
    param_text(b, idx, idx == 100u || idx == 101u ? 0u : kp[sel][side][idx]);
    if (is_freq(idx)) gauge(cx, 182, 15, (uint32_t)(dv[i] >> 4));
    else vu(x, (uint32_t)(dv[i] >> 4));
    txtc(cx, 200, b, K_CREAM);
}
static void snd_body(void) { draw_page_bar(); draw_scope(pg >= 3u); }
static void snd_ctrl(void) { uint32_t i; for (i = 0; i < 4u; i++) snd_card(i, &PG[pg].p[i], 1); }

/* ---- the SEND screen's own pieces: the PO-32 as TE draws it, and the glass of the tube */
#define CR_G  RGB(120, 255, 150)
#define CR_GM RGB(60, 180, 96)
#define CR_GD RGB(20, 70, 38)
#define CR_AM RGB(255, 184, 56)
#define CR_BK RGB(3, 12, 7)
static void po32_key(int32_t cx, int32_t cy, uint16_t line, uint16_t fill, uint32_t kind)     /* a button: 0 plain, 1 hatched (acc, FX, write), 2 play */
{
    fcirc(cx, cy, 4, line); fcirc(cx, cy, 3, fill);
    if (kind == 1u) { cv_line(cx - 2, cy + 2, cx + 2, cy - 2, line); cv_line(cx - 2, cy, cx, cy - 2, line); cv_line(cx, cy + 2, cx + 2, cy, line); }
    else if (kind == 2u) { cv_rect(cx - 1, cy - 2, 1, 5, line); cv_rect(cx, cy - 1, 1, 3, line); cv_rect(cx + 1, cy, 1, 1, line); }
}
/* the PO-32 Tonic, 60 x 124: the hanger with its slot, the body, the lock tab, the LCD between its two pin rows (a play mark, two digits, a level
 * bar), the mic and the logo, the three round buttons and the knobs A and B, a 4 x 4 grid of keys and the column acc / FX / play / write. */
static void po32_pic(int32_t px, int32_t py, uint32_t live, uint32_t fr)
{
    static const int8_t KX[5] = {8, 19, 30, 41, 52}, KY[4] = {77, 89, 102, 114};
    static const char *const D2[4] = {"16", "08", "12", "03"};
    const uint16_t line = live ? CR_G : CR_GM, lit = live ? CR_G : CR_GM;
    uint32_t i, j, lv = live ? 2u + (fr * 5u + (fr >> 2) * 3u) % 12u : 0u;
    rrect(px + 8, py, 44, 18, 8, line); rrect(px + 9, py + 1, 42, 16, 7, CR_BK);            /* the hanger and its slot */
    rrect(px + 13, py + 5, 34, 8, 3, line); rrect(px + 14, py + 6, 32, 6, 2, CR_BK);
    cv_rect(px + 21, py + 17, 3, 3, line); cv_rect(px + 36, py + 17, 3, 3, line);
    rrect(px, py + 19, 60, 105, 7, line); rrect(px + 1, py + 20, 58, 103, 6, CR_BK);        /* the body */
    cv_rect(px, py + 34, 4, 8, line); cv_rect(px + 1, py + 35, 2, 6, CR_BK);                /* the lock tab */
    fcirc(px + 30, py + 25, 3, line); fcirc(px + 30, py + 25, 1, CR_BK); cv_rect(px + 20, py + 24, 6, 3, line);        /* the logo */
    fcirc(px + 53, py + 24, 2, line); cv_rect(px + 53, py + 26, 1, 3, line);                /* the mic */
    cv_rect(px + 4, py + 30, 53, 27, line); cv_rect(px + 5, py + 31, 51, 25, CR_BK);        /* the LCD */
    for (i = 0; i < 17u; i++) { cv_rect(px + 6 + (int32_t)i * 3, py + 28, 2, 1, CR_GM); cv_rect(px + 6 + (int32_t)i * 3, py + 58, 2, 1, CR_GM); }
    for (i = 0; i < 4u; i++) cv_rect(px + 8 + (int32_t)i, py + 34 + (int32_t)i, 1, 7 - 2 * (int32_t)i, lit);          /* a play mark */
    ttx(px + 41, py + 34, D2[(fm1_ms / 600u) & 3u], lit, 1);
    for (i = 0; i < 16u; i++) cv_rect(px + 7 + (int32_t)i * 3, py + 48, 2, 5, i < lv ? (i > 12u ? CR_AM : CR_G) : CR_GD);
    for (i = 0; i < 3u; i++) po32_key(px + KX[i], py + 65, line, CR_BK, 0u);               /* sound, pattern, bpm */
    for (i = 0; i < 2u; i++) {                                                             /* the knobs A and B */
        int32_t kx = px + KX[3 + i], ky = py + 65, nx, ny;
        uint32_t ph = (uint32_t)(0xC0000000u + (live ? fm1_ms * (1500u + i * 900u) : 0u));
        rrect(kx - 5, ky - 6, 10, 12, 2, line); rrect(kx - 4, ky - 5, 8, 10, 1, CR_BK);
        fcirc(kx, ky, 3, line); fcirc(kx, ky, 2, CR_BK);
        phpt(kx, ky, 2, ph, &nx, &ny); cv_line(kx, ky, nx, ny, live ? CR_G : CR_GM);
    }
    for (i = 0; i < 4u; i++)
        for (j = 0; j < 5u; j++) {
            int on = j < 4u && live && ((fr * 7u + i * 5u + j * 3u) * 2654435761u >> 28 & 1u);
            po32_key(px + KX[j], py + KY[i], line, on ? CR_G : (j < 4u ? CR_GD : CR_BK), j < 4u ? 0u : (i == 2u ? 2u : 1u));
        }
}
/* the bulge: the tube's glass is round, so the picture swells in the middle and its straight lines bow outward. Every pixel of the picture is taken from
 * a point nearer the centre (by BUL_A/256 in the middle, none at the corners), so the picture is magnified in the middle and the lines curve.
 * It is done in place and needs no second picture: the top half is made from the row it is on or the rows below it, the bottom half from those above, so the
 * screen's two sections (split on its centre line) each stand alone. Rows are made nearest the centre last. */
#define BUL_A 245
static uint16_t VX[220], VY[142];
static void crt_tabs(void)
{
    static int vok;
    int32_t X, Y;
    if (vok) return;
    for (X = 0; X < 220; X++) VX[X] = (uint16_t)((X - 110) * (X - 110) * 256 / (110 * 110));
    for (Y = 0; Y < 142; Y++) VY[Y] = (uint16_t)((Y - 71) * (Y - 71) * 256 / (71 * 71));
    vok = 1;
}
static void crt_bulge(void)
{
    static uint16_t cur[220];
    int32_t Y, X, a = -cv_oy, b = -cv_oy + (int32_t)cv_h - 1, y0, y1, step, lo, hi;
    uint32_t pass;
    crt_tabs();
    for (pass = 0; pass < 2u; pass++) {
        if (pass == 0u) { y0 = b < 141 ? b : 141; y1 = a > 71 ? a : 71; step = -1; lo = y1; hi = y0; }              /* bottom half, from the bottom up */
        else { y0 = a > 0 ? a : 0; y1 = b < 70 ? b : 70; step = 1; lo = y0; hi = y1; }                           /* top half, from the top down */
        if (y0 > 141 || y1 < 0 || (step < 0 ? y0 < y1 : y0 > y1)) continue;
        for (Y = y0; ; Y += step) {
            uint16_t *row = &cv_px[(uint32_t)(Y + cv_oy) * cv_w + (uint32_t)cv_ox];
            int32_t dy = Y - 71;
            for (X = 0; X < 220; X++) cur[X] = row[X];
            for (X = 0; X < 220; X++) {
                int32_t f = BUL_A + (((int32_t)VX[X] + (int32_t)VY[Y]) * (256 - BUL_A) >> 9);
                int32_t xp = 110 + (((X - 110) * f + 128) >> 8), yp = 71 + ((dy * f + 128) >> 8);
                if (yp < lo) yp = lo; else if (yp > hi) yp = hi;
                row[X] = yp == Y ? cur[xp] : cv_px[(uint32_t)(yp + cv_oy) * cv_w + (uint32_t)(cv_ox + xp)];
            }
            if (Y == y1) break;
        }
    }
}
/* the glass of the tube, over the CRT's 220 x 142 (the drawing's origin is the screen's): every pixel keeps what was drawn, a bright one bleeds a
 * soft glow into its four neighbours (phosphor bloom), every other row is darker (the scanlines), and the light falls off toward the corners.
 * Rows are done top to bottom from copies of the row above, so a pixel never blooms from an already-glowed neighbour. */
static void crt_glass(void)
{
    static uint16_t pr[222], cu[222];
    int32_t X, Y, have = 0;
    crt_tabs();
    for (Y = 0; Y < 142; Y++) {
        int32_t j = Y + cv_oy;
        uint16_t *row;
        const uint16_t *nx = 0;
        uint32_t odd = (uint32_t)Y & 1u, vy = VY[Y];
        if (j < 0 || j >= (int32_t)cv_h) { have = 0; continue; }
        row = &cv_px[(uint32_t)j * cv_w + (uint32_t)cv_ox];
        for (X = 0; X < 220; X++) cu[X + 1] = swap16(row[X]);
        cu[0] = cu[1]; cu[221] = cu[220];
        if (!have) for (X = 0; X < 222; X++) pr[X] = cu[X];
        {                                                                   /* the row below, if it is on the canvas */
            static uint16_t dn[222];
            if (Y + 1 < 142 && j + 1 < (int32_t)cv_h) { const uint16_t *q = &cv_px[(uint32_t)(j + 1) * cv_w + (uint32_t)cv_ox]; for (X = 0; X < 220; X++) dn[X + 1] = swap16(q[X]); dn[0] = dn[1]; dn[221] = dn[220]; }
            else for (X = 0; X < 222; X++) dn[X] = cu[X];
            nx = dn;
            for (X = 0; X < 220; X++) {
                uint32_t c = cu[X + 1], l = cu[X], r = cu[X + 2], u = pr[X + 1], d = nx[X + 1], rr, gg, bb, k;
                rr = c >> 11; gg = (c >> 5) & 63u; bb = c & 31u;
                if (l != c || r != c || u != c || d != c) {                 /* a bright neighbour glows into this pixel */
                    uint32_t n = l, ng = (l >> 5) & 63u, t;
                    t = (r >> 5) & 63u; if (t > ng) { n = r; ng = t; }
                    t = (u >> 5) & 63u; if (t > ng) { n = u; ng = t; }
                    t = (d >> 5) & 63u; if (t > ng) { n = d; ng = t; }
                    if (ng > gg) {
                        t = (n >> 11) * 100u >> 8; if (t > rr) rr = t;
                        t = ng * 100u >> 8; if (t > gg) gg = t;
                        t = (n & 31u) * 100u >> 8; if (t > bb) bb = t;
                    }
                }
                k = 256u - (((uint32_t)VX[X] + vy) * 58u >> 8);             /* the corners get 55 % of the light */
                if (odd) k = k * 186u >> 8;                                 /* a scanline: 73 % */
                row[X] = swap16((uint16_t)((rr * k >> 8) << 11 | (gg * k >> 8) << 5 | (bb * k >> 8)));
            }
        }
        for (X = 0; X < 222; X++) pr[X] = cu[X];
        have = 1;
    }
}
/* ---- SEND: a CRT monitor ---- */
static void draw_send(void)
{
    static const char *const RN[4] = {"SOUNDS", "PATTERNS", "CHAIN", "STATE"};
    const uint16_t G = RGB(120, 255, 150), GM = RGB(60, 180, 96), GD = RGB(20, 70, 38), AM = RGB(255, 184, 56), BK = RGB(3, 12, 7);
    uint32_t i, pct = sending && tx.bits ? (uint32_t)((uint64_t)tx.bit * 100u / tx.bits) : 0u, fr = fm1_ms / 100u;
    int32_t sx = cv_ox, sy = cv_oy;
    rrect_o(2, 46, 236, 169, 16, K_CHR, RGB(62, 58, 54)); rrect_o(5, 49, 230, 163, 14, RGB(34, 32, 30), RGB(62, 58, 54));
    rrect(10, 53, 220, 142, 16, BK);
    cv_ox = sx + 10; cv_oy = sy + 53;                               /* the screen's own coordinates */
    cv_text(23, 9, &FONT_S, "PO-32 TRANSFER", RGB(90, 56, 12)); cv_text(22, 8, &FONT_S, "PO-32 TRANSFER", AM);
    for (i = 0; i < 4u; i++) {
        const char *st = "--";
        uint16_t col = GM;
        uint32_t a = i == 0 ? 0u : i == 1 ? 30u : i == 2 ? 65u : 80u, b = i == 0 ? 30u : i == 1 ? 65u : i == 2 ? 80u : 100u;
        int32_t y = 25 + (int32_t)i * 14;
        if (sending && pct >= b) { st = "OK"; col = G; } else if (sending && pct >= a) { st = ">>"; col = AM; }
        cv_text(14, y, &FONT_S, RN[i], sending && st[0] != '-' ? G : GM);
        cv_text(112 - text_w(&FONT_S, st), y, &FONT_S, st, col);
    }
    po32_pic(148, 9, sending, fr);
    cv_rect(14, 94, 120, 1, GD);                                        /* the signal: its phase flips */
    if (sending) {
        uint32_t ph = 0;
        int32_t x, lx = 14, ly = 94, off = (int32_t)(fm1_ms / 25u);
        for (x = 14; x < 134; x++) {
            uint32_t slot = (uint32_t)(x + off) / 14u;
            int32_t y;
            if (((uint32_t)(x + off) % 14u) == 0u && (slot * 2654435761u >> 30 & 1u)) ph += 0x80000000u;
            y = 94 + (int32_t)(((int64_t)8 * sinq((uint32_t)(x + off) * 125000000u + ph)) >> 30);
            if (x > 14) cv_line(lx, ly, x, y, G);
            lx = x; ly = y;
        }
    } else cv_rect(14, 94, 120, 1, GM);
    cv_rect(14, 108, 100, 12, GM); cv_rect(15, 109, 98, 10, BK);
    for (i = 0; i < 19u; i++) if (i * 100u / 19u < pct) cv_rect(16 + (int32_t)i * 5, 111, 4, 6, i >= 16u ? AM : G);
    { char t[6], *e = numu(t, pct); *e++ = '%'; *e = 0; cv_text(140 - text_w(&FONT_S, t), 110, &FONT_S, t, sending ? G : GM); }
    if (sending) cv_rect(14, 126, 8, 8, AM);
    else { for (i = 0; i < 6u; i++) cv_rect(14 + (int32_t)i, 124 + (int32_t)i, 1, 12 - 2 * (int32_t)i, AM); }
    cv_text(28, 122, &FONT_S, sending ? "STOP" : "SEND", AM);
    if (send_ask) {                                                    /* a window: the buttons may be named inside it; it grows in */
        uint32_t el = fm1_ms - ask_t, g = el >= 140u ? 100u : 30u + el * 70u / 140u;
        int32_t bw = (int32_t)(180u * g / 100u), bh = (int32_t)(84u * g / 100u), bx = 110 - bw / 2, by = 72 - bh / 2;
        tint(0, 0, 220, 142, C_BLACK, 170);
        cv_rect(bx, by, bw, bh, AM); cv_rect(bx + 2, by + 2, bw - 4, bh - 4, BK);
        if (g < 100u) goto ask_done;
        cv_text(110 - text_w(&FONT_S, "SEND TO PO-32?") / 2, 38, &FONT_S, "SEND TO PO-32?", AM);
        cv_rect(34, 70, 8, 14, AM); cv_line(42, 70, 56, 60, AM); cv_line(56, 60, 56, 94, AM); cv_line(56, 94, 42, 84, AM);
        for (i = 0; i < 2u; i++) { int32_t r = 8 + (int32_t)i * 7; cv_line(62 + (int32_t)i * 6, 77 - r / 2, 64 + (int32_t)i * 6, 77, AM); cv_line(64 + (int32_t)i * 6, 77, 62 + (int32_t)i * 6, 77 + r / 2, AM); }
        cv_text(96, 64, &FONT_S, "OCT+  Yes", G); cv_text(96, 86, &FONT_S, "OCT-  No", G);
    }
ask_done:
    {                                                                   /* the glass */
        int32_t k, ry = (int32_t)(fm1_ms / 24u % 190u) - 24;
        crt_bulge();                                                    /* the picture swells in the middle */
        crt_glass();                                                    /* bloom, scanlines, the light falling off */
        if (sending && !send_ask) tint(0, ry, 220, 10, G, 16);          /* the rolling bar of a sweep */
        for (k = 0; k < 9; k++) tint(12, 8 + k, 58 - k * 6, 1, C_WHITE, 14);          /* a reflection in the corner */
    }
    cv_ox = sx; cv_oy = sy;
    fcirc(22, 205, 3, RGB(60, 200, 90)); ring(22, 205, 3, K_CHR);
    ttc(118, 201, "DRUMLAB", RGB(200, 194, 170));
    for (i = 0; i < 2u; i++) { int32_t kx = 196 + (int32_t)i * 18; fcirc(kx, 204, 5, K_CHR); ring(kx, 204, 5, K_CHRD); cv_line(kx, 204, kx + 3, 200, RGB(40, 40, 50)); }
}

/* ---- the project picker and help (dark, as Chipbox's) ---- */
/* the project picker: twelve index cards of punched paper, each a picture of its first pattern; the one you point at glows like the seek head */
static void draw_pick(void)
{
    uint32_t s, l, c;
    cv_rect(0, 44, 240, 172, SK.bg);
    for (s = 0; s < 12u; s++) {
        int32_t x = 4 + (int32_t)(s % 3u) * 78, y = 49 + (int32_t)(s / 3u) * 41;
        uint16_t rows[4];
        char b[4];
        int ex = (s == lab_slot) || lab_exists(s);
        lab_preview(s, rows);
        if (!ex) {                                                      /* a blank card: nothing on it yet */
            rrect_o(x, y, 74, 36, 4, s == pick_sel ? K_CORAL : RGB(70, 68, 84), K_BG);
            ttc(x + 37, y + 14, "NEW", RGB(110, 108, 124));
            continue;
        }
        cv_rect(x + 2, y + 3, 74, 36, K_SHAD);
        rrect(x, y, 74, 36, 4, (s & 1u) ? K_CREAM : K_BAR);
        rrect(x + 3, y + 3, 17, 14, 3, s == lab_slot ? K_CORAL : RGB(120, 118, 134));
        numu(b, s + 1u);
        ttx(x + 11 - tw(b, 1) / 2, y + 7, b, s == lab_slot ? K_CREAM : K_BG, 1);
        for (l = 0; l < 4u; l++)
            for (c = 0; c < 16u; c++)
                if (rows[l] >> c & 1u) cv_rect(x + 23 + (int32_t)c * 3, y + 4 + (int32_t)l * 8, 2, 6, INKC[l]);
        if (s == 0u && demo0) { cv_rect(x + 7, y + 25, 9, 6, K_BG); frame(x + 9, y + 20, 5, 6, K_BG); }      /* KOROBEINIKI: a padlock */
        for (c = 0; c < 8u; c++) cv_rect(x + 3 + (int32_t)c * 3 + 0, y + 29, 1, 1, RGB(140, 130, 100));
        for (c = 0; c < 4u; c++) { fcirc(x + 70, y + 7 + (int32_t)c * 8, 1, K_BG); }
    }
    {                                                                   /* the seek head's glow on the card you point at */
        int32_t x = 4 + (int32_t)(pick_sel % 3u) * 78, y = 49 + (int32_t)(pick_sel / 3u) * 41;
        tint(x - 3, y - 3, 80, 42, RGB(255, 225, 110), 46);
        frame(x - 2, y - 2, 78, 40, K_CORAL); frame(x - 3, y - 3, 80, 42, K_CORAL);
    }
    if (del_ask) {                                                      /* a window: the buttons may be named inside it */
        rrect_o(30, 80, 180, 86, 6, SK.rec, SK.bg);
        txtc(120, 94, "Delete project?", SK.text);
        txtc(120, 124, "OCT+  Yes", SK.text); txtc(120, 144, "OCT-  No", SK.text);
    }
}

/* the kits window: a rolled kit, LAB-1 and the recipes; the sounds already sound as the highlighted kit */
static void draw_kits(void)
{
    uint32_t i, top = kit_sel < 2u ? 0u : kit_sel - 2u;
    char t[8];
    tint(0, 44, 240, 172, C_BLACK, 150);
    rrect_o(12, 50, 216, 164, 6, K_CORAL, K_BG);
    txt(24, 55, "KITS", K_AMB);
    if (top + 6u > NKL) top = NKL - 6u;
    for (i = 0; i < 6u && top + i < NKL; i++) {
        uint32_t k = top + i, on = k == kit_sel;
        int32_t y = 76 + (int32_t)i * 17;
        const char *n = k == 0u ? kit_title : k == 1u ? "LAB-1" : kit_def[k - 2u].name;
        rrect_o(20, y, 200, 16, 3, on ? K_CREAM : K_CHRD, on ? RGB(58, 52, 74) : K_PLATE);
        if (k == 0u) {                                                    /* a die */
            {
                static const uint8_t PX[7][2] = {{1, 1}, {7, 1}, {1, 7}, {7, 7}, {4, 4}, {1, 4}, {7, 4}};
                static const uint8_t FACE[6] = {0x10, 0x09, 0x19, 0x0F, 0x1F, 0x6F};          /* 1 .. 6; bits as PX */
                uint32_t face = fm1_ms - kit_roll_t < 500u ? (fm1_ms / 70u) % 6u : 2u, q;
                rrect_o(26, y + 3, 11, 11, 2, K_CREAM, K_CREAM);
                for (q = 0; q < 7u; q++) if (FACE[face] >> q & 1u) cv_rect(27 + PX[q][0], y + 4 + PX[q][1], 2, 2, K_BG);
            }
            txt(42, y + 2, n, on ? K_CREAM : RGB(200, 194, 170));
            t[0] = '#'; t[1] = "0123456789ABCDEF"[kit_seed >> 12 & 15u]; t[2] = "0123456789ABCDEF"[kit_seed >> 8 & 15u]; t[3] = "0123456789ABCDEF"[kit_seed >> 4 & 15u]; t[4] = "0123456789ABCDEF"[kit_seed & 15u]; t[5] = 0;
            txt(212 - text_w(&FONT_S, t), y + 2, t, RGB(150, 144, 126));
        } else {
            int ic = name_is(n, "ACID") ? 1 : name_is(n, "CHIP") ? 2 : 0;
            if (ic == 1) smiley(26, y + 2); else if (ic == 2) chip_icon(26, y + 2);
            txt(ic ? 42 : 30, y + 2, n, on ? K_CREAM : RGB(200, 194, 170));
        }
    }
    txt(24, 182, "OCT+ Keep", K_MINT); txt(112, 182, "OCT- Discard", RGB(255, 150, 130));
    if (kit_sel == 0u) txt(24, 196, "PLAY Roll the dice", K_AMB);
}

/* the system menu (GLO + HOME) */
/* the Bluetooth page: the same words on all three brothers */
static void draw_bt(void)
{
    static const char *const MODE[3] = {"OFF", "LEAD", "FOLLOW"};
    static const char *const STAT[3][3] = {{"OFF", "OFF", "OFF"}, {"READY", "READY", "LEADING"}, {"SEARCHING", "LINKED", "IN SYNC"}};
    static const char *const ON[2] = {"OFF", "ON"};
    const char *lab[6] = {"SYNC", "STATUS", "SIDECHAIN", "PATTERNS", "PUNCH-IN FX", "DEMO"}, *val[6];
    uint32_t i;
    val[0] = MODE[brothers_mode % 3u]; val[1] = STAT[brothers_mode % 3u][brothers_link() % 3u]; val[2] = ON[bt_side & 1u]; val[3] = ON[bt_patsync & 1u]; val[4] = ON[bt_fxsync & 1u]; val[5] = bt_locked ? "LOCKED" : "LOAD";
    cv_rect(0, 44, 240, 172, SK.bg);
    for (i = 0; i < 6u; i++) {
        int32_t y = 48 + (int32_t)i * 28;
        uint32_t row = i == 0u ? 0u : i == 1u ? 9u : i - 1u, on = row == bt_sel;                      /* the status line is only shown */
        rrect_o(8, y, 224, 25, 5, on ? K_CREAM : K_CHRD, on ? RGB(58, 52, 74) : K_PLATE);
        txt(20, y + 6, lab[i], on ? K_CREAM : RGB(200, 194, 170));
        txt(206 - text_w(&FONT_S, val[i]), y + 6, val[i], row == 9u ? RGB(160, 154, 134) : K_AMB);
        if (on) { txt(206 - text_w(&FONT_S, val[i]) - 14, y + 6, "<", K_CORAL); txt(212, y + 6, ">", K_CORAL); }
    }
}
static void draw_lessons(void)                                      /* the lessons list: a tick and mint text on the ones you finished */
{
    uint32_t i, top;
    char t[16], *e = t;
    cv_rect(0, 44, 240, 172, SK.bg);
    top = les_ltop;
    if (les_pick < top) top = les_pick; else if (les_pick >= top + 7u) top = les_pick - 6u;
    les_ltop = top;
    txt(10, 46, "LESSONS", K_AMB);
    e = numu(e, les_ndone()); *e++ = '/'; e = numu(e, LES_ALL); *e = 0;
    txt(228 - text_w(&FONT_S, t), 46, t, RGB(160, 154, 134));
    for (i = 0; i < 7u && top + i < LES_ALL; i++) {
        const uint32_t p = top + i, n = les_at(p), on = p == les_pick, dn = les_isdone(n);
        const int32_t y = 62 + (int32_t)i * 18;
        rrect_o(8, y, 218, 17, 3, on ? K_CREAM : K_CHRD, on ? RGB(58, 52, 74) : K_PLATE);
        if (dn) { cv_rect(16, y + 8, 2, 2, K_MINT); cv_rect(18, y + 10, 2, 2, K_MINT); cv_rect(20, y + 8, 2, 2, K_MINT); cv_rect(22, y + 6, 2, 2, K_MINT); cv_rect(24, y + 4, 2, 2, K_MINT); }
        else { cv_rect(15, y + 3, 11, 1, RGB(90, 86, 100)); cv_rect(15, y + 13, 11, 1, RGB(90, 86, 100)); cv_rect(15, y + 3, 1, 11, RGB(90, 86, 100)); cv_rect(25, y + 3, 1, 11, RGB(90, 86, 100)); }
        txt(34, y + 1, les_name(n), on ? K_CREAM : dn ? K_MINT : les_ismc(n) ? K_AMB : RGB(200, 194, 170));       /* (a masterclass in gold) */
    }
    {   const int32_t th = 7 * 18 - 4, len = th * 7 / (int32_t)LES_ALL, at = (th - len) * (int32_t)top / (int32_t)(LES_ALL - 7u);
        cv_rect(234, 62, 2, th, K_CHRD); cv_rect(234, 62 + at, 2, len, K_AMB);
    }
    txt(10, 194, "OCT+ START", K_AMB); txt(230 - text_w(&FONT_S, "HOME CLOSE"), 194, "HOME CLOSE", RGB(160, 154, 134));
}
static void draw_menu(void)
{
    if (bt_on) { draw_bt(); return; }
    if (les_list_on) { draw_lessons(); return; }
    static const char *const LAB[NMENU] = {"MIDI CLOCK", "MIDI CHANNEL", "LESSONS", "SHOW TIPS", "QUICK START ALWAYS", "PROJECTS AT START", "BATTERY", "RESTORE DEMOS", "FACTORY RESET", "BROTHERS LINK", "ABOUT", "SETTINGS", "SYNC AND LINK", "RESET"};
    static const char *const SYNC[3] = {"OFF", "SEND", "FOLLOW"};
    uint32_t i;
    cv_rect(0, 44, 240, 172, SK.bg);
    uint8_t rows[8];
    const uint32_t vis_n = menu_rows(rows);
    uint32_t sel_vi = 0, k;
    static uint32_t top;                                                                          /* nine lines of 18 px fit: the list scrolls with the bar */
    for (k = 0; k < vis_n; k++) if (rows[k] == menu_sel) sel_vi = k;
    if (sel_vi < top) top = sel_vi;
    if (sel_vi >= top + 9u) top = sel_vi - 8u;
    if (top + 9u > vis_n) top = vis_n > 9u ? vis_n - 9u : 0u;
    for (k = 0; k < vis_n; k++) {
        int32_t y;
        char v[20], *e = v;
        const uint32_t slot = k;
        i = rows[k];
        if (slot < top || slot >= top + 9u) continue;
        y = 46 + (int32_t)(slot - top) * 18;
        uint32_t on = i == menu_sel, sw = i == MR_TIPS || i == MR_QS || i == MR_PICK;
        rrect_o(8, y, 224, 17, 3, on ? K_CREAM : K_CHRD, on ? RGB(58, 52, 74) : K_PLATE);
        txt(20, y + 1, i == MR_DEMO && demo_gone() && !demo_can_restore() ? "RESTORE: SLOT 1 IN USE" : LAB[i], on ? K_CREAM : RGB(200, 194, 170));
        v[0] = 0;
        if (i >= MR_GSET) { if (on) { e[0] = 'O'; e[1] = 'C'; e[2] = 'T'; e[3] = '+'; e[4] = 0; } }
        else if (i == MR_CLOCK) { const char *t = SYNC[lset.sync]; while (*t) *e++ = *t++; *e = 0; }
        else if (i == MR_CHAN) { if (!lset.chan) { e[0] = 'A'; e[1] = 'N'; e[2] = 'Y'; e[3] = 0; } else numu(e, lset.chan); }
        else if (i == MR_LESSONS) { e = numu(e, les_ndone()); *e++ = '/'; e = numu(e, LES_ALL); *e = 0; }
        else if (sw) { uint32_t yes = i == MR_TIPS ? !(lset.pad[0] & QS_NOTIPS) : (lset.pad[0] & (i == MR_QS ? QS_ALWAYS : QS_PICK)) != 0u; const char *t = yes ? "YES" : "NO"; while (*t) *e++ = *t++; *e = 0; }
        else if (i == MR_BAT) { e = numu(e, bat_pct); *e++ = '%'; *e = 0; }
        else if (i == MR_DEMO) { if (on && demo_can_restore()) { e[0] = 'O'; e[1] = 'C'; e[2] = 'T'; e[3] = '+'; e[4] = 0; } }
        else if (i == MR_RESET || i == MR_BT) { if (on) { e[0] = 'O'; e[1] = 'C'; e[2] = 'T'; e[3] = '+'; e[4] = 0; } }
        else { const char *t = "FM-1_032"; while (*t) *e++ = *t++; *e = 0; }
        txt((i == MR_CLOCK || i == MR_CHAN || sw ? 206 : 220) - text_w(&FONT_S, v), y + 1, v, i == MR_ABOUT ? RGB(160, 154, 134) : K_AMB);
        if (on && (i == MR_CLOCK || i == MR_CHAN || sw)) { txt(206 - text_w(&FONT_S, v) - 14, y + 1, "<", K_CORAL); txt(212, y + 1, ">", K_CORAL); }
    }
    if (vis_n > 9u) {                                                                                 /* more lines than fit: a thin bar on the right, the part on show lit */
        const int32_t th = 162 * 9 / (int32_t)vis_n, ty = 46 + (162 - th) * (int32_t)top / (int32_t)(vis_n - 9u);
        cv_rect(236, 46, 2, 162, K_CHRD); cv_rect(236, ty, 2, th, K_AMB);
    }
    if (about_on) {
        rrect_o(20, 56, 200, 150, 6, K_AMB, SK.bg);
        txtc(120, 64, "DRUMLAB", K_AMB);
        txtc(120, 84, "FOR THE PO-32", SK.text);
        txtc(120, 102, "THANKS MICROTONIC", SK.text);
        txtc(120, 120, "THANKS TEENAGE ENG", SK.text);
        chip_icon(72, 150); smiley(150, 150);
        ttc(77, 166, "CHIPBOX", RGB(150, 144, 126)); ttc(155, 166, "SMILEYFACE", RGB(150, 144, 126));
        ttc(120, 184, "THE THREE BROTHERS", K_MINT);
    }
    if (reset_ask) {
        rrect_o(30, 78, 180, 90, 6, SK.rec, SK.bg);
        txtc(120, 92, "Factory reset?", SK.text);
        txtc(120, 124, "OCT+  Yes", SK.text); txtc(120, 146, "OCT-  No", SK.text);
    }
}

/* ---- painting: the screen is four sections, each its own canvas; a change repaints only the sections it touches */
static const int16_t SEC_Y[5] = {0, 44, 150, 216, 240};
static int32_t sec_y(uint32_t i) { return i == 2u && view == V_SEND ? 124 : SEC_Y[i]; }      /* SEND's screen is cut on its centre line */
static void ui_sec(uint32_t s)
{
    int32_t y0 = sec_y(s), y1 = sec_y(s + 1);
    cv_begin(240, (uint32_t)(y1 - y0), lite() ? K_BODY : K_BG);
    cv_oy = -y0; band_y0 = y0; band_y1 = y1;
    if (help_on) scene_man();                                         /* the manual is the whole screen */
    else if (s != 3u && po_active()) draw_po();
    else if (s == 0u) { draw_header(); if (fm1_ms - flash_ms < 500u) cv_rect(8, 40, 30, 2, save_ok ? K_MINT : K_CORAL); }
    else if (s == 3u) draw_tabs();
    else if (menu_on) draw_menu();
    else if (pick_on) draw_pick();
    else if (view == V_PAT) { if (s == 1u) pat_body(); else pat_ctrl(); }
    else if (view == V_SND) { if (s == 1u) snd_body(); else snd_ctrl(); }
    else draw_send();
    if (s < 2u && gl_card()) draw_glcard();                           /* (the card sits over the top, so the knobs stay in view) */
    if (s == 0u && gl_active() && gl_hide) draw_gltab();              /* put away: a small tab stays */
    if ((s == 1u || s == 2u) && gl_reading()) draw_read();            /* a masterclass page */
    if (kit_on && (s == 1u || s == 2u)) draw_kits();
    if (tip_on && (s == 1u || s == 2u)) draw_tip();
    cv_oy = 0;
    cv_blit(0, (uint32_t)y0);
}

/* a piece of the screen: the seek head's column (and the one it left), the atom, the sound map */
static void ui_col(uint32_t s, int run)
{
    int32_t x0 = cell_x(s) - 6;
    cv_begin(24, 106, K_BG);
    cv_ox = -x0; cv_oy = -44; band_y0 = 44; band_y1 = 150;
    draw_rail(); draw_lanes(x0, x0 + 24);
    if (run) draw_head((int32_t)s, 1);
    cv_ox = 0; cv_oy = 0;
    cv_blit((uint32_t)x0, 44);
}
static void ui_atom(void)
{
    uint32_t lt = lite();
    cv_begin(46, 40, lt ? K_CREAM : K_BG);
    cv_oy = 0; band_y0 = 0; band_y1 = 40;
    atom(atom_n(), lt ? K_NAVY : K_BG, lt ? K_TEAL : K_MINT, atom_ph);
    cv_blit(0, 0);
}
static void ui_map(void)
{
    uint32_t lt = lite();
    cv_begin(34, 34, lt ? K_CREAM : K_PLATE);
    cv_ox = -(map_x() - 1); cv_oy = -6;
    keymap(map_x(), 7, lt ? K_CREAM : K_PLATE, lt ? K_NAVY : K_CREAM);
    cv_ox = 0; cv_oy = 0;
    cv_blit((uint32_t)(map_x() - 1), 6);
}

/* the knob row's needles: where each should point (0..255), -1 where there is none; 0 when the row shows something else */
static uint32_t ctrl_targets(int32_t *t)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) t[i] = -1;
    if (help_on || pick_on || menu_on || kit_on || view == V_SEND || fx_layer || les_on) return 0;
    if (view == V_PAT) {
        if (chainm) return 0;
        t[0] = (int32_t)((bpm - 60u) * 255u / 180u); t[1] = (int32_t)((uint32_t)swing * 255u / 12u); t[2] = knob[sel][0]; t[3] = knob[sel][1];
        return 1;
    }
    for (i = 0; i < 4u; i++) {
        uint32_t idx = PG[pg].p[i].idx;
        if (idx == 255u || disc_of(idx) >= 0) continue;
        t[i] = idx == 100u ? knob[sel][0] : idx == 101u ? knob[sel][1] : (int32_t)((uint32_t)kp[sel][side][idx] * 255u / 65535u);
    }
    return 1;
}
static void anim_step(uint32_t dt)                                  /* one frame of the little animations */
{
    int32_t t[4], moved = 0, d, target;
    uint32_t i, show, snap;
    uint8_t cur[7];
    static uint8_t lpg = 99, lview = 99, lbat;
    atom_ph += dt + dt * atom_boost / 24u; if (atom_boost) atom_boost--;
    atom_pulse = fm1_ms - atom_hit_t < 100u ? 1 : 0;
    cur[0] = wr; cur[1] = accm; cur[2] = mutem != 0u; cur[3] = chainm; cur[4] = fx_layer; cur[5] = side == 0u; cur[6] = aud_loop != 0u;
    if (tg_view != view) { tg_view = view; for (i = 0; i < 7u; i++) { tg_prev[i] = cur[i]; tg_t[i] = 6; } }
    for (i = 0; i < 7u; i++) {
        if (cur[i] != tg_prev[i]) { tg_prev[i] = cur[i]; tg_t[i] = 0; dsec |= SEC_HDR; }
        else if (tg_t[i] < 6u) { tg_t[i]++; dsec |= SEC_HDR; }
    }
    show = ctrl_targets(t);
    snap = lpg != pg || lview != view; lpg = pg; lview = view;
    if (show) {
        for (i = 0; i < 4u; i++) {
            if (t[i] < 0) { dv[i] = 0; continue; }
            target = t[i] << 4; d = target - dv[i];
            if (snap) dv[i] = target;
            else if (d > -10 && d < 10) { if (d) { dv[i] = target; moved = 1; } }
            else { dv[i] += d / 3 + (d > 0 ? 1 : -1); moved = 1; }
        }
        if (moved) dsec |= SEC_CTRL;
    }
    target = 28 + (int32_t)view * 80; d = target - tab_x;                      /* the radio needle slides to its view */
    if (d) { tab_x += d > -4 && d < 4 ? d : d / 3; dsec |= SEC_TABS; }
    if (bat_pct < 12u && (uint8_t)(fm1_ms / 400u & 1u) != lbat) { lbat = (uint8_t)(fm1_ms / 400u & 1u); dsec |= SEC_HDR; }
    if (kit_on && kit_sel == 0u && fm1_ms - kit_roll_t < 540u) dsec |= SEC_BODY | SEC_CTRL;          /* the die is rolling */
    if (send_ask && fm1_ms - ask_t < 180u) dsec |= SEC_BODY | SEC_CTRL;                              /* the window grows in */
    { static uint32_t pf; if (po_active() && sq_run && ++pf % 3u == 0u) dsec |= SEC_HDR | SEC_BODY | SEC_CTRL; }       /* the LCD moves */
}

static uint32_t atom_ms, send_ms;
static void ui_draw(void)
{
    uint32_t st = cur_step(), s;
    if (fm1_ms - anim_ms >= 33u) { uint32_t dt = fm1_ms - anim_ms; anim_ms = fm1_ms; anim_step(dt > 200u ? 33u : dt); }
    if (fm1_ms - flash_ms < 520u) flash_drawn = 1;
    else if (flash_drawn) { flash_drawn = 0; dsec |= SEC_HDR; }                            /* the saved mark goes */
    if (view == V_SEND && sending && fm1_ms - send_ms >= 90u) { send_ms = fm1_ms; dsec |= SEC_HDR | SEC_BODY | SEC_CTRL; }       /* its picture moves */
    if (dirty) dsec = 15u;
    if (dsec) {
        uint32_t m = dsec;
        dsec = 0; dirty = 0; map_dirty = 0; last_step = st; atom_ms = fm1_ms;
        for (s = 0; s < 4u; s++) if (m >> s & 1u) ui_sec(s);
        return;
    }
    if (view == V_PAT && !fx_layer && !help_on && !pick_on && !menu_on && !po_active() && !gl_over() && sq_run && st != last_step) {          /* the seek head: one column and the one it left */
        if (last_step < 16u) ui_col(last_step, 0);
        ui_col(st, 1);
        last_step = st;
    } else if (!sq_run) last_step = st;
    if (map_dirty && !help_on && !pick_on && !menu_on && !gl_over() && view != V_PAT) { map_dirty = 0; ui_map(); }
    if (fm1_ms - atom_ms >= 60u) { atom_ms = fm1_ms; if (!po_active() && !help_on && !gl_over()) ui_atom(); }
}

static void ui_leds(void)
{
    uint32_t i, step = cur_step();
    for (i = 0; i < 16u; i++) {
        int on = 0;
        if (view == V_PAT && wr) on = step_has(sel, i) ^ (sq_run && step == i);
        else if (view == V_PAT && sq_run && step == i) on = 1;
        else if (pad_on >> i & 1u && fm1_ms - pad_t[i] < 120u) on = 1;
        fm1_led_key(KEY_LED(WHITE[i]), on);
    }
    fm1_led_key(BTN_REC, wr); fm1_led_key(BTN_ENV, accm); fm1_led_key(BTN_ARP, mutem != 0u); fm1_led_key(BTN_SEL, chainm || (view == V_SND && side));
    fm1_led_key(BTN_PLAY, sq_run || sending); fm1_led_key(BTN_FX, fx_layer);
    fm1_led_key(BTN_SAVE, lab_dirty); fm1_led_key(BTN_GLO, help_on);
}

static void ui_init(void)
{
    skin_init();
    view = V_PAT; sel = 0; pg = 0; side = 0; mute_mask = 0; solo_lane = -1; flash_ms = fm1_ms - 1000u;
    pv_dirty = 1; pv_t = fm1_ms;
}
