#!/usr/bin/env python3
"""check_capture.py <jkdiag-out-dir> <crashtest.exe> <kserv.dll> <wine-stdout>
Validates what jkdiag wrote for the crashtest run. Exit 0 = pass."""
import os, re, struct, sys

out, exe, kserv, log = sys.argv[1:5]
fails = []
def check(c, m):
    print(("PASS " if c else "FAIL ") + m)
    if not c: fails.append(m)

def pe(path):
    d = open(path, "rb").read()
    p = struct.unpack_from("<I", d, 0x3C)[0]
    nsec = struct.unpack_from("<H", d, p + 6)[0]
    optsz = struct.unpack_from("<H", d, p + 20)[0]
    base, = struct.unpack_from("<I", d, p + 24 + 28)
    soi, = struct.unpack_from("<I", d, p + 24 + 56)
    secs = []
    for i in range(nsec):
        name, vs, va, rs, ro, _, _, _, _, ch = struct.unpack_from("<8sIIIIIIHHI", d, p + 24 + optsz + 40 * i)
        secs.append((name.rstrip(b"\0").decode(), vs, va, rs, ro, ch))
    return d, base, soi, secs

txt_p = os.path.join(out, "crash.txt")
check(os.path.isfile(txt_p), "crash.txt written")
if not os.path.isfile(txt_p):
    sys.exit(1)
t = open(txt_p, "rb").read().decode("ascii", "replace")
stdout = open(log, "rb").read().decode("ascii", "replace")
check("handled AV survived" in stdout, "handled non-null AV did not stop the program")
check("SURVIVED" not in stdout, "process still died after the capture (CONTINUE_SEARCH)")
m = re.search(r"w7_crash=([0-9A-Fa-fx]+)", stdout)
w7 = int(m.group(1), 16) if m else None

check(re.search(r"code=C0000005 ", t) is not None, "exception code C0000005")
check("info0=00000000" in t, "info0 = read access")
check("info1=00000007" in t, "info1 = fault address 7")
check("EDI=00000000" in t, "EDI = 0 recorded")
eip = int(re.search(r"EIP=([0-9A-F]{8})", t).group(1), 16)
check(w7 is not None and w7 <= eip < w7 + 32, "EIP %#x inside w7_crash (%s)" % (eip, hex(w7) if w7 else None))
check(re.search(r"EIP at crashtest\.exe\+", t) is not None, "EIP attributed to crashtest.exe+offset")
for k in ("EFLAGS=", "CS=", "SS=", "FS=", "DR7=", "tid=", "flags="):
    check(k in t, "context/record field %s present" % k)
check(re.search(r"  [0-9A-F]{8} [0-9A-F]{8} text .* jkdiag\.dll", t) is not None, "module list has jkdiag.dll")
check(re.search(r"kserv\.dll", t) is not None, "module list has kserv.dll")

# skipped AV logged, not captured
avl = os.path.join(out, "avlog.txt")
check(os.path.isfile(avl) and "addr=DEAD0000" in open(avl).read(), "handled AV logged to avlog.txt only")

# stack
sb = os.path.join(out, "stack.bin")
check(os.path.isfile(sb) and os.path.getsize(sb) == 2048, "stack.bin is 2048 bytes")

# EIP code dump
cd = os.path.join(out, "code-%08X.bin" % eip)
check(os.path.isfile(cd) and os.path.getsize(cd) == 512, "code-<eip>.bin 512 bytes")
if os.path.isfile(cd):
    c = open(cd, "rb").read()
    check(c[256:260] == b"\x0f\xb6\x4f\x07", "code dump centred on the faulting movzx (0F B6 4F 07)")

# image dump == mapped exe: size and .text bytes match the file
d, base, soi, secs = pe(exe)
img_p = os.path.join(out, "pes2013-image.bin")
check(os.path.isfile(img_p) and os.path.getsize(img_p) == soi, "pes2013-image.bin size == SizeOfImage (%#x)" % soi)
if os.path.isfile(img_p):
    img = open(img_p, "rb").read()
    check(img[:2] == b"MZ", "image dump starts with MZ (file offset == RVA)")
    for name, vs, va, rs, ro, ch in secs:
        if ch & 0x20000000:
            n = min(vs, rs)
            check(img[va:va + n] == d[ro:ro + n], "executable section %s matches file bytes (%#x bytes)" % (name, n))
    if w7:
        check(img[eip - base:eip - base + 4] == b"\x0f\xb6\x4f\x07", "image dump has the movzx at EIP-base")
kimg = os.path.join(out, "kserv-image.bin")
kd, kbase, ksoi, _ = pe(kserv)
check(os.path.isfile(kimg) and os.path.getsize(kimg) == ksoi, "kserv-image.bin size == SizeOfImage")
check("kload-image.bin: module not loaded" in t, "absent kload reported, not crashed")

# return address of w7_crash into caller: on the stack, in the ebp chain, dumped
rets = re.findall(r"ESP\+[0-9A-F]{3} ([0-9A-F]{8}) crashtest\.exe\+", t)
check(len(rets) >= 1, "code pointers into crashtest.exe found on the stack (%d)" % len(rets))
check(re.search(r"#0 ebp=[0-9A-F]{8} ret=[0-9A-F]{8} crashtest\.exe\+", t) is not None, "ebp chain frame #0 returns into crashtest.exe")
dumped = [r for r in rets if os.path.isfile(os.path.join(out, "code-%s.bin" % r))]
check(len(dumped) >= 1 and all(os.path.getsize(os.path.join(out, "code-%s.bin" % r)) == 256 for r in dumped),
      "256-byte dumps written for stack code pointers")
check(re.search(r" EDI=00000000 \?\r\n  \(null page\)", t) is not None, "null register reported, not read")
check(re.search(r" ESP=[0-9A-F]{8} \?\r\n  [0-9A-F]{8}:( [0-9A-F]{2}){16}", t) is not None, "64 bytes shown at ESP")
check(t.rstrip().endswith("end"), "crash.txt complete")

# v2: pointed-to heap memory, two levels (register -> object -> object+0x5c)
heaps = [f for f in os.listdir(out) if f.startswith("heap-") and f.endswith(".bin")]
blobs = {f: open(os.path.join(out, f), "rb").read() for f in heaps}
check(any(b.startswith(b"JKDIAG-MARKER-L1") for b in blobs.values()), "level-1 heap dump of the object ESI points to")
check(any(b.startswith(b"JKDIAG-MARKER-L2") for b in blobs.values()), "level-2 heap dump of the pointer stored inside it")
check(all(len(b) == 512 for b in blobs.values()) and 0 < len(blobs) <= 96, "heap dumps are 512 bytes, capped (%d files)" % len(blobs))
check("pointed memory" in t, "crash.txt lists pointed memory")

print("\n%d failure(s)" % len(fails))
sys.exit(1 if fails else 0)
