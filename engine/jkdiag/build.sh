#!/bin/bash
# Build jkdiag.dll (+ test binaries) with the i686 mingw of the Wine bootstrap.
# Usage: build.sh [OUTDIR]   (default: ./build next to this script)
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=${1:-$HERE/build}
B=${ORBIS_BOOTSTRAP:-$HOME/orbis-win-host-bootstrap}
export PATH=$B/private-bin:$B/deps/rootfs/usr/bin:$PATH
CC=i686-w64-mingw32-gcc
mkdir -p "$OUT"
DLLFLAGS="-O2 -Wall -Wextra -shared -nostdlib -ffreestanding -fno-builtin -fno-stack-protector \
  -fno-asynchronous-unwind-tables -Wl,--entry,_DllMain@12 -Wl,--enable-stdcall-fixup -s"
$CC $DLLFLAGS -o "$OUT/jkdiag.dll" "$HERE/src/jkdiag.c" -lkernel32 -lgcc
$CC $DLLFLAGS -o "$OUT/stub-jkdiag.dll" "$HERE/test/stub.c" -lkernel32
$CC -O2 -shared -o "$OUT/kserv.dll" "$HERE/test/fakekserv.c"
$CC -O1 -fno-omit-frame-pointer -masm=att -o "$OUT/crashtest.exe" "$HERE/test/crashtest.c"
i686-w64-mingw32-objdump -p "$OUT/jkdiag.dll" 2>/dev/null | grep -E "DLL Name" || objdump -p "$OUT/jkdiag.dll" | grep -E "DLL Name"
sha256sum "$OUT/jkdiag.dll"
