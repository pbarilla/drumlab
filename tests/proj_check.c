/* SPDX-License-Identifier: GPL-3.0-only */
/* The firmware's own project code on files the website made: for each file given (a raw project, as the Projects page sends it over SysEx)
 *   1. labio.c's px_ok (copied out of the source at build time: tests/test_mtimport.mjs does the sed) must accept it,
 *   2. lab_unpack must load it, lab_pack must give the same bytes back (version 3: nothing was normalised or lost),
 *   3. lab_store + lab_load through the host flash must give them again.
 * Prints "ok name" or "FAIL name why". Build: see tests/test_mtimport.mjs. */
#define main host_main_unused
#include "../firmware/src/lab.c"
#undef main
#include "px_ok.inc"
#include <stdio.h>

int main(int argc, char **argv)
{
    static uint8_t in[PROJ_BANK], out[PROJ_BANK];
    int i, bad = 0;
    for (i = 1; i < argc; i++) {
        FILE *f = fopen(argv[i], "rb");
        size_t n;
        uint32_t m;
        const char *why = 0;
        if (!f) { printf("FAIL %s cannot open\n", argv[i]); bad = 1; continue; }
        n = fread(in, 1, sizeof in, f); fclose(f);
        core_init();
        if (px_ok(in, (uint32_t)n)) why = "px_ok refuses it";
        else if (lab_unpack(in, (uint32_t)n)) why = "lab_unpack refuses it";
        else {
            m = lab_pack(out);
            if (in[0] == 3 && (m != n || memcmp(in, out, n))) why = "pack(unpack(x)) differs from x";
            else if (lab_store(5, out, m)) why = "lab_store failed";
            else { core_init(); if (lab_load(5)) why = "lab_load failed"; else { uint8_t again[PROJ_BANK]; if (lab_pack(again) != m || memcmp(again, out, m)) why = "stored and loaded project differs"; } }
        }
        if (why) { printf("FAIL %s %s\n", argv[i], why); bad = 1; } else printf("ok %s\n", argv[i]);
    }
    return bad;
}
