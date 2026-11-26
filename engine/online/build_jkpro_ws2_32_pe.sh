#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright 2026 XxXJoker13XxX and contributors
# JKPro i386 ws2_32.dll: Wine 490f6d5 ws2_32 where IPv4 multicast options the console refuses
# (IP_ADD_MEMBERSHIP, IP_MULTICAST_TTL, ...: STATUS_INVALID_PARAMETER on the PS4) report success.
# PES 2013's UPnP router discovery (SSDP, 239.255.255.250:1900) sets them and treats a failure as
# "check your network settings"; the router answers to the console's unicast address, so the
# membership is not needed to receive it. Every ignored option is logged (warn+winsock).
# Usage (WSL): bash build_jkpro_ws2_32_pe.sh OUT
set -euo pipefail
OUT=$(realpath -m "${1:?OUT}")
here="$(cd "$(dirname "$0")" && pwd)"
B="${JKFLOW_BUILD_ROOT:-$HOME/orbis-win-host-bootstrap}"
SRC=$B/native247-wine-source
EXP="${JKFLOW_EXPORT:-$here/../../../JkFlow-export}"
export PATH="$B/private-bin:$B/deps/rootfs/usr/bin:$PATH"
[[ $(git -C "$SRC" rev-parse HEAD) == 490f6d5dcbb2a5047345b8af88d114bbcaad69a8 ]] || { echo 'wrong Wine base' >&2; exit 1; }
[[ ! -e $OUT ]] || { echo "exists: $OUT" >&2; exit 1; }
mkdir -p "$OUT/src"
T=dlls/ws2_32/i386-windows/ws2_32.dll
O=dlls/ws2_32/i386-windows/socket.o
grep -q "^srcdir = $SRC\$" "$B/native248-build-wine/build/Makefile" || { echo 'unexpected srcdir' >&2; exit 1; }
cp -a "$B/native248-build-wine/build" "$OUT/build"
echo "== control build"
make -C "$OUT/build" -j"${JOBS:-6}" "$T" > "$OUT/make-control.log" 2>&1 || { tail -30 "$OUT/make-control.log"; exit 1; }
cp "$OUT/build/$T" "$OUT/control-ws2_32.dll"
echo "== patched source"
python3 - "$SRC/dlls/ws2_32/socket.c" "$OUT/src/socket.c" <<'PY'
import sys
s = open(sys.argv[1]).read()
a = '''    status = NtDeviceIoControlFile( (HANDLE)s, NULL, NULL, NULL, &io, code, (void *)optval, optlen, NULL, 0 );
    SetLastError( NtStatusToWSAError( status ) );
    TRACE( "status %#lx.\\n", status );
    return status ? -1 : 0;
}
'''
b = '''    status = NtDeviceIoControlFile( (HANDLE)s, NULL, NULL, NULL, &io, code, (void *)optval, optlen, NULL, 0 );
    if (status && (code == IOCTL_AFD_WINE_SET_IP_ADD_MEMBERSHIP || code == IOCTL_AFD_WINE_SET_IP_DROP_MEMBERSHIP ||
                   code == IOCTL_AFD_WINE_SET_IP_ADD_SOURCE_MEMBERSHIP || code == IOCTL_AFD_WINE_SET_IP_DROP_SOURCE_MEMBERSHIP ||
                   code == IOCTL_AFD_WINE_SET_IP_BLOCK_SOURCE || code == IOCTL_AFD_WINE_SET_IP_UNBLOCK_SOURCE ||
                   code == IOCTL_AFD_WINE_SET_IP_MULTICAST_IF || code == IOCTL_AFD_WINE_SET_IP_MULTICAST_LOOP ||
                   code == IOCTL_AFD_WINE_SET_IP_MULTICAST_TTL))
    {
        /* JKPro (PS4): the console refuses IPv4 multicast options for applications; report success
         * so UPnP/SSDP discovery goes on (replies come to the unicast address). */
        WARN( "PS4: multicast option %#lx refused with %#lx, reported as success\\n", code, status );
        status = STATUS_SUCCESS;
    }
    SetLastError( NtStatusToWSAError( status ) );
    TRACE( "status %#lx.\\n", status );
    return status ? -1 : 0;
}
'''
assert s.count(a) == 1
open(sys.argv[2], 'w').write(s.replace(a, b))
PY
diff -u --label a/dlls/ws2_32/socket.c --label b/dlls/ws2_32/socket.c "$SRC/dlls/ws2_32/socket.c" "$OUT/src/socket.c" > "$OUT/jkpro-ws2_32-pe.patch" || true
echo "== patched object + relink"
cmd=$(make --no-print-directory -C "$OUT/build" -n -W "$SRC/dlls/ws2_32/socket.c" "$O" | tr '\n' ' ' | sed 's/\\ / /g')
[[ $cmd == *"-c -o $O $SRC/dlls/ws2_32/socket.c"* ]] || { echo "unexpected compile command: $cmd" >&2; exit 1; }
cmd=${cmd/"$SRC/dlls/ws2_32/socket.c"/"$OUT/src/socket.c -I$SRC/dlls/ws2_32 -fdebug-prefix-map=$OUT/src=$SRC/dlls/ws2_32"}
(cd "$OUT/build" && eval "$cmd")
touch "$OUT/build/$O"
make -C "$OUT/build" "$T" > "$OUT/make-jkpro.log" 2>&1 || { tail -30 "$OUT/make-jkpro.log"; exit 1; }
cp "$OUT/build/$T" "$OUT/ws2_32.debug.dll"
i686-w64-mingw32-strip --strip-debug -o "$OUT/ws2_32.strip.dll" "$OUT/ws2_32.debug.dll"
# the build host's home becomes an equal-length neutral path (/build/jkflw/ for a 13-byte home, as the pinned builds)
src="$HOME/"; dst=/build/jkflw/; [ ${#dst} -eq ${#src} ] || dst="/build/$(printf '%*s' $((${#src} - 8)) '' | tr ' ' j)/"
printf '%s %s\n' "$src" "$dst" > "$OUT/pathmap"
POSTLINK_PATH_MAP="$OUT/pathmap" python3 "$EXP/wine/tools/paths_clean.py" paths6 pe "$OUT/ws2_32.strip.dll" "$OUT/ws2_32.dll"
if grep -a -c -E "$(id -un)|/home/|/mnt/" "$OUT/ws2_32.dll"; then echo 'build-host path left' >&2; exit 1; fi
grep -a -c 'multicast option' "$OUT/ws2_32.dll" >/dev/null || { echo 'patch missing' >&2; exit 1; }
sha256sum "$OUT/control-ws2_32.dll" "$OUT/ws2_32.dll" | tee "$OUT/SHA256SUMS"
