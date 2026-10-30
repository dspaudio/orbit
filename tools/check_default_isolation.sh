# SPDX-License-Identifier: GPL-3.0-only
# Build two private snapshots, never the shared checkout's intermediates.
set -eu
: "${TMPDIR:?Export a private TMPDIR first}"
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
WORK=$(mktemp -d "$TMPDIR/default-isolation.XXXXXX")
PYTHON=${PYTHON:-python3}
export JIELI_TOOLCHAIN=${JIELI_TOOLCHAIN:-"$HOME/.jieli/toolchain"}
export AC79_SDK=${AC79_SDK:-"$HOME/fw-AC79_AIoT_SDK"}
unset ORBIT_BLE BLE_MEASURE
export BOARD=fm1 MODEL=pro
printf 'isolation: snapshot %s\n' "$WORK"
tar -C "$ROOT" --exclude='./.git' --exclude='./build' --exclude='./.claude/worktrees' \
    --exclude='./.omo' -cf "$WORK/source.tar" .
mkdir "$WORK/as-is" "$WORK/no-ble"
tar -C "$WORK/as-is" -xf "$WORK/source.tar"
tar -C "$WORK/no-ble" -xf "$WORK/source.tar"
# Deletion is restricted to the newly created copy, never ROOT.
rm -rf "$WORK/no-ble/firmware/src/ble"
rm "$WORK/no-ble/firmware/hal/fm1_ble_hal.h" \
   "$WORK/no-ble/firmware/app_ble.ld" "$WORK/no-ble/tools/ble_libs.py"
for tree in as-is no-ble; do
    printf 'isolation: build default %s\n' "$tree"
    (
        cd "$WORK/$tree"
        "$PYTHON" -c 'import sys; sys.path.insert(0, "tools"); import build; build.generate(); build.build_app()'
    )
done
cmp "$WORK/as-is/build/felucca.bin" "$WORK/no-ble/build/felucca.bin"
printf 'PASS: default felucca.bin byte-identical with all BLE-only files deleted\n'
printf 'isolation: evidence retained at %s\n' "$WORK"
