#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright 2026 XxXJoker13XxX and contributors
"""JKPro ws2_32.prx: the JkFlow 1.00 ws2_32 plus the JK_HOSTS table (pw_hosts.h).

Usage (WSL): python3 build_jkpro_ws2_32.py OUT
  JKFLOW_BUILD_ROOT  the JkFlow build root (default ~/orbis-win-host-bootstrap)
  JKFLOW_EXPORT      the JkFlow source export (default: ../../../JkFlow-export from this repo)

Steps
  control  the retained JkFlow link ELF a52c7aa7 through the same post-link chain
           (paths6 prefixes, paths6b env names, paths7 /data/blurdev/) and the
           create-fself conversion gives the shipped ws2_32.prx 35b8e2e8 byte
           for byte. That proves the chain; the build stops otherwise.
  jkpro    the JkFlow source (blur100-ws2_32/src) + the pw_console_resolve hook
           below, compiled with the JkFlow flags and neutral path prefixes,
           linked with the retained objects, then the same chain.
  scan     the new ELF and prx hold no build-host path and no developer path.
"""
import getpass, hashlib, importlib.util, json, os, re, shutil, subprocess, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
B = Path(os.environ.get('JKFLOW_BUILD_ROOT', Path.home() / 'orbis-win-host-bootstrap'))
EXP = Path(os.environ.get('JKFLOW_EXPORT', HERE.parents[2] / 'JkFlow-export'))
ORB = B / 'orbis-win-9749fcd'
SDK = ORB / 'sdk/orbis-sdk-v1/sdk'
CC = ORB / 'build/ps4-wine-sdk/bin/prospero-clang'
OLDP = B / 'native248-build-wine/prx'
J = B / 'blur100-ws2_32'
SRC = J / 'src/pw_ws2_32_libc.c'
INC = Path(os.environ.get('JKFLOW_WINE_PS5', HERE.parents[2] / 'Ps4 BLUR - PC project/research/native-wine/shared-prospero-108194e/wine/ps5'))
P6B = B / 'blur100-paths6b'
SHIPPED = '35b8e2e89535dc8fb7cedbaac701c618189856f21505176d220aa61245e3e66d'
PINS = {SRC: None, J / 'blur100/ws2_32.shared.elf': 'a52c7aa7821f38235bc362c5794c08d1d72eebb8dcaddc8a7d1ed7e2611e003e'}
STUBS = [('ntdll.prx', P6B / 'prx/ntdll/elf/candidate.elf'), ('win32u.prx', P6B / 'prx/win32u/elf/candidate.elf'),
         ('wineserver.prx', P6B / 'prx/wineserver/elf/candidate.elf'),
         ('libvulkan.prx', P6B / 'prx-radvlog/libvulkan/elf/libvulkan.shared.elf')]
_HOME = (os.path.expanduser('~').rstrip('/') + '/').encode()
_HOME_TO = b'/build/jkflw/' if len(_HOME) == 13 else b'/build/' + b'j' * (len(_HOME) - 8) + b'/'   # equal length
PATH_MAP = [(_HOME, _HOME_TO),(b'/home/zpeirce/', b'/build/jkflow/'), (b'/home/runner/', b'/build/ci-rt/')]
RES_FLAGS = ['-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror', '-fPIC', '-D_REENTRANT', '-DPW_PS4_DNS', '-femulated-tls',
             '-I' + str(SDK / 'include')]
HOOK_AT = '''    *count = 0;
    /* Initialises libSceNet once;'''
HOOK = '''    *count = 0;
    /* JKPro online: names in JK_HOSTS (set by the launcher) first. */
    if (family != AF_INET6) {
        unsigned char ip[4];
        if (pw_hosts_lookup(getenv("JK_HOSTS"), name, ip)) {
            answers[0].family = AF_INET;
            memcpy(answers[0].bytes, ip, 4);
            *count = 1;
            return 0;
        }
    }
    /* Initialises libSceNet once;'''
INCLUDE_AT = '#include "pw_ws2_32_resolver.h"\n'
sha = lambda b: hashlib.sha256(b).hexdigest()
fsha = lambda p: sha(Path(p).read_bytes())

env = dict(os.environ, OO_PS4_TOOLCHAIN=str(SDK), PW_SHARED_LIBC='1', PW_PRX_LINK='1', MESA_BUILD=str(B / 'native248-platform'))
env['PATH'] = ':'.join(str(p) for p in (B / 'native248-build-wine/tools', B / 'private-bin', B / 'deps/rootfs/usr/bin')) + ':' + env['PATH']
os.environ['PATH'] = env['PATH']   # postlink_devpaths.convert runs create-fself-repro.sh with os.environ
# As build_blur100_ws2_32.py: find's directory order, one line (the wrapper links ntdll etc. from these stubs).
_found = subprocess.check_output(['find', str(OLDP / 'stubs'), '-maxdepth', '1', '-name', '*.so', '-printf', '%p '], text=True)
env['PW_PRX_STUBS'] = '-L%s -Wl,--as-needed %s' % (OLDP / 'stubs', _found)


