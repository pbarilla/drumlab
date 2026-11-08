/* SPDX-License-Identifier: GPL-3.0-only */
/* The imported sound against libpo, end to end: records on stdin, each the drum's text (what libpo imports) then a line "#U" + the 21 u16 our
 * JavaScript importer made from the same file, then a line holding one form feed. libpo: text -> float synth. Ours: those u16 -> drum.c.
 * Prints per record: spectral deviation (dB, the metric of tests/drum/sweep.py: 1024-point Hann frames, 8-bin bands), level deviation (dB), tail
 * (dB, what libpo still plays after we stopped), libpo's peak. Run through tests/test_mtaccuracy.mjs.
 *   L=<libpo>/src/core; cc -O2 -I$L/include -I$L/src -Ifirmware/src -Ifirmware/gen -o imp_cmp $L/src/po32.c $L/src/po32_synth.c $L/src/po32_patch_import.c firmware/src/drum.c tests/drum/imp_cmp.c -lm */
#include "po32.h"
#include "po32_synth.h"
#include "drum.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 132300
#define FR 1024

static void fft(double *re, double *im)                  /* in place, FR points */
{
    int i, j, k, m;
    for (i = 1, j = 0; i < FR; i++) {
        int bit = FR >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { double t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (m = 2; m <= FR; m <<= 1) {
        double a = -2 * M_PI / m;
        for (k = 0; k < FR; k += m)
            for (j = 0; j < m / 2; j++) {
                double wr = cos(a * j), wi = sin(a * j), xr = re[k + j + m / 2] * wr - im[k + j + m / 2] * wi, xi = re[k + j + m / 2] * wi + im[k + j + m / 2] * wr;
                re[k + j + m / 2] = re[k + j] - xr; im[k + j + m / 2] = im[k + j] - xi; re[k + j] += xr; im[k + j] += xi;
            }
    }
}
static void spec(const double *x, double *mag)           /* Hann window, magnitudes of bins 0..FR/2 */
{
    static double re[FR], im[FR];
    int i;
    for (i = 0; i < FR; i++) { re[i] = x[i] * (0.5 - 0.5 * cos(2 * M_PI * i / FR)); im[i] = 0; }
    fft(re, im);
    for (i = 0; i <= FR / 2; i++) mag[i] = sqrt(re[i] * re[i] + im[i] * im[i]);
}

static void one(const char *t, size_t tl, const unsigned *u, int vel)
{
    static float ref[N]; static int32_t o[N]; static double a[N], r[N];
    po32_patch_params_t P, Q; uint8_t b[64]; size_t bl, rl; po32_synth_t sy; static drum_t v;
    uint16_t p[21]; int i, cnt = 0, k, frames = 0;
    double dev = 0, lev = 0, tail = 0, sr = 0, pk = 0;
    if (po32_patch_parse_mtdrum_text(t, tl, &P) != PO32_OK || po32_encode_patch(&P, b, sizeof b, &bl) != PO32_OK) { puts("ERR"); return; }
    po32_decode_patch(b, bl, &Q);
    memset(ref, 0, sizeof ref); po32_synth_init(&sy, 44100); po32_synth_render(&sy, &Q, vel, 3.0f, ref, N, &rl);
    for (i = 0; i < 21; i++) p[i] = (uint16_t)u[i];
    memset(&v, 0, sizeof v); drum_trig(&v, p, vel); memset(o, 0, sizeof o);
    while (cnt < N) { int n = 1024 < N - cnt ? 1024 : N - cnt, on = drum_render(&v, o + cnt, n); cnt += n; if (!on) break; }
    for (i = 0; i < N; i++) { a[i] = i < cnt ? o[i] / 1048576.0 : 0; r[i] = ref[i]; sr += r[i] * r[i]; if (fabs(r[i]) > pk) pk = fabs(r[i]); }
    for (i = cnt; i < N; i++) tail += r[i] * r[i];
    for (i = 0; i + FR <= N; i += 512) {
        static double ma[FR / 2 + 1], mb[FR / 2 + 1];
        double ea = 0, eb = 0, bmax = 0, s = 0, ba[64], bb[64]; int nb = 0, m = 0;
        for (k = 0; k < FR; k++) { ea += a[i + k] * a[i + k]; eb += r[i + k] * r[i + k]; }
        if (eb / FR < 1e-7) continue;
        spec(a + i, ma); spec(r + i, mb);
        for (k = 0; k + 8 < FR / 2 + 1; k += 8) { double sa = 0, sb = 0; int j; for (j = 0; j < 8; j++) { sa += ma[k + j]; sb += mb[k + j]; } ba[nb] = sa / 8; bb[nb] = sb / 8; if (bb[nb] > bmax) bmax = bb[nb]; nb++; }
        for (k = 0; k < nb; k++) if (bb[k] > bmax * 1e-3) { s += fabs(20 * log10((ba[k] + 1e-9) / (bb[k] + 1e-9))); m++; }
        if (!m) continue;
        dev += s / m; lev += 20 * log10((sqrt(ea / FR) + 1e-9) / (sqrt(eb / FR) + 1e-9)); frames++;
    }
    if (!frames) { puts("QUIET"); return; }
    printf("%.4f %.4f %.2f %.3f\n", dev / frames, lev / frames, 10 * log10((tail + 1e-20) / (sr + 1e-20)), pk);
}

int main(int argc, char **argv)
{
    size_t cap = 1 << 16, n = 0; char *buf = malloc(cap), line[8192]; unsigned u[21]; int have = 0, vel = argc > 1 ? atoi(argv[1]) : 127, rec = 0;
    while (fgets(line, sizeof line, stdin)) {
        if (!strncmp(line, "#U", 2)) { char *s = line + 2; int i; for (i = 0; i < 21; i++) u[i] = (unsigned)strtoul(s, &s, 10); have = 1; continue; }
        if (line[0] == '\f') { if (have) one(buf, n, u, (rec++ & 1) ? 100 : vel); else puts("ERR"); n = 0; have = 0; continue; }
        size_t l = strlen(line); if (n + l + 1 > cap) { cap *= 2; buf = realloc(buf, cap); } memcpy(buf + n, line, l); n += l;
    }
    return 0;
}
