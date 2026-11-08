/* SPDX-License-Identifier: GPL-3.0-only */
/* Projects in flash: all 16 sounds (both patches, both knobs), the 16 patterns, the chain, tempo and swing, ~4.5 KB each.
 * Twelve slots of two 12 KB banks in Chipbox's project flash; a save writes the older bank and puts its header last, so a power cut keeps
 * the old version whole. A slot holding a Chipbox project is simply not ours (other magic): it shows as empty, and a save replaces it. */
#define LAB_MAGIC 0x504C4244u                   /* "DBLP" */
#define LAB_SLOTS PROJ_SLOTS
#define LAB_HDR 16u
#define LAB_VER 3u                            /* 2: the patterns also hold their recorded effects; 3: and the sounds' names follow them */
#define LAB_PSZ1 194u                         /* a version 1 pattern: no effects */
#define LAB_PAT0 (5u + CHAIN_MAX + 2u * NS + 2u * NS * DR_PARAMS * 2u)       /* where the patterns start in the payload */

static uint8_t lab_buf[PROJ_BANK];
static uint32_t lab_dirty, lab_t_edit, lab_loaded;
static uint8_t lab_slot;                        /* the open project */
static uint8_t demo0;                           /* slot 1 (index 0) holds KOROBEINIKI: locked, never saved over */
static uint32_t lab_seqflag;                    /* 0x40000000 while the demo is written: its header says so */
typedef struct { uint32_t magic, seq, len, crc; } lab_hdr_t;

static uint32_t lab_crc(const uint8_t *p, uint32_t n)
{
    uint32_t c = 0xFFFFFFFFu, k;
    while (n--) { c ^= *p++; for (k = 0; k < 8u; k++) c = c & 1u ? (c >> 1) ^ 0xEDB88320u : c >> 1; }
    return ~c;
}
static uint32_t lab_base(uint32_t slot, uint32_t bank) { return PROJ_AT + (slot * 2u + bank) * PROJ_BANK; }

static uint32_t lab_pack(uint8_t *p)
{
    uint32_t n = 0, s, i, q;
    p[n++] = LAB_VER; p[n++] = bpm; p[n++] = swing; p[n++] = cur_pat; p[n++] = chain_n;
    for (i = 0; i < CHAIN_MAX; i++) p[n++] = chain[i];
    for (s = 0; s < NS; s++) { p[n++] = knob[s][0]; p[n++] = knob[s][1]; }
    for (s = 0; s < NS; s++)
        for (q = 0; q < 2u; q++)
            for (i = 0; i < DR_PARAMS; i++) { p[n++] = (uint8_t)kp[s][q][i]; p[n++] = (uint8_t)(kp[s][q][i] >> 8); }
    for (s = 0; s < NPAT; s++) { memcpy(p + n, &pat[s], sizeof pat[s]); n += sizeof pat[s]; }
    for (s = 0; s < NS; s++) p[n++] = snm[s];
    return n;
}
static int lab_unpack(const uint8_t *p, uint32_t len)
{
    uint32_t n = 5, s, i, q;
    uint32_t psz = p[0] == 1u ? LAB_PSZ1 : (uint32_t)sizeof(lpat_t);
    if (p[0] < 1u || p[0] > LAB_VER || len < LAB_PAT0 + NPAT * psz) return -1;
    bpm = p[1]; swing = p[2] > 12u ? 12u : p[2]; cur_pat = p[3] & 15u; chain_n = p[4] > CHAIN_MAX ? CHAIN_MAX : p[4];
    for (i = 0; i < CHAIN_MAX; i++) chain[i] = p[n++] & 15u;
    for (s = 0; s < NS; s++) { knob[s][0] = p[n++]; knob[s][1] = p[n++]; }
    for (s = 0; s < NS; s++)
        for (q = 0; q < 2u; q++)
            for (i = 0; i < DR_PARAMS; i++) { kp[s][q][i] = (uint16_t)(p[n] | p[n + 1u] << 8); n += 2u; }
    for (s = 0; s < NPAT; s++) { memset(&pat[s], 0, sizeof pat[s]); memcpy(&pat[s], p + n, psz); n += psz; }
    for (s = 0; s < NS; s++) snm[s] = p[0] >= 3u && len >= n + NS && p[n + s] <= 32u ? p[n + s] : 0u;
    tempo_set(bpm);
    return 0;
}

