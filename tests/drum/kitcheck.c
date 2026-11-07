/* kitcheck: renders every sound of N rolled kits (and the preset kits) through drum.c and measures them: peak, length, loudness, DC, brightness.
 *   cc -O2 -DCHIPBOX_HOST -Ifirmware/src -Ifirmware/gen -o kitcheck tests/drum/kitcheck.c -lm ; ./kitcheck 2000 */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#define i32 int32_t
#define u32 uint32_t
#define i64 int64_t
#define u64 uint64_t
#include "drum.c"
#define NS 16
#define PITCH_MID 0x80u
static uint16_t kp[NS][2][DR_PARAMS];
static uint8_t knob[NS][2], snm[NS];
#include "kit_lab1.h"
#include "labgen.c"
typedef struct { double peak, rms, len, dc, bright; } st_t;
static st_t measure(const uint16_t *p, int vel)
{
    static drum_t v;
    static int32_t o[44100 * 4];
    int n = 0, k, on = 1;
    double pk = 0, e = 0, dc = 0, hf = 0, prev = 0;
    st_t r;
    drum_trig(&v, p, vel);
    while (on && n < 44100 * 4 - 1024) { on = drum_render(&v, o + n, 1024); n += 1024; }
    for (k = 0; k < n; k++) { double a = o[k] / 1048576.0; if (fabs(a) > pk) pk = fabs(a); e += a * a; dc += a; hf += (a - prev) * (a - prev); prev = a; }
    for (k = n - 1; k > 0 && fabs(o[k] / 1048576.0) < 0.003; k--) {}
    r.peak = pk; r.rms = sqrt(e / n); r.len = k / 44100.0; r.dc = fabs(dc / n); r.bright = e > 0 ? sqrt(hf / e) : 0;
    return r;
}
static int bad(const st_t *s, uint32_t col, uint32_t row, char *why)
{
    if (s->peak > 1.25) { strcpy(why, "loud"); return 1; }
    if (s->peak < 0.06) { strcpy(why, "quiet"); return 1; }
    if (s->len > 3.9)   { strcpy(why, "long"); return 1; }
    if (s->len < 0.010) { strcpy(why, "short"); return 1; }
    if (s->dc > 0.04)   { strcpy(why, "dc"); return 1; }
    (void)col; (void)row;
    return 0;
}
int main(int argc, char **argv)
{
    int N = argc > 1 ? atoi(argv[1]) : 500, i, s, q, fails = 0, total = 0, k;
    double pmin = 9, pmax = 0, lmax = 0;
    char why[16];
    int why_n[8] = {0};
    for (k = 0; k < NKITDEF + 1 + N; k++) {
        if (k == 0) kit_lab1(); else if (k <= NKITDEF) kit_preset((uint32_t)(k - 1)); else kit_roll((uint32_t)(k * 7919 + 13));
        for (s = 0; s < NS; s++)
            for (q = 0; q < 2; q++) {
                st_t m = measure(kp[s][q], 127);
                total++;
                if (m.peak < pmin) pmin = m.peak;
                if (m.peak > pmax) pmax = m.peak;
                if (m.len > lmax) lmax = m.len;
                if (bad(&m, (uint32_t)s & 3u, (uint32_t)s >> 2, why)) {
                    fails++;
                    why_n[!strcmp(why, "loud") ? 0 : !strcmp(why, "quiet") ? 1 : !strcmp(why, "long") ? 2 : !strcmp(why, "short") ? 3 : 4]++;
                    if (k <= NKITDEF || fails < 12) printf("kit %d sound %d %s %c: %s (peak %.2f rms %.3f len %.2f dc %.3f)\n", k, s, snd_name((uint32_t)s), q ? 'B' : 'A', why, m.peak, m.rms, m.len, m.dc);
                }
            }
    }
    (void)i; (void)why_n;
    printf("outside the limits: loud %d, quiet %d, long %d, short %d, dc %d\n", why_n[0], why_n[1], why_n[2], why_n[3], why_n[4]);
    printf("%d kits, %d patches, %d outside the limits; peak %.2f .. %.2f, longest %.2f s\n", NKITDEF + 1 + N, total, fails, pmin, pmax, lmax);
    return 0;
}
