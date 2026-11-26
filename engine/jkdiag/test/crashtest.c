/* crashtest.exe <jkdiag.dll> [kserv.dll]
 * Loads jkdiag the way kload does (LoadLibrary by path), loads a stand-in
 * kserv.dll, takes one HANDLED non-null access violation (must not trigger
 * the capture), then faults exactly like W7: movzx ecx, byte [edi+7], edi=0. */
#include <windows.h>
#include <stdio.h>

#define BAD_ADDR 0xDEAD0000u

static LONG CALLBACK skip_bad(EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
        ep->ExceptionRecord->ExceptionInformation[1] == BAD_ADDR) {
        ep->ContextRecord->Eip += 5;            /* mov eax,[BAD_ADDR] = A1 imm32 */
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

__attribute__((noinline)) static int handled_av(void)
{
    int r;
    __asm__ volatile(".byte 0xA1\n\t.long 0xDEAD0000\n\tmov %%eax,%0" : "=r"(r) : : "eax");
    return r;
}

/* heap object graph like the game's: ESI -> L1 block (marker + pointer) -> L2 block */
static char *g_l1;

__attribute__((noinline)) int w7_crash(volatile unsigned char *p)
{
    int r;
    __asm__ volatile("movzbl 7(%%edi),%%ecx\n\tmov %%ecx,%0" : "=r"(r) : "D"(p), "S"(g_l1) : "ecx");
    return r;
}

__attribute__((noinline)) int caller(void) { return w7_crash(NULL) + 1; }

static void make_graph(void)
{
    char *l2 = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 256);
    g_l1 = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 256);
    lstrcpyA(l2, "JKDIAG-MARKER-L2");
    lstrcpyA(g_l1, "JKDIAG-MARKER-L1");
    *(char **)(g_l1 + 0x5c) = l2;      /* like this+0x5c in the W7 loader */
}

int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    if (!LoadLibraryA(argv[1])) { printf("LoadLibrary jkdiag failed %lu\n", GetLastError()); return 3; }
    if (argc > 2 && !LoadLibraryA(argv[2])) { printf("LoadLibrary kserv failed %lu\n", GetLastError()); return 4; }
    make_graph();
    AddVectoredExceptionHandler(0, skip_bad);
    handled_av();
    printf("handled AV survived\n");
    fflush(stdout);
    printf("w7_crash=%p caller=%p\n", (void *)w7_crash, (void *)caller);
    fflush(stdout);
    caller();
    printf("SURVIVED\n");
    return 0;
}
