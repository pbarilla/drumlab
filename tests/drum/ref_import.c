/* SPDX-License-Identifier: GPL-3.0-only */
/* libpo's .mtdrum text importer as a filter, for tests/test_mtimport.mjs: reads drum texts on stdin (records end with a line holding one form feed),
 * writes one line per record: the 21 u16 as the PO-32 would carry them, or "ERR". libpo is MIT (github.com/ericlewis/libpo), not bundled:
 *   L=<libpo>/src/core; cc -O2 -I$L/include -I$L/src -o ref_import tests/drum/ref_import.c $L/src/po32.c $L/src/po32_patch_import.c */
#include "po32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void flush(const char *t, size_t n)
{
    po32_patch_params_t P;
    uint8_t b[64];
    size_t bl = 0, i;
    if (po32_patch_parse_mtdrum_text(t, n, &P) != PO32_OK || po32_encode_patch(&P, b, sizeof b, &bl) != PO32_OK || bl < 42) { puts("ERR"); return; }
    for (i = 0; i < 21; i++) printf("%u%c", (unsigned)(b[2 * i] | b[2 * i + 1] << 8), i == 20 ? '\n' : ' ');
}

int main(void)
{
    size_t cap = 1 << 16, n = 0;
    char *buf = malloc(cap), line[8192];
    while (fgets(line, sizeof line, stdin)) {
        size_t l = strlen(line);
        if (line[0] == '\f') { flush(buf, n); n = 0; continue; }
        if (n + l + 1 > cap) { cap *= 2; buf = realloc(buf, cap); }
        memcpy(buf + n, line, l); n += l;
    }
    if (n) flush(buf, n);
    free(buf);
    return 0;
}
