#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""JKPro's first-start seed (the Wine prefix), from JkFlow 1.00's seed245.

  python3 -I jkpro_seed.py <package-view> <out-dir> <replaced-rel>...   -> prints the new END hash

JkFlow's seed.idx copies some prefix files from the package's own Wine files ("F size sha rel src").
JKPro 1.01 replaced some of those package files (online: i386 ws2_32.dll, iphlpapi.dll), so a fresh
install found a source that is not the pinned one and stopped with E110 (an update from 1.00 kept the
prefix's copies and never read them). Here every entry whose source is a replaced file gets the
original bytes as its own object (src "-"), so the prefix is exactly 1.00's, and the index gets a new
END hash, which build-launcher.sh compiles in (N245_SEED_END).
"""
import hashlib, os, shutil, sys

SLUG = 'blur-native248'
# package files make_pkg_jkpro.sh replaces (it checks every seed source against the package it builds)
REPLACED = {
    'sce_module/dxvknative.prx', 'win/wine/lib/wine/x86_64-unix/dxvknative.prx',
    'sce_module/ws2_32.prx', 'win/wine/lib/wine/x86_64-unix/ws2_32.prx',
    'win/wine/lib/wine/i386-windows/iphlpapi.dll', 'win/wine/lib/wine/i386-windows/ws2_32.dll',
}


def sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()


def main():
    view, out, replaced = sys.argv[1], sys.argv[2], set(sys.argv[3:]) or REPLACED
    src_dir = os.path.join(view, 'seed245', SLUG)
    text = open(os.path.join(src_dir, 'seed.idx'), 'rb').read().decode('utf-8')
    assert text.endswith('\n')
    lines = text.split('\n')[:-1]
    assert lines[-1].startswith('END ') and len(lines[-1]) == 68, 'no END line'
    body = '\n'.join(lines[:-1]) + '\n'
    assert hashlib.sha256(body.encode()).hexdigest() == lines[-1][4:], 'seed.idx END does not match'
    dst = os.path.join(out, 'seed245', SLUG)
    if os.path.exists(os.path.join(out, 'seed245')):
        shutil.rmtree(os.path.join(out, 'seed245'))
    os.makedirs(os.path.join(dst, 'objects'))
    for name in os.listdir(os.path.join(src_dir, 'objects')):
        shutil.copyfile(os.path.join(src_dir, 'objects', name), os.path.join(dst, 'objects', name))
    new, moved = [], []
    for line in lines[:-1]:
        f = line.split('\t')
        if f[0] == 'F' and len(f) == 5 and f[4] != '-' and f[4] in replaced:
            orig = os.path.join(view, f[4])
            assert sha(orig) == f[2] and os.path.getsize(orig) == int(f[1]), 'view file is not the pinned one: ' + f[4]
            shutil.copyfile(orig, os.path.join(dst, 'objects', f[2]))
            f[4] = '-'
            moved.append(f[3])
            line = '\t'.join(f)
        new.append(line)
    assert moved, 'no seed entry uses a replaced file: nothing to do'
    nbody = '\n'.join(new) + '\n'
    end = hashlib.sha256(nbody.encode()).hexdigest()
    with open(os.path.join(dst, 'seed.idx'), 'w', newline='\n') as f:
        f.write(nbody + 'END ' + end + '\n')
    for m in moved:
        print('own object: ' + m, file=sys.stderr)
    print(end)


if __name__ == '__main__':
    main()