static int lab_header(uint32_t slot, uint32_t bank, lab_hdr_t *h)
{
    if (pf_read(lab_base(slot, bank), h, sizeof *h)) return 0;
    return h->magic == LAB_MAGIC && h->len > 0u && h->len <= PROJ_BANK - LAB_HDR;
}
static int lab_newest(uint32_t slot, lab_hdr_t *h)
{
    lab_hdr_t a, b;
    int ha = lab_header(slot, 0, &a), hb = lab_header(slot, 1, &b);
    if (ha && hb) { if ((int32_t)((b.seq & 0x3FFFFFFFu) - (a.seq & 0x3FFFFFFFu)) > 0) { *h = b; return 1; } *h = a; return 0; }
    if (ha) { *h = a; return 0; }
    if (hb) { *h = b; return 1; }
    return -1;
}
static int lab_exists(uint32_t slot) { lab_hdr_t h; return lab_newest(slot, &h) >= 0; }
static int lab_load(uint32_t slot)
{
    lab_hdr_t h;
    int b = lab_newest(slot, &h);
    uint32_t tries;
    for (tries = 0; b >= 0 && tries < 2u; tries++, b ^= 1) {
        if (tries && !lab_header(slot, (uint32_t)b, &h)) continue;
        if (pf_read(lab_base(slot, (uint32_t)b) + LAB_HDR, lab_buf, h.len) == 0 && lab_crc(lab_buf, h.len) == h.crc && lab_unpack(lab_buf, h.len) == 0) return 0;
    }
    return -1;
}
static int lab_store(uint32_t slot, const uint8_t *buf, uint32_t len)     /* a project's bytes into a slot: the older bank, the header last */
{
    lab_hdr_t h, nh;
    int b = lab_newest(slot, &h);
    uint32_t bank = b < 0 ? 0u : (uint32_t)(b ^ 1), i, sectors, base = lab_base(slot, bank);
    sectors = (LAB_HDR + len + 0xFFFu) / 0x1000u;
    for (i = 0; i < sectors; i++) if (pf_erase(base + i * 0x1000u)) return -1;
    if (pf_write(base + LAB_HDR, buf, len)) return -1;
    nh.magic = LAB_MAGIC; nh.seq = (b < 0 ? 1u : ((h.seq & 0x3FFFFFFFu) + 1u) & 0x3FFFFFFFu) | lab_seqflag; nh.len = len; nh.crc = lab_crc(buf, len);
    if (pf_write(base, &nh, sizeof nh)) return -1;
    return 0;
}
static uint8_t bt_locked;                /* the Bluetooth demo is on: a locked project, in memory only (nothing of it is saved; opening a project ends it) */
static int lab_save(uint32_t slot)       /* stopped and silent: an erase stalls the sound */
{
    if (bt_locked || (slot == 0u && demo0)) return 0;           /* the Bluetooth demo and KOROBEINIKI are never saved */
    return lab_store(slot, lab_buf, lab_pack(lab_buf));
}
static void lab_clear(uint32_t slot) { pf_erase(lab_base(slot, 0)); pf_erase(lab_base(slot, 1)); }

/* what a slot looks like in the picker: a bit per step for each lane of its first pattern */
static void lab_preview(uint32_t slot, uint16_t rows[4])
{
    lab_hdr_t h;
    uint8_t t[64];
    uint32_t l, s;
    int b;
    for (l = 0; l < 4u; l++) rows[l] = 0;
    b = lab_newest(slot, &h);
    if (b < 0 || h.len < LAB_PAT0 + 64u || pf_read(lab_base(slot, (uint32_t)b) + LAB_HDR + LAB_PAT0, t, 64)) return;
    for (l = 0; l < 4u; l++) for (s = 0; s < 16u; s++) if (t[l * 16u + s] & 15u) rows[l] |= (uint16_t)(1u << s);
}

static void lab_edited(void) { lab_dirty = 1; lab_t_edit = fm1_ms; }
/* what is remembered across power-offs (one flash sector, 8 bytes): the open project, MIDI sync and channel, a few switches */
typedef struct { uint8_t magic, slot, sync, chan, flags, pad[3]; } lset_t;
#define LSET_MAGIC 0xD3u
#define LF_PO 1u                                /* the pocket operator's LCD skin */
#define LF_NOBAT 2u
static lset_t lset = {LSET_MAGIC, 0, 0, 0, 0, {0, 0, 0}};      /* sync: 0 off, 1 send clock, 2 follow clock; chan: 0 any, 1..16 */
static void lab_slot_note(void) { lset.slot = lab_slot; settings_save(&lset, sizeof lset); }            /* which project was open: it opens again next time */
static void demo_install(void);
static void demo_sync0(void);
static void proj_boot(void)
{
    lset_t t;
    int fresh = 1;
    if (settings_load(&t, sizeof t) == 0 && t.magic == LSET_MAGIC && t.slot < LAB_SLOTS && t.sync < 3u && t.chan < 17u) { lset = t; fresh = 0; }
    if (fresh) { demo_install(); settings_save(&lset, sizeof lset); }                  /* a new device: the demos go into the empty slots */
    else demo_sync0();                                                                  /* slot 1 is always KOROBEINIKI: made again if it is not exactly this firmware's */
    lab_slot = lset.slot; sync_mode = lset.sync;
    bt_side = lset.flags >> 2 & 1u; bt_patsync = lset.flags >> 3 & 1u; bt_fxsync = lset.flags >> 4 & 1u;            /* Bluetooth sync: bits 0-1 the mode, 2 sidechain, 3 follow patterns */
    lab_loaded = lab_load(lab_slot) == 0;
    lab_dirty = 0;
}
static void lab_open(uint32_t slot)       /* switch projects: what is unsaved is kept, a new slot starts from the kit */
{
    seq_stop();
    if (lab_dirty) { lab_save(lab_slot); lab_dirty = 0; }
    bt_locked = 0;
    lab_slot = (uint8_t)slot;
    if (lab_load(slot) != 0) core_init();
    lab_loaded = lab_exists(slot);
    lab_dirty = 0;
    lab_slot_note();
}
