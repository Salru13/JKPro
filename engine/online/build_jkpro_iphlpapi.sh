#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright 2026 XxXJoker13XxX and contributors
# JKPro i386 iphlpapi.dll: Wine 490f6d5 iphlpapi + iphlpapi_synth.inc (a synthetic IPv4
# adapter when the console has no Nsi device). Usage (WSL): bash build_jkpro_iphlpapi.sh OUT
#   control  the unpatched module from the same configure (its sha is recorded, not pinned:
#            the shipped 74aa98a0 is the upstream maintainer's build)
#   jkpro    the patched object relinked, debug stripped, paths6 clean, scanned
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
OUT=$(realpath -m "${1:?OUT}")
B="${JKFLOW_BUILD_ROOT:-$HOME/orbis-win-host-bootstrap}"
SRC=$B/native247-wine-source
EXP="${JKFLOW_EXPORT:-$here/../../../JkFlow-export}"
sdk=$B/orbis-win-9749fcd/build/ps4-wine-sdk
export PATH="$B/private-bin:$B/deps/rootfs/usr/bin:$PATH"
export BISON_PKGDATADIR="$B/deps/rootfs/usr/share/bison"
export OO_PS4_TOOLCHAIN="$B/orbis-win-9749fcd/sdk/orbis-sdk-v1/sdk"
[[ $(git -C "$SRC" rev-parse HEAD) == 490f6d5dcbb2a5047345b8af88d114bbcaad69a8 ]] || { echo 'wrong Wine base' >&2; exit 1; }
[[ ! -e $OUT ]] || { echo "exists: $OUT" >&2; exit 1; }
mkdir -p "$OUT/src"
T=dlls/iphlpapi/i386-windows/iphlpapi.dll
O=dlls/iphlpapi/i386-windows/iphlpapi_main.o
echo "== copy the configured native248 build tree (srcdir $SRC; the original is not touched)"
grep -q "^srcdir = $SRC\$" "$B/native248-build-wine/build/Makefile" || { echo 'unexpected srcdir' >&2; exit 1; }
cp -a "$B/native248-build-wine/build" "$OUT/build"
echo "== control build"
make -C "$OUT/build" -j"${JOBS:-6}" "$T" > "$OUT/make-control.log" 2>&1 || { tail -30 "$OUT/make-control.log"; exit 1; }
cp "$OUT/build/$T" "$OUT/control-iphlpapi.dll"
echo "== patched source"
python3 - "$SRC/dlls/iphlpapi/iphlpapi_main.c" "$OUT/src/iphlpapi_main.c" <<'PY'
import sys
s = open(sys.argv[1]).read()
def rep(a, b, n=1):
    global s
    assert s.count(a) == n, (a[:60], s.count(a))
    s = s.replace(a, b)
rep('WINE_DEFAULT_DEBUG_CHANNEL(iphlpapi);\n', 'WINE_DEFAULT_DEBUG_CHANNEL(iphlpapi);\n\n#include "iphlpapi_synth.inc"\n')
# GetAdaptersInfo: the first Nsi call
rep('''                                  NULL, 0, (void **)&if_stat, sizeof(*if_stat), &if_count, 0 );

    if (err) return err;
    for (i = 0; i < if_count; i++)
    {
        if (if_stat[i].type == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        if_num++;
    }
''', '''                                  NULL, 0, (void **)&if_stat, sizeof(*if_stat), &if_count, 0 );

    if (err) { DWORD r = pw_synth_adaptersinfo( info, size ); return r == ERROR_NOT_SUPPORTED ? err : r; }
    for (i = 0; i < if_count; i++)
    {
        if (if_stat[i].type == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        if_num++;
    }
''')
rep('''    if (err) return err;

    needed = FIELD_OFFSET( MIB_IPADDRTABLE, table[count] );
''', '''    if (err) { DWORD r = pw_synth_ipaddrtable( table, size ); return r == ERROR_NOT_SUPPORTED ? err : r; }
    if (pw_synth_no_ipv4( keys, count ))   /* the console's Nsi answers with no IPv4 address */
    {
        DWORD r = pw_synth_ipaddrtable( table, size );
        if (r != ERROR_NOT_SUPPORTED) { NsiFreeTable( keys, rw, NULL, NULL ); return r; }
    }

    needed = FIELD_OFFSET( MIB_IPADDRTABLE, table[count] );
''')
# GetAdaptersInfo: Nsi answers, but with no adapter
rep('''    if (!if_num)
    {
        err = ERROR_NO_DATA;
        goto err;
    }

    err = NsiAllocateAndGetTable( 1, &NPI_MS_IPV4_MODULEID, NSI_IP_UNICAST_TABLE,''', '''    if (!if_num)
    {
        DWORD r = pw_synth_adaptersinfo( info, size );
        err = r == ERROR_NOT_SUPPORTED ? ERROR_NO_DATA : r;
        goto err;
    }

    err = NsiAllocateAndGetTable( 1, &NPI_MS_IPV4_MODULEID, NSI_IP_UNICAST_TABLE,''')
