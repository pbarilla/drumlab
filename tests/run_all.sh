#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Everything that runs on a computer, under the address and undefined-behaviour sanitizers: every lab_*.txt host script (the monkeys too),
# random input (tests/fuzz.py, FUZZ seeds of FUZZ_SECS seconds), the kit generator check, the site's installer and projects tests, the Microtonic import tests (LIBPO adds the libpo comparisons).
# Prints FAIL lines; exit 1 if any.   FUZZ=12 FUZZ_SECS=900 sh tests/run_all.sh
cd "$(dirname "$0")/.." || exit 1
mkdir -p build/host/asan
fail=0
CF="-g -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined"
cc $CF -DCHIPBOX_HOST -DAPP_VER='"dev"' -Ifirmware/src -Ifirmware/gen -o build/host/lab_san firmware/src/lab.c -lm 2>/dev/null || { echo "FAIL build lab_san"; exit 1; }
run() {                                         # run NAME SCRIPT
    rm -rf build/host/asan/$1 build/host/asan/$1.bin; mkdir -p build/host/asan/$1
    HOST_FLASH=build/host/asan/$1.bin timeout 1800 build/host/lab_san build/host/asan/$1/f $2 > build/host/asan/$1.log 2>&1 || { echo "FAIL $1 (exit $?)"; tail -3 build/host/asan/$1.log; fail=1; return; }
    grep -q "runtime error\|AddressSanitizer" build/host/asan/$1.log && { echo "FAIL $1 (sanitizer)"; fail=1; } || echo "ok   $1"
    rm -rf build/host/asan/$1
}
for f in tests/lab_*.txt; do run "$(basename $f .txt)" "$f"; done
n=0
while [ $n -lt ${FUZZ:-3} ]; do
    n=$((n + 1)); python3 tests/fuzz.py $n ${FUZZ_SECS:-300} > build/host/asan/fuzz$n.txt; run fuzz$n build/host/asan/fuzz$n.txt
done
cc $CF -DCHIPBOX_HOST -Ifirmware/src -Ifirmware/gen -o build/host/kitcheck tests/drum/kitcheck.c -lm 2>/dev/null && build/host/kitcheck ${KITS:-600} > build/host/asan/kitcheck.log 2>&1 && ! grep -q "runtime error\|AddressSanitizer" build/host/asan/kitcheck.log && echo "ok   kitcheck" || { echo "FAIL kitcheck"; tail -3 build/host/asan/kitcheck.log; fail=1; }
if [ -n "$LIBPO" ]; then                          # libpo (MIT, not bundled): LIBPO=.../libpo/core. The transfer must match its bytes.
    cc $CF -DCHIPBOX_HOST -I$LIBPO/include -I$LIBPO/src -Ifirmware/src -Ifirmware/gen $LIBPO/src/po32.c $LIBPO/src/po32_synth.c $LIBPO/src/po32_patch_import.c tests/drum/pack.c -lm -o build/host/pack 2>/dev/null \
        && build/host/pack > build/host/asan/pack.log 2>&1 && ! grep -q "runtime error\|AddressSanitizer\|MISMATCH\|differ" build/host/asan/pack.log && echo "ok   pack (bytes equal libpo's)" || { echo "FAIL pack"; tail -3 build/host/asan/pack.log; fail=1; }
fi
node tests/test_mtimport.mjs > build/host/asan/test_mtimport.log 2>&1 && echo "ok   test_mtimport (Microtonic import / export, share files, firmware parser)" || { echo "FAIL test_mtimport"; tail -3 build/host/asan/test_mtimport.log; fail=1; }
if [ -n "$LIBPO" ]; then                          # the importer against libpo's C, and the imported sounds through both engines (needs the local Microtonic sample folders, see tests/mtlib.mjs)
    node tests/cmp_import.mjs > build/host/asan/cmp_import.log 2>&1 && echo "ok   cmp_import ($(grep -o 'of [0-9]* compared, [0-9]* identical' build/host/asan/cmp_import.log | head -1))" || { echo "FAIL cmp_import"; tail -3 build/host/asan/cmp_import.log; fail=1; }
    node tests/test_mtaccuracy.mjs > build/host/asan/test_mtaccuracy.log 2>&1 && echo "ok   test_mtaccuracy" || { echo "FAIL test_mtaccuracy"; tail -3 build/host/asan/test_mtaccuracy.log; fail=1; }
fi
for t in test_installer test_projects; do node web/$t.mjs > build/host/asan/$t.log 2>&1 && echo "ok   $t" || { echo "FAIL $t"; tail -3 build/host/asan/$t.log; fail=1; }; done
exit $fail
