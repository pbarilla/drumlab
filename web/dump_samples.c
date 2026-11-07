/* SPDX-License-Identifier: GPL-3.0-only */
/* Writes project files with the firmware's own lab_pack (so the bytes are exactly what the device keeps): web/make_samples.py feeds it a spec.
 *   cc -O2 -DCHIPBOX_HOST -Dmain=sim_main -Ifirmware/src -Ifirmware/gen -o dump web/dump_samples.c
 * Spec, one word per line group: "kit N" (-1 LAB-1, else the preset kit N), "bpm N", "swing N", "chain n c0 c1 ..", "pat I ACC t0 .. t63",
 * "out PATH" (writes the project and starts the next). */
#include "../firmware/src/lab.c"
#undef main
int main(int argc, char **argv)
{
    static uint8_t buf[PROJ_BANK];
    FILE *f;
    char w[64];
    int kit = -1;
    unsigned v, i, j, n, a;
    if (argc < 2 || !(f = fopen(argv[1], "r"))) return 1;
    core_init();
    while (fscanf(f, "%63s", w) == 1) {
        if (!strcmp(w, "kit")) { fscanf(f, "%d", &kit); core_init(); if (kit < 0) kit_lab1(); else kit_preset((uint32_t)kit); }
        else if (!strcmp(w, "bpm")) { fscanf(f, "%u", &v); tempo_set(v); }
        else if (!strcmp(w, "swing")) { fscanf(f, "%u", &v); swing = (uint8_t)v; }
        else if (!strcmp(w, "chain")) { fscanf(f, "%u", &n); chain_n = (uint8_t)n; for (i = 0; i < n; i++) { fscanf(f, "%u", &v); chain[i] = (uint8_t)v; } }
        else if (!strcmp(w, "pat")) {
            fscanf(f, "%u %u", &i, &a);
            memset(&pat[i], 0, sizeof pat[i]);
            for (j = 0; j < 64; j++) { fscanf(f, "%u", &v); pat[i].trig[j / 16][j % 16] = (uint8_t)v; pat[i].lock[j / 16][j % 16][0] = PITCH_MID; }
            pat[i].acc = (uint16_t)a;
        }
        else if (!strcmp(w, "out")) {
            char p[512]; FILE *o;
            fscanf(f, "%511s", p);
            n = lab_pack(buf);
            if (!(o = fopen(p, "wb"))) return 2;
            fwrite(buf, 1, n, o); fclose(o);
            printf("%s %u bytes\n", p, n);
            core_init();
        }
    }
    return 0;
}