# GetAdaptersAddresses: the PS4 NetCtl adapter also when Nsi answers with no interface
rep('''        return adapters_addresses_ps4( info, count );
    }
    if (err) return err;

    if (!*count)
    {
        err = ERROR_NO_DATA;
        goto err;
    }
''', '''        return adapters_addresses_ps4( info, count );
    }
    if (err) return err;

    if (!*count)
    {
        NsiFreeTable( luids, rw, dyn, stat );
        TRACE( "PS4 NSI has no interface, using NetCtl adapter config\\n" );
        return adapters_addresses_ps4( info, count );
    }
''')
# GetIfEntry: the synthetic interface
rep('''    err = ConvertInterfaceIndexToLuid( row->dwIndex, &luid );
    if (err) return err;

    err = NsiGetAllParameters( 1, &NPI_MS_NDIS_MODULEID, NSI_NDIS_IFINFO_TABLE,
                               &luid, sizeof(luid), &rw, sizeof(rw),
                               &dyn, sizeof(dyn), &stat, sizeof(stat) );
    if (!err) if_row_fill( row, &rw, &dyn, &stat );
    return err;''', '''    err = ConvertInterfaceIndexToLuid( row->dwIndex, &luid );
    if (err) return pw_synth_ifrow( row, err );

    err = NsiGetAllParameters( 1, &NPI_MS_NDIS_MODULEID, NSI_NDIS_IFINFO_TABLE,
                               &luid, sizeof(luid), &rw, sizeof(rw),
                               &dyn, sizeof(dyn), &stat, sizeof(stat) );
    if (!err) if_row_fill( row, &rw, &dyn, &stat );
    else err = pw_synth_ifrow( row, err );
    return err;''')
rep('''    if (err) return err;

    needed = FIELD_OFFSET( MIB_IPFORWARDTABLE, table[count] );
''', '''    if (err) { DWORD r = pw_synth_forwardtable( table, size ); return r == ERROR_NOT_SUPPORTED ? err : r; }
    if (!count)   /* the console's Nsi answers with no route */
    {
        DWORD r = pw_synth_forwardtable( table, size );
        if (r != ERROR_NOT_SUPPORTED) { NsiFreeTable( keys, rw, dyn, stat ); return r; }
    }

    needed = FIELD_OFFSET( MIB_IPFORWARDTABLE, table[count] );
''')
open(sys.argv[2], 'w').write(s)
PY
cp "$here/iphlpapi_synth.inc" "$OUT/src/"
diff -u --label a/dlls/iphlpapi/iphlpapi_main.c --label b/dlls/iphlpapi/iphlpapi_main.c \
    "$SRC/dlls/iphlpapi/iphlpapi_main.c" "$OUT/src/iphlpapi_main.c" > "$OUT/jkpro-iphlpapi.patch" || true
echo "== patched object + relink"
cmd=$(make --no-print-directory -C "$OUT/build" -n -W "$SRC/dlls/iphlpapi/iphlpapi_main.c" "$O" | tr '\n' ' ' | sed 's/\\ / /g')
[[ $cmd == *"-c -o $O $SRC/dlls/iphlpapi/iphlpapi_main.c"* ]] || { echo "unexpected compile command: $cmd" >&2; exit 1; }
cmd=${cmd/"$SRC/dlls/iphlpapi/iphlpapi_main.c"/"$OUT/src/iphlpapi_main.c -I$OUT/src -fdebug-prefix-map=$OUT/src=$SRC/dlls/iphlpapi"}
(cd "$OUT/build" && eval "$cmd")
touch "$OUT/build/$O"
make -C "$OUT/build" "$T" > "$OUT/make-jkpro.log" 2>&1 || { tail -30 "$OUT/make-jkpro.log"; exit 1; }
cp "$OUT/build/$T" "$OUT/iphlpapi.debug.dll"
i686-w64-mingw32-strip --strip-debug -o "$OUT/iphlpapi.strip.dll" "$OUT/iphlpapi.debug.dll"
# the build host's home becomes an equal-length neutral path (/build/jkflw/ for a 13-byte home, as the pinned builds)
src="$HOME/"; dst=/build/jkflw/; [ ${#dst} -eq ${#src} ] || dst="/build/$(printf '%*s' $((${#src} - 8)) '' | tr ' ' j)/"
printf '%s %s\n' "$src" "$dst" > "$OUT/pathmap"
POSTLINK_PATH_MAP="$OUT/pathmap" python3 "$EXP/wine/tools/paths_clean.py" paths6 pe "$OUT/iphlpapi.strip.dll" "$OUT/iphlpapi.dll"
if grep -a -c -E "$(id -un)|/home/|/mnt/" "$OUT/iphlpapi.dll"; then echo 'build-host path left' >&2; exit 1; fi
grep -a -c 'synthetic adapter' "$OUT/iphlpapi.dll" >/dev/null || { echo 'patch missing' >&2; exit 1; }
sha256sum "$OUT/control-iphlpapi.dll" "$OUT/iphlpapi.dll" | tee "$OUT/SHA256SUMS"
echo "shipped: $(sha256sum "$(readlink -f "$B/blur-launcher-r4/package-view-292/win/wine/lib/wine/i386-windows/iphlpapi.dll")" | cut -c1-64)"
