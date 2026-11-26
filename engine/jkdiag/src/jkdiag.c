/* jkdiag.dll - one-shot crash capture for PES 2013 (32-bit) under Wine.
 *
 * Loaded by kitserver's kload like any module ("dll = jkdiag" in [kload]):
 * kload just calls LoadLibrary(<kitserver dir>\jkdiag), no exports needed.
 * DllMain only installs a vectored exception handler. On the first access
 * violation whose EIP is inside the main exe image and whose fault address is
 * in the null page (< 0x10000) it writes, into <dir of kitserver>\..\jkdiag\:
 *   crash.txt            registers, exception record, modules, stack, EBP
 *                        chain, code pointers on the stack, register memory,
 *                        unreadable image ranges
 *   stack.bin            2 KB from ESP
 *   code-XXXXXXXX.bin    512 bytes centred on EIP, 256 centred on each code
 *                        pointer found on the stack and on fixed addresses
 *   pes2013-image.bin    the main exe image as mapped (file offset == RVA,
 *                        unreadable pages zero-filled and listed)
 *   kserv-image.bin, kload-image.bin   same for those modules when loaded
 * then returns EXCEPTION_CONTINUE_SEARCH so the process dies as before.
 * Other AVs inside the image are only counted in avlog.txt (first 32).
 * All memory reads go through VirtualQuery + ReadProcessMemory; no CRT. */
#include <windows.h>

BOOL WINAPI K32EnumProcessModules(HANDLE, HMODULE *, DWORD, LPDWORD);

#define NULL_PAGE_LIMIT 0x10000u
#define MAX_MODS 512
#define MAX_CODE_DUMPS 96

static volatile LONG g_done;
static volatile LONG g_busy;
static volatile LONG g_avlog_lines;
static char g_dir[MAX_PATH];
static char g_txt[512 * 1024];
static DWORD g_len;
static BYTE g_page[4096];
static BYTE g_tmp[2048];
static HMODULE g_mods[MAX_MODS];
static DWORD g_code_done[MAX_CODE_DUMPS + 8];
static DWORD g_ncode;

/* fixed addresses of interest in PES 2013 v1.04 (dumped when readable) */
static const DWORD g_fixed[] = { 0x011E1062, 0x011D6A3E, 0x011D74F0, 0x012BCAC0, 0x011CA197 };

void *memset(void *d, int c, size_t n) { BYTE *p = d; while (n--) *p++ = (BYTE)c; return d; }
void *memcpy(void *d, const void *s, size_t n) { BYTE *p = d; const BYTE *q = s; while (n--) *p++ = *q++; return d; }

/* ---------- text output (no CRT) ---------- */
static void put(const char *s) { while (*s && g_len < sizeof(g_txt) - 1) g_txt[g_len++] = *s++; }
static void hexn(DWORD v, int digits)
{
    static const char h[] = "0123456789ABCDEF";
    char b[9]; int i;
    for (i = digits - 1; i >= 0; i--) { b[i] = h[v & 15]; v >>= 4; }
    b[digits] = 0; put(b);
}
static void hex(DWORD v) { hexn(v, 8); }
static void dec(DWORD v) { char b[12]; int i = 11; b[i] = 0; do { b[--i] = (char)('0' + v % 10); v /= 10; } while (v); put(b + i); }
static void kv(const char *k, DWORD v) { put(k); put("="); hex(v); put(" "); }
static void nl(void) { put("\r\n"); }

