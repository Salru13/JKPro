# jkdiag.dll: one-shot crash capture for PES 2013 (W7)

`jkdiag.dll` is a 32-bit kitserver module. It records the state of PES 2013
at its first null-page access violation, so the W7 crash at `011CA197` can be
reverse-engineered from the *decrypted* game code. (The `pes2013.exe` `.text`
is SecuROM-encrypted on disk.) It changes nothing in the game and does not
stop the crash.

## Install (the lead deploys; read-only from the agent side)

1. Copy `jkdiag.dll` to `<PES13>\kitserver\jkdiag.dll`.
2. In `kitserver\config.txt`, under `[kload]`, add `dll = jkdiag` as the
   **first** `dll =` line.
3. Reproduce the crash. The output goes to `<PES13>\jkdiag\`, that is
   `Z:\data\JkPro\PES13\jkdiag\`. The path is derived from the DLL location,
   which is two levels up plus `jkdiag\`.
4. To remove it, delete the config line.

Why this load path works (kload source, `kloadLoadDlls`): kload calls plain
`LoadLibrary(<kitserver dir>\<name>)` for each `dll =` entry. It needs no
exports, and `RegisterKModule` only writes a log line. The player's older
kload logs `Loading module "Z:\data\JkPro\PES13\kitserver\kserv"`, so it uses
the same scheme without a `modules\` subfolder. A failed load is logged as
`... ERROR!` and the game keeps running.

## Behaviour

- `DllMain` (`PROCESS_ATTACH`) does only two things: it computes the output
  directory and calls `AddVectoredExceptionHandler(1, handler)`. It does no
  I/O. The DLL has no CRT; it imports only KERNEL32.
- **Trigger:** `EXCEPTION_ACCESS_VIOLATION`, EIP inside the main exe image,
  and fault address < 0x10000 (the null page). It fires once. Other access
  violations in the image are SecuROM's or handled ones; they are appended to
  `avlog.txt` (first 32 only) and do not use up the capture.
- **Output:**
  - `crash.txt`: the full CONTEXT (general registers, EFLAGS, segment and
    debug registers), tid/pid, the exception record (code, flags, info0/1),
    and the module list (base, size, text range).
  - 64 bytes at each register that points to readable memory.
  - 2 KB of stack, also written to `stack.bin`.
  - Every stack dword that points into the text of the exe, kserv or kload,
    each with module+offset.
  - An EBP chain of up to 32 frames.
  - Any unreadable image ranges.
  - `code-XXXXXXXX.bin`: 512 bytes centred on EIP, and 256 bytes for each
    stack or frame code pointer and for the fixed addresses `011E1062`,
    `011D6A3E`, `011D74F0`, `012BCAC0` and `011CA197`.
  - `pes2013-image.bin`: the whole exe image as mapped. File offset equals
    RVA, so load it at base 0x400000. Unreadable pages are zero-filled and
    listed.
  - `kserv-image.bin` and `kload-image.bin`, when those modules are loaded.
- Every read uses VirtualQuery (only committed pages, never NOACCESS or
  GUARD) followed by ReadProcessMemory, so the handler cannot fault. A
  re-entrancy guard covers nested exceptions. The handler then returns
  `EXCEPTION_CONTINUE_SEARCH`, so the process ends exactly as before. The
  runtime's own `PW_EXCEPTION32` record is still written too.
- Size: about 20 MB for the exe image, plus kserv and kload, written once at
  crash time.

## Build and test (WSL, host Wine)

    bash engine/jkdiag/build.sh [OUTDIR]   # i686-w64-mingw32-gcc from ~/orbis-win-host-bootstrap/private-bin
    bash engine/jkdiag/test.sh             # RED with test/stub.c, then GREEN with jkdiag.dll

`test/crashtest.exe` runs in a console-like layout (`PES13/kitserver/`):

1. It loads jkdiag by path, as kload does.
2. It loads a stand-in `kserv.dll`.
3. It takes one handled access violation at `0xDEAD0000`. This must not
   trigger the capture.
4. It faults with the W7 instruction, `movzx ecx, byte [edi+7]` with
   edi = 0.

`test/check_capture.py` checks 34 properties, among them: code C0000005,
info1 = 7, EDI = 0, EIP inside the faulting function, the 512-byte dump
centred on `0F B6 4F 07`, image size equal to SizeOfImage with `.text`
identical to the file, the kserv image, stack code pointers and their dumps,
an EBP chain into the exe, the handled AV only in `avlog.txt`, and the
process still dying.

**v2** adds `heap-XXXXXXXX.bin`: 512 bytes at each non-image address held in
a register, one more level through the dwords in the first 0x100 bytes of
each block a register points to (for example `this+0x5c`), and then the
first 0x200 bytes of stack. At most 96 dumps are written, and each is listed
under "pointed memory" in crash.txt. In the W7 capture this shows the loader
object, the BIN buffer header and the model/resource objects that v1 missed.

Result on 2026-11-25:
- v1: RED 1 failure (no crash.txt); GREEN 0 failures out of 34.
- v2: RED against the v1 DLL gave 4 failures, all in the new heap checks.
  GREEN gave 0 failures out of 38 (level-1 and level-2 heap markers found).

The delivered `jkdiag.dll` v2 has sha256
`a8eb6ff9a281c5f984fe561a4dbf3e677df60803881bca29a9f4c4edd16c7e03` and
imports KERNEL32 only. It was tested with
`JKDIAG_DLL=engine/jkdiag/jkdiag.dll bash engine/jkdiag/test.sh`. Rebuilds
get a different hash because of the PE timestamp.

## Analysis plan once a capture exists

1. Load `pes2013-image.bin` at 0x400000 (raw, i386) into objdump or Ghidra.
   Check that `.text` at `011CA197` is `0F B6 4F 07 83 C1 F8`.
2. Disassemble the function that contains `011CA197`, from its prologue to
   its `ret`. Find where `edi` comes from (the load that produced NULL), and
   what `[edi+7]` and the following `add ecx,-8` mean (a record header byte?).
3. Look at the caller around return address `011E1062` (taken from the
   stack, since the EBP chain will likely be empty: ebp held heap data). Find
   which object or array is passed and how it is indexed (team, slot, kit
   record).
4. Look at the kserv hook sites `011D6A3E`, `011D74F0` and `012BCAC0` as
   patched in memory. Check the `E8` call into kserv plus NOPs, and compare
   with the instruction they replaced: does `mov esi/ecx,[016A3E30]` really
   fit 6 bytes, and do 12 bytes fit at the third site? Then check whether the
   crash function uses the hooked num-slots value. A slot ≥ 0x1000
   (x-slot) or an index past the real slot count would read past an array
   whose entries are pointers, and would give NULL.
5. Check the game's references to the kits AFS table (`0x0c`, replaced by
   kserv's 26734-slot copy) and to the euro-kit list pointer (`019E8288`,
   replaced by a sorted, deduplicated heap copy). Test whether either feeds
   `edi`.
6. Write a fix as a kserv binary patch only once the chain from kserv's
   change to the NULL is shown in that code.
