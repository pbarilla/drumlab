#!/bin/sh
# kits/arch.py + kits/mkarch.c -> firmware/gen/arch.h (the 32 archetypes and the kit recipes). Needs libpo (MIT): LIBPO=/path/to/libpo-main/core
set -e
cd "$(dirname "$0")/.."
: "${LIBPO:?set LIBPO to the libpo core folder}"
T=$(mktemp -d)
python3 kits/arch.py "$T/mt" >/dev/null
cc -O2 -I"$LIBPO/include" -I"$LIBPO/src" -o "$T/mkarch" kits/mkarch.c "$LIBPO/src/po32.c" "$LIBPO/src/po32_patch_import.c" -lm
python3 kits/mkarch_gen.py "$T"
rm -rf "$T"
