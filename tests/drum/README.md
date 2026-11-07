Host tests for `drum.c` (the drum voice) and `po32tx.c` (the PO-32 transfer), against libpo (MIT, github.com/ericlewis/libpo).

- Get libpo: `curl -L https://github.com/ericlewis/libpo/archive/refs/heads/main.tar.gz | tar xz`; `L=libpo-main/core`.
- `cmp.c`: one `.mtdrum` text through libpo's float synth and `drum.c`: `cc -O2 -I$L/include -I$L/src -Ifirmware/src -Ifirmware/gen $L/src/po32.c $L/src/po32_synth.c $L/src/po32_patch_import.c firmware/src/drum.c tests/drum/cmp.c -lm`, run `./a.out d.txt out.raw [vel]`.
- `mt2txt.py preset.mtpreset outdir`: a Microtonic preset to eight `.mtdrum` texts. `sweep.py SEED`: 50 random presets, spectral and level deviation (needs numpy; run it where `cmp` is built).
- `txt.c`: builds a transfer with libpo and with `po32tx.c`, checks the bytes match and that libpo decodes our audio.
- `eqt.c`: the EQ biquad against double precision.