static void path_of(char *out, const char *name)
{
    char *o = out; const char *s = g_dir;
    while (*s) *o++ = *s++;
    while (*name) *o++ = *name++;
    *o = 0;
}
static void write_file(const char *name, const void *data, DWORD n)
{
    char p[MAX_PATH + 64]; DWORD w;
    HANDLE h;
    path_of(p, name);
    h = CreateFileA(p, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    if (n) WriteFile(h, data, n, &w, NULL);
    CloseHandle(h);
}

/* ---------- safe memory access ---------- */
static int page_readable(DWORD a)
{
    MEMORY_BASIC_INFORMATION mi;
    if (!VirtualQuery((void *)(ULONG_PTR)a, &mi, sizeof(mi))) return 0;
    if (mi.State != MEM_COMMIT) return 0;
    if (mi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return 0;
    if (!(mi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                        PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) return 0;
    return 1;
}
/* reads up to n bytes from a; stops at the first unreadable page; returns bytes read */
static DWORD safe_read(DWORD a, void *buf, DWORD n)
{
    DWORD done = 0;
    while (done < n) {
        DWORD cur = a + done, room = 0x1000 - (cur & 0xFFF), chunk = n - done;
        SIZE_T got = 0;
        if (chunk > room) chunk = room;
        if (cur < 0x10000 || !page_readable(cur)) break;
        if (!ReadProcessMemory(GetCurrentProcess(), (void *)(ULONG_PTR)cur, (BYTE *)buf + done, chunk, &got) || got != chunk) break;
        done += chunk;
    }
    return done;
}

/* ---------- modules ---------- */
typedef struct { DWORD base, size, tlo, thi; char name[64]; } modinfo;
static modinfo g_mi[MAX_MODS];
static DWORD g_nmods;
static int g_main = -1, g_kserv = -1, g_kload = -1;

static int ieq(const char *a, const char *b)
{
    while (*a && *b) { char x = *a++, y = *b++; if (x >= 'A' && x <= 'Z') x += 32; if (y >= 'A' && y <= 'Z') y += 32; if (x != y) return 0; }
    return *a == *b;
}
static void load_modules(void)
{
    DWORD need = 0, i, n;
    HMODULE main = GetModuleHandleA(NULL);
    if (!K32EnumProcessModules(GetCurrentProcess(), g_mods, sizeof(g_mods), &need)) need = 0;
    n = need / sizeof(HMODULE);
    if (n > MAX_MODS) n = MAX_MODS;
    for (i = 0; i < n; i++) {
        modinfo *m = &g_mi[g_nmods];
        IMAGE_DOS_HEADER dos; IMAGE_NT_HEADERS32 nt; IMAGE_SECTION_HEADER sh;
        char full[MAX_PATH]; const char *b; DWORD k, s;
        m->base = (DWORD)(ULONG_PTR)g_mods[i];
        if (safe_read(m->base, &dos, sizeof(dos)) != sizeof(dos) || dos.e_magic != IMAGE_DOS_SIGNATURE) continue;
        if (safe_read(m->base + dos.e_lfanew, &nt, sizeof(nt)) != sizeof(nt) || nt.Signature != IMAGE_NT_SIGNATURE) continue;
        m->size = nt.OptionalHeader.SizeOfImage;
        m->tlo = m->thi = 0;
        for (s = 0; s < nt.FileHeader.NumberOfSections && s < 96; s++) {
            DWORD sa = m->base + dos.e_lfanew + 4 + sizeof(IMAGE_FILE_HEADER) + nt.FileHeader.SizeOfOptionalHeader + s * sizeof(sh);
            if (safe_read(sa, &sh, sizeof(sh)) != sizeof(sh)) break;
            if (sh.Characteristics & IMAGE_SCN_MEM_EXECUTE) {
                DWORD lo = m->base + sh.VirtualAddress, hi = lo + sh.Misc.VirtualSize;
                if (!m->tlo || lo < m->tlo) m->tlo = lo;
                if (hi > m->thi) m->thi = hi;
            }
        }
        full[0] = 0;
        GetModuleFileNameA(g_mods[i], full, sizeof(full));
        for (b = full, k = 0; full[k]; k++) if (full[k] == '\\' || full[k] == '/') b = full + k + 1;
        for (k = 0; b[k] && k < sizeof(m->name) - 1; k++) m->name[k] = b[k];
        m->name[k] = 0;
        if (g_mods[i] == main) g_main = (int)g_nmods;
        if (ieq(m->name, "kserv.dll")) g_kserv = (int)g_nmods;
        if (ieq(m->name, "kload.dll")) g_kload = (int)g_nmods;
        g_nmods++;
    }
}
static int mod_of(DWORD a)
{
    DWORD i;
    for (i = 0; i < g_nmods; i++) if (a >= g_mi[i].base && a - g_mi[i].base < g_mi[i].size) return (int)i;
    return -1;
}
static void where(DWORD a)
{
    int m = mod_of(a);
    if (m < 0) { put("?"); return; }
    put(g_mi[m].name); put("+"); hex(a - g_mi[m].base);
}
static int in_text(int m, DWORD a) { return m >= 0 && g_mi[m].tlo && a >= g_mi[m].tlo && a < g_mi[m].thi; }
static int interesting_code(DWORD a)
{
    return in_text(g_main, a) || in_text(g_kserv, a) || in_text(g_kload, a);
}

/* ---------- dumps ---------- */
static void dump_code(DWORD center, DWORD size)
{
    DWORD i, start = center - size / 2, got;
    char name[32] = "code-";
    for (i = 0; i < g_ncode; i++) if (g_code_done[i] == center) return;
    if (g_ncode >= MAX_CODE_DUMPS) return;
    g_code_done[g_ncode++] = center;
    memset(g_tmp, 0, size);
    got = safe_read(start, g_tmp, size);
    if (!got) { /* maybe only the tail is readable: retry from the centre */
        got = safe_read(center, g_tmp + size / 2, size / 2);
        if (got) got += size / 2;
    }
    put("code dump "); hex(center); put(" ("); where(center); put(") start="); hex(start);
    put(" bytes="); dec(got); nl();
    if (!got) return;
    {
        static const char h[] = "0123456789ABCDEF";
        int k;
        for (k = 0; k < 8; k++) name[5 + k] = h[(center >> (28 - 4 * k)) & 15];
        name[13] = '.'; name[14] = 'b'; name[15] = 'i'; name[16] = 'n'; name[17] = 0;
    }
    write_file(name, g_tmp, size);
}
/* v2: 512-byte dumps of heap/data memory that registers and stack point to,
 * plus one more level through the dwords inside register-pointed blocks. */
#define MAX_HEAP_DUMPS 96
static DWORD g_heap_done[MAX_HEAP_DUMPS];
static DWORD g_nheap;
static BYTE g_heapbuf[512];
static int dump_heap(DWORD a, const char *why)
{
    DWORD i, got;
    char name[32] = "heap-";
    static const char h[] = "0123456789ABCDEF";
    if (a < 0x10000 || mod_of(a) >= 0) return 0;          /* null page or module image */
    for (i = 0; i < g_nheap; i++) if (g_heap_done[i] == a) return 0;
    if (g_nheap >= MAX_HEAP_DUMPS) return 0;
    memset(g_heapbuf, 0, sizeof(g_heapbuf));
    got = safe_read(a, g_heapbuf, sizeof(g_heapbuf));
    if (!got) return 0;
    g_heap_done[g_nheap++] = a;
    for (i = 0; i < 8; i++) name[5 + i] = h[(a >> (28 - 4 * i)) & 15];
    name[13] = '.'; name[14] = 'b'; name[15] = 'i'; name[16] = 'n'; name[17] = 0;
    write_file(name, g_heapbuf, sizeof(g_heapbuf));
    put("  "); hex(a); put(" "); put(why); put(" bytes="); dec(got); put(" first:");
    for (i = 0; i < 8 && i * 4 < got; i++) { put(" "); hex(((DWORD *)g_heapbuf)[i]); }
    nl();
    return 1;
}

static void hexdump(DWORD a, DWORD n)
{
    DWORD got = safe_read(a, g_tmp, n), i;
    if (!got) { put("  (unreadable)"); nl(); return; }
    for (i = 0; i < got; i++) {
        if (i % 16 == 0) { if (i) nl(); put("  "); hex(a + i); put(":"); }
        put(" "); hexn(g_tmp[i], 2);
    }
    nl();
}
static void dump_image(int m, const char *file)
{
    char p[MAX_PATH + 64]; DWORD off, w, bad_start = 0, in_bad = 0;
    HANDLE h;
    if (m < 0) { put(file); put(": module not loaded"); nl(); return; }
    path_of(p, file);
    h = CreateFileA(p, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { put(file); put(": create failed "); hex(GetLastError()); nl(); return; }
    put(file); put(": "); put(g_mi[m].name); put(" base="); hex(g_mi[m].base); put(" size="); hex(g_mi[m].size); nl();
    for (off = 0; off < g_mi[m].size; off += 0x1000) {
        DWORD n = g_mi[m].size - off < 0x1000 ? g_mi[m].size - off : 0x1000;
        DWORD got = safe_read(g_mi[m].base + off, g_page, n);
        if (got < n) {
            memset(g_page + got, 0, n - got);
            if (!in_bad) { in_bad = 1; bad_start = off; }
        } else if (in_bad) {
            put("  unreadable rva "); hex(bad_start); put("-"); hex(off); nl(); in_bad = 0;
        }
        WriteFile(h, g_page, n, &w, NULL);
    }
    if (in_bad) { put("  unreadable rva "); hex(bad_start); put("-"); hex(g_mi[m].size); nl(); }
    CloseHandle(h);
}

static void capture(EXCEPTION_POINTERS *ep)
{
    CONTEXT *c = ep->ContextRecord;
    EXCEPTION_RECORD *r = ep->ExceptionRecord;
    DWORD i, n, ebp, sp[512];
    static const char *rn[] = { "EAX", "EBX", "ECX", "EDX", "ESI", "EDI", "EBP", "ESP", "EIP" };
    DWORD rv[9];

    CreateDirectoryA(g_dir, NULL);
    load_modules();
    put("jkdiag crash capture v1"); nl();
    kv("pid", GetCurrentProcessId()); kv("tid", GetCurrentThreadId()); nl();
    put("exception "); kv("code", r->ExceptionCode); kv("flags", r->ExceptionFlags);
    kv("address", (DWORD)(ULONG_PTR)r->ExceptionAddress); kv("nparams", r->NumberParameters);
    kv("info0", r->NumberParameters > 0 ? (DWORD)r->ExceptionInformation[0] : 0);
    kv("info1", r->NumberParameters > 1 ? (DWORD)r->ExceptionInformation[1] : 0); nl();
    rv[0] = c->Eax; rv[1] = c->Ebx; rv[2] = c->Ecx; rv[3] = c->Edx; rv[4] = c->Esi;
    rv[5] = c->Edi; rv[6] = c->Ebp; rv[7] = c->Esp; rv[8] = c->Eip;
    put("context "); kv("ContextFlags", c->ContextFlags); nl();
    for (i = 0; i < 9; i++) kv(rn[i], rv[i]);
    nl();
    kv("EFLAGS", c->EFlags); kv("CS", c->SegCs); kv("SS", c->SegSs); kv("DS", c->SegDs);
    kv("ES", c->SegEs); kv("FS", c->SegFs); kv("GS", c->SegGs); nl();
    kv("DR0", c->Dr0); kv("DR1", c->Dr1); kv("DR2", c->Dr2); kv("DR3", c->Dr3); kv("DR6", c->Dr6); kv("DR7", c->Dr7); nl();
    put("EIP at "); where(c->Eip); nl();

    put("modules "); dec(g_nmods); nl();
    for (i = 0; i < g_nmods; i++) {
        put("  "); hex(g_mi[i].base); put(" "); hex(g_mi[i].size); put(" text ");
        hex(g_mi[i].tlo); put("-"); hex(g_mi[i].thi); put(" "); put(g_mi[i].name); nl();
    }

    put("registers -> memory (64 bytes each)"); nl();
    for (i = 0; i < 9; i++) {
        put(" "); put(rn[i]); put("="); hex(rv[i]); put(" "); where(rv[i]); nl();
        if (rv[i] >= 0x10000) hexdump(rv[i], 64); else { put("  (null page)"); nl(); }
    }

    memset(sp, 0, sizeof(sp));
    n = safe_read(c->Esp, sp, 2048);
    write_file("stack.bin", sp, n);
    put("stack from ESP="); hex(c->Esp); put(" bytes="); dec(n); nl();
    for (i = 0; i < n / 4; i++) {
        if (i % 8 == 0) { if (i) nl(); put("  +"); hexn(i * 4, 3); put(":"); }
        put(" "); hex(sp[i]);
    }
    nl();

    put("code pointers on stack (main/kserv/kload text)"); nl();
    for (i = 0; i < n / 4; i++)
        if (interesting_code(sp[i])) { put("  ESP+"); hexn(i * 4, 3); put(" "); hex(sp[i]); put(" "); where(sp[i]); nl(); }

    put("ebp chain"); nl();
    ebp = c->Ebp;
    for (i = 0; i < 32; i++) {
        DWORD fr[2];
        if (safe_read(ebp, fr, 8) != 8) { put("  stop: unreadable ebp="); hex(ebp); nl(); break; }
        put("  #"); dec(i); put(" ebp="); hex(ebp); put(" ret="); hex(fr[1]); put(" "); where(fr[1]); nl();
        if (fr[0] <= ebp) { put("  stop: saved ebp "); hex(fr[0]); put(" not above"); nl(); break; }
        ebp = fr[0];
    }

    put("pointed memory (heap-XXXXXXXX.bin, 512 bytes each)"); nl();
    {
        static const char *rr[] = { "<-EAX", "<-EBX", "<-ECX", "<-EDX", "<-ESI", "<-EDI", "<-EBP" };
        DWORD lvl1[7], k, j;
        for (i = 0; i < 7; i++) { lvl1[i] = 0; if (dump_heap(rv[i], rr[i])) lvl1[i] = rv[i]; }
        /* level 2: dwords inside the first 0x100 bytes of register-pointed blocks */
        for (i = 0; i < 7; i++) {
            DWORD inner[64];
            if (!lvl1[i]) continue;
            k = safe_read(lvl1[i], inner, sizeof(inner)) / 4;
            for (j = 0; j < k; j++) dump_heap(inner[j], "<-[reg]+x");
        }
        /* then the first 0x200 bytes of stack (the faulting frames) */
        for (i = 0; i < n / 4 && i < 128; i++) dump_heap(sp[i], "<-stack");
    }

    put("code dumps"); nl();
    dump_code(c->Eip, 512);
    for (i = 0; i < n / 4; i++) if (interesting_code(sp[i])) dump_code(sp[i], 256);
    for (i = 0; i < sizeof(g_fixed) / sizeof(g_fixed[0]); i++)
        if (in_text(g_main, g_fixed[i])) dump_code(g_fixed[i], 256);
    {   /* frame returns too */
        ebp = c->Ebp;
        for (i = 0; i < 32; i++) {
            DWORD fr[2];
            if (safe_read(ebp, fr, 8) != 8) break;
            if (interesting_code(fr[1])) dump_code(fr[1], 256);
            if (fr[0] <= ebp) break;
            ebp = fr[0];
        }
    }

    put("images"); nl();
    dump_image(g_main, "pes2013-image.bin");
    dump_image(g_kserv, "kserv-image.bin");
    dump_image(g_kload, "kload-image.bin");
    put("end"); nl();
    write_file("crash.txt", g_txt, g_len);
}

static void avlog(EXCEPTION_RECORD *r, CONTEXT *c)
{
    char line[96], p[MAX_PATH + 64];
    DWORD k = 0, w; HANDLE h;
    const char *pre = "skipped AV eip=";
    static const char hx[] = "0123456789ABCDEF";
    DWORD vals[2]; int j, v;
    if (InterlockedIncrement(&g_avlog_lines) > 32) return;
    vals[0] = c->Eip; vals[1] = r->NumberParameters > 1 ? (DWORD)r->ExceptionInformation[1] : 0;
    while (*pre) line[k++] = *pre++;
    for (v = 0; v < 2; v++) {
        if (v) { const char *s = " addr="; while (*s) line[k++] = *s++; }
        for (j = 7; j >= 0; j--) line[k++] = hx[(vals[v] >> (4 * j)) & 15];
    }
    line[k++] = '\r'; line[k++] = '\n';
    CreateDirectoryA(g_dir, NULL);
    path_of(p, "avlog.txt");
    h = CreateFileA(p, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    WriteFile(h, line, k, &w, NULL);
    CloseHandle(h);
}

static LONG CALLBACK handler(EXCEPTION_POINTERS *ep)
{
    EXCEPTION_RECORD *r = ep->ExceptionRecord;
    DWORD eip = ep->ContextRecord->Eip, base, size;
    IMAGE_DOS_HEADER *dos; IMAGE_NT_HEADERS32 *nt;
    if (r->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || g_done) return EXCEPTION_CONTINUE_SEARCH;
    base = (DWORD)(ULONG_PTR)GetModuleHandleA(NULL);
    dos = (IMAGE_DOS_HEADER *)(ULONG_PTR)base;
    nt = (IMAGE_NT_HEADERS32 *)(ULONG_PTR)(base + dos->e_lfanew);
    size = nt->OptionalHeader.SizeOfImage;
    if (eip < base || eip - base >= size) return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedCompareExchange(&g_busy, 1, 0) != 0) return EXCEPTION_CONTINUE_SEARCH; /* nested */
    if (r->NumberParameters < 2 || (DWORD)r->ExceptionInformation[1] >= NULL_PAGE_LIMIT) {
        avlog(r, ep->ContextRecord);
    } else if (InterlockedCompareExchange(&g_done, 1, 0) == 0) {
        capture(ep);
    }
    InterlockedExchange(&g_busy, 0);
    return EXCEPTION_CONTINUE_SEARCH;
}

/* <dir of this dll>\..\jkdiag\  (kitserver\jkdiag.dll -> PES13\jkdiag\) */
static void init_dir(HINSTANCE self)
{
    char p[MAX_PATH]; DWORD n = GetModuleFileNameA(self, p, MAX_PATH), k, cut = 0, seps = 0;
    const char *tail = "jkdiag\\";
    for (k = n; k > 0; k--) if (p[k - 1] == '\\' || p[k - 1] == '/') { if (++seps == 2) { cut = k; break; } }
    if (!cut || cut + 8 >= MAX_PATH) { const char *f = "Z:\\data\\JkPro\\PES13\\"; cut = 0; while (*f) p[cut++] = *f++; }
    for (k = 0; tail[k]; k++) g_dir[cut + k] = tail[k];
    for (k = 0; k < cut; k++) g_dir[k] = p[k];
    g_dir[cut + 7] = 0;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        init_dir(inst);
        AddVectoredExceptionHandler(1, handler);
    }
    return TRUE;
}