def run(argv, cwd=None):
    p = subprocess.run([str(x) for x in argv], cwd=cwd, env=env, text=True, capture_output=True)
    assert not p.returncode, (str(argv[0])[-30:], p.stdout[-1500:], p.stderr[-2500:])


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def postlink(elf_bytes, pc, devpaths):
    d, _ = pc.replace(elf_bytes, PATH_MAP)                 # paths6
    d = paths6b(d, pc)                                     # paths6b
    d, _ = devpaths.patch_bytes(d)                         # paths7
    assert len(d) == len(elf_bytes)
    return d


def paths6b(d, pc):
    return pc.replace(d, pc.ENV6B)[0]


def main():
    out = Path(sys.argv[1]).resolve()
    assert not out.exists(), ('exists', out)
    out.mkdir(parents=True)
    pc = load('paths_clean', EXP / 'wine/tools/paths_clean.py')
    devpaths = load('postlink_devpaths', EXP / 'title/postlink_devpaths.py')
    for p, want in PINS.items():
        PINS[p] = fsha(p) if want is None else want
        assert fsha(p) == PINS[p], ('input changed', p)

    # control: the shipped JkFlow module from its retained link ELF
    ctl = out / 'control'
    ctl.mkdir()
    (ctl / 'ws2_32.shared.elf').write_bytes(postlink((J / 'blur100/ws2_32.shared.elf').read_bytes(), pc, devpaths))
    prx, _ = devpaths.convert(str(ORB), str(ctl / 'ws2_32.shared.elf'), str(ctl / 'conv'), 'ws2_32', STUBS_S)
    assert sha(prx) == SHIPPED, ('control prx', sha(prx))
    print('PASS control: post-link chain + conversion = shipped ws2_32.prx', SHIPPED[:8], flush=True)

    # jkpro build
    work = out / 'jkpro'
    (work / 'src').mkdir(parents=True)
    t = SRC.read_text()
    assert t.count(HOOK_AT) == 1 and t.count(INCLUDE_AT) == 1
    t = t.replace(INCLUDE_AT, INCLUDE_AT + '#include "pw_hosts.h"\n').replace(HOOK_AT, HOOK)
    (work / 'src' / SRC.name).write_text(t)
    shutil.copyfile(HERE / 'pw_hosts.h', work / 'src/pw_hosts.h')
    run([CC, *RES_FLAGS, '-iquote', INC, '-ffile-prefix-map=%s=/build/jkpro/ws2_32' % (work / 'src'),
         '-ffile-prefix-map=%s=/build/jkpro/ws2_32/inc' % INC, '-fdebug-prefix-map=%s=/build/jkpro' % work,
         '-c', work / 'src' / SRC.name, '-o', work / 'resolver.o'], cwd=work)
    run([CC, '-shared', '-Wl,-Bsymbolic', '-Wl,--eh-frame-hdr', '-Wl,-soname,ws2_32.prx', '-Wl,--warn-unresolved-symbols',
         '-o', work / 'link.elf', OLDP / 'obj/ws2_32_unixlib_dns.o', work / 'resolver.o', J / 'blur100/pw_emutls_ws2_32.o',
         J / 'blur100/pw_wow_log_ws2_32.o', OLDP / 'heap/pw_wine_heap_libc.o', OLDP / 'obj/pw_wine_dl_libc.o',
         OLDP / 'obj/ws2_32_desc.o', '-lSceNetCtl'], cwd=work)
    (work / 'elf').mkdir()
    elf = work / 'elf/ws2_32.shared.elf'
    d = postlink((work / 'link.elf').read_bytes(), pc, devpaths)
    # The retained JkFlow objects carry their build folder as a DWARF comp_dir (also in the JkFlow ELF);
    # JKPro replaces it with a neutral string of the same length.
    for m in sorted(set(re.findall(rb'/mnt/c/[^\0]*', d))):
        d = d.replace(m, (b'/build/jkpro/src' + b'_' * len(m))[:len(m)])
    elf.write_bytes(d)
    prx, _ = devpaths.convert(str(ORB), str(elf), str(work / 'conv'), 'ws2_32', STUBS_S)
    final = out / 'ws2_32.prx'
    final.write_bytes(prx)

    bad = re.compile(re.escape(getpass.getuser().encode()) + rb'|zpeirce|/home/|/mnt/|OneDrive|/data/blurdev|orbis-env\.txt')
    for p in (elf, final):
        hits = sorted(set(m.group(0) for m in bad.finditer(p.read_bytes())))
        assert not hits, ('scan', p, hits)
    assert b'JK_HOSTS' in prx
    diff = subprocess.run(['diff', '-u', '--label', 'a/pw_ws2_32_libc.c', '--label', 'b/pw_ws2_32_libc.c', str(SRC),
                           str(work / 'src' / SRC.name)], text=True, capture_output=True).stdout
    (out / 'jkpro-ws2_32.patch').write_text(diff)
    m = dict(kind='jkpro-ws2_32', status='BUILT_NOT_PS4_TESTED', control_prx=SHIPPED, prx_sha256=sha(prx),
             bytes=len(prx), elf_sha256=fsha(elf), patch_sha256=fsha(out / 'jkpro-ws2_32.patch'),
             pw_hosts_h_sha256=fsha(HERE / 'pw_hosts.h'), inputs={str(k): v for k, v in PINS.items()})
    (out / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
    print(json.dumps(m, indent=2))


STUBS_S = [(lib, str(p)) for lib, p in STUBS]

if __name__ == '__main__':
    main()
