#!/bin/bash
# Build (if stale) and run 5 Card Stud under the FujiNet-patched o2em against a
# live fujinet-pc-rs232.
#
#   ./run.sh                                       windowed, interactive
#   ./run.sh -frames=600 -dumpscr=/tmp/s.ppm -dumptxt=1
#   ./run.sh -input=90:d,150:f -frames=400         replay stick input
#   ./run.sh -resetat=300 -frames=600              prove reset survival
#
# fujinet-pc-rs232 must be running with [BOIP] enabled (port 9995 by default).
# The console BIOS is copyrighted and is not in this repo: BIOSDIR defaults to
# the firmware tree's gitignored bios/, which is where the bring-up keeps it.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
O2EM="${O2EM:-$HOME/Workspace/o2em/o2em}"
BIOSDIR="${BIOSDIR:-$HOME/Workspace/fujinet-firmware/pico/o2/bios}"
FUJI="${FUJI:-127.0.0.1:9995}"
CART="${CART:-5card}"

if [ ! -x "$O2EM" ]; then
    echo "o2em not found or not executable at: $O2EM" >&2
    echo "Build the FujiNet-patched o2em first:" >&2
    echo "  ~/Workspace/fujinet-firmware/pico/o2/emu/apply.sh ~/Workspace/o2em" >&2
    exit 1
fi
if [ ! -f "$BIOSDIR/o2rom.bin" ]; then
    echo "No console BIOS at $BIOSDIR/o2rom.bin -- supply your own (it is" >&2
    echo "copyrighted and not distributed here), or set BIOSDIR." >&2
    exit 1
fi

# Rebuild only when a source is newer than the image, same as intv/run.sh.
if [ ! -f "$HERE/build/$CART.bin" ] || \
   [ -n "$(find "$HERE" -maxdepth 1 \( -name '*.a48' -o -name '*.inc' \) -newer "$HERE/build/$CART.bin")" ]; then
    echo "Building $CART.bin..."
    "$HERE/build.sh" "$CART"
fi

exec "$O2EM" -romdir="$HERE/build/" -biosdir="$BIOSDIR/" -nosound \
     -fujinet="$FUJI" -fujinet-debug=1 "$@" "$CART.bin"
