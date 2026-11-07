/* kits/mkarch.c: .mtdrum texts (argv) -> rows of 21 wire parameters, with libpo's importer.
 *   cc -O2 -I$L/include -I$L/src -o mkarch kits/mkarch.c $L/src/po32.c $L/src/po32_patch_import.c -lm      (L = libpo's core folder) */
#include "po32.h"
#include <stdio.h>
#include <stdint.h>
int main(int argc, char **argv)
{
    int i, k;
    for (i = 1; i < argc; i++) {
        FILE *f = fopen(argv[i], "rb");
        char t[4096];
        size_t l;
        po32_patch_params_t P;
        uint8_t b[64];
        size_t bl;
        int off;
        if (!f) return 1;
        l = fread(t, 1, sizeof t, f); fclose(f);
        if (po32_patch_parse_mtdrum_text(t, l, &P) != PO32_OK) { fprintf(stderr, "parse fail %s\n", argv[i]); return 1; }
        po32_encode_patch(&P, b, sizeof b, &bl);
        off = bl == 43 ? 1 : 0;
        for (k = 0; k < 21; k++) printf("%u%s", (unsigned)(b[off + 2 * k] | b[off + 2 * k + 1] << 8), k < 20 ? "," : "\n");
    }
    return 0;
}
