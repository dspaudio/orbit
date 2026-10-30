#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The editor protocol / external input fuzz (tests/editor_fuzz.c), its reproducers (tests/editor_repro.c) and the
# power-on sound test (tests/boot_sound_test.c) under AddressSanitizer + UBSan, on the real editor, storage and
# project code (tests/editor_host.c). Ported from SLOOP 2.8.4, isod89/sloop-fm1 91f3018 (tests/editor_fuzz.sh).
#   sh tests/editor_fuzz.sh [N] [SEED]     (from anywhere, after build/gen exists: sh build.sh or tools/build.py)
# N (default 2000) scales every mode as upstream. The sanitizer compiler is CC_UB (default cc); on macOS use
# Homebrew LLVM with the SDK (docs/VALIDATION.md):
#   CC_UB="/opt/homebrew/opt/llvm/bin/clang -isysroot $(xcrun --show-sdk-path)" sh tests/editor_fuzz.sh
# Any failure, including a sanitizer that cannot start, exits non-zero (set -e).
set -e
cd "$(dirname "$0")/.."
OUT="${EDFZ_OUT:-build/host/edfz}"
mkdir -p "$OUT/gen"
# SMP_DATA read through a pointer: the user sample slots are addressed as offsets from it (one flash), which
# -fsanitize=bounds would take for an overflow of the array; AddressSanitizer still checks every byte
{ echo '#define SMP_DATA (smp_data_v)'; sed 's/^static const uint8_t SMP_DATA\[/static const uint8_t SMP_DATA_ARR[/' build/gen/felucca_samples.h;
  echo 'static const uint8_t *volatile smp_data_v = SMP_DATA_ARR;'; } > "$OUT/gen/felucca_samples.h"
ASAN="${CC_UB:-cc} -O1 -g -w -fsanitize=address,undefined -fno-sanitize=shift-base,signed-integer-overflow -fno-sanitize-recover=all"
INC="-I$OUT/gen -Ibuild/gen -Ifirmware/src -Ifirmware/hal"
$ASAN $INC -o "$OUT/editor_fuzz" tests/editor_fuzz.c -lm
$ASAN $INC -o "$OUT/editor_repro" tests/editor_repro.c -lm
$ASAN $INC -o "$OUT/boot_sound_test" tests/boot_sound_test.c -lm
N=${1:-2000}
S=${2:-1}
"$OUT/editor_fuzz" 0 $((N * 10)) "$S"        # single frames (legacy and ED_WIDE), every command, lengths at the edges
"$OUT/editor_fuzz" 1 "$N" "$S"               # sequences: uploads, restores, presets, macros, FM6, SYN kits, UI, audio
"$OUT/editor_fuzz" 2 "$N" "$S"               # USB-MIDI and TRS bytes in, with the rest
"$OUT/editor_fuzz" 3 $((N / 20)) "$S"        # boots from a damaged / foreign flash (a child each)
"$OUT/editor_fuzz" 4 1 "$S"                  # every command over every engine / track / id / step
"$OUT/editor_fuzz" 5 "$N" "$S"               # MIDI in only, then no note may be left held
"$OUT/editor_fuzz" 6 $((N / 100)) "$S"       # backup / section / autosave round trips give the project back
# the reproducers of what it found: each must run clean (1: user kit root, 2: program change, 3: step velocity)
for r in 1 2 3; do "$OUT/editor_repro" $r; done
# power-on sounds: first install, after an update, over a FUN6 or invalid-engine ORP8 working project
"$OUT/boot_sound_test" > "$OUT/boot_sound.log" || { cat "$OUT/boot_sound.log"; exit 1; }
tail -n 1 "$OUT/boot_sound.log"
