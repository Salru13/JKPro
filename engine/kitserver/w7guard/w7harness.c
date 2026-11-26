/* w7harness.exe <kserv.dll>
 * Behavioural test of the W7 decision-point guard without the game.
 * Maps kserv.dll (no DllMain, forced relocation), plants the game's original
 * 6 bytes at 0x011E0EC6 (`cmp byte [edx+78h],0; je 011E0F35`), installs the
 * hook exactly like kload's HookCallPoint(code[7], func, codeShift, numNops)
 * using the values in the DLL, then runs the site for a matrix of
 * (model+78h, loader flags, bin entry count) and checks:
 *   - where execution continues: 0x011E0ECC (texture-from-bin path, original
 *     fall-through) or 0x011E0F35 (the game's [model+78h]==0 path);
 *   - every register and the stack pointer are preserved. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

volatile DWORD g_saved_esp, g_where, g_in[7], g_out[7], g_land_esp;
static BYTE g_model[0x100], g_this[0x100];

/* run_site: load eax,ebx,ecx,edx,esi,edi,ebp from g_in, jump to the site.
 * land pads store the same registers to g_out and return to C. */
__asm__(
    ".globl _run_site\n_run_site:\n"
    "  pushal\n  movl %esp,_g_saved_esp\n"
    "  movl _g_in+4,%ebx\n  movl _g_in+8,%ecx\n  movl _g_in+12,%edx\n"
    "  movl _g_in+16,%esi\n  movl _g_in+20,%edi\n  movl _g_in+24,%ebp\n  movl _g_in,%eax\n"
    "  pushl $0x011E0EC6\n  ret\n"
    ".globl _land_keep\n_land_keep:\n  movl $1,_g_where\n  jmp store\n"
    ".globl _land_alt\n_land_alt:\n  movl $2,_g_where\n"
    "store:\n  movl %esp,_g_land_esp\n  movl %eax,_g_out\n  movl %ebx,_g_out+4\n  movl %ecx,_g_out+8\n"
    "  movl %edx,_g_out+12\n  movl %esi,_g_out+16\n  movl %edi,_g_out+20\n  movl %ebp,_g_out+24\n"
    "  movl _g_saved_esp,%esp\n  popal\n  ret\n");
void run_site(void); void land_keep(void); void land_alt(void);

static void jmp_to(DWORD at, void *to) { BYTE *p = (BYTE *)at; p[0] = 0xE9; *(DWORD *)(p + 1) = (DWORD)to - (at + 5); }

int main(int argc, char **argv)
{
    int fails = 0, m78, fl, cnt, ci, fi;
    static const int flags[] = { 0x47, 0x23, 0x1E, 0x12, 0x0C, 0x02, 0x01, 0x03 };
    BYTE *game, *k; DWORD func, target, old, i, site;
    VirtualAlloc((void *)0x10000000, 0x10000, MEM_RESERVE, PAGE_NOACCESS);
    game = VirtualAlloc((void *)0x011C0000, 0x30000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (game != (BYTE *)0x011C0000) { printf("FAIL cannot map game range\n"); return 2; }
    memset(game, 0xCC, 0x30000);
    memcpy((void *)0x011E0EC6, "\x80\x7a\x78\x00\x74\x69", 6);   /* original bytes (capture image) */
    jmp_to(0x011E0ECC, land_keep); jmp_to(0x011E0F35, land_alt);

    k = (BYTE *)LoadLibraryExA(argv[1], NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!k) { printf("FAIL load %lu\n", GetLastError()); return 3; }
    VirtualProtect(k + 0x1000, 0x25000, PAGE_EXECUTE_READ, &old);
    site = *(DWORD *)(k + 0x310c8);                  /* code[C_READ_UNIFORM_PICK], v1.04 row */
    func = *(DWORD *)(k + 0x8792);
    printf("kserv %p hook addr %#lx func %#lx codeShift %d numNops %d\n", k, site, func, k[0x8790], k[0x878E]);
    if (site != 0x011E0EC6) { printf("FAIL code[7] is %#lx, not 0x011E0EC6 (no hook would be installed)\n", site); return 1; }
    if (k[0x878E] != 1) { printf("FAIL numNops %d != 1 (6-byte site)\n", k[0x878E]); fails++; }
    target = func + k[0x8790];                        /* kload HookCallPoint */
    ((BYTE *)site)[0] = 0xE8; *(DWORD *)(site + 1) = target - (site + 5);
    for (i = 0; i < (DWORD)k[0x878E]; i++) ((BYTE *)site)[5 + i] = 0x90;

    for (m78 = 0; m78 <= 2; m78++)
    for (fi = 0; fi < (int)(sizeof(flags) / sizeof(flags[0])); fi++)
    for (cnt = 0; cnt <= 4; cnt++) {
        int want, ok;
        fl = flags[fi];
        g_model[0x78] = (BYTE)m78; *(WORD *)(g_this + 0x58) = (WORD)fl;
        g_in[0] = 0xA0A0A0A0; g_in[1] = cnt; g_in[2] = 0xC0C0C0C0; g_in[3] = (DWORD)g_model;
        g_in[4] = 0; g_in[5] = 0xD1D1D1D1; g_in[6] = (DWORD)g_this;
        g_where = 0;
        run_site();
        /* original game: 78h==0 -> alt; guard: 78h!=0 && flag 2 && count<3 -> alt; else keep */
        want = (m78 == 0 || ((fl & 2) && cnt < 3)) ? 2 : 1;
        ok = g_where == (DWORD)want && g_land_esp == g_saved_esp;
        for (ci = 0; ci < 7; ci++) ok = ok && g_out[ci] == g_in[ci];
        if (!ok) { fails++; printf("FAIL m78=%d flags=%#x count=%d where=%lu want=%d esp_ok=%d\n", m78, fl, cnt,
                                   g_where, want, g_land_esp == g_saved_esp); }
    }
    /* the three console captures: model+78h=1, flags 0x47, count 1 */
    g_model[0x78] = 1; *(WORD *)(g_this + 0x58) = 0x47; g_in[1] = 1; g_in[3] = (DWORD)g_model; g_in[6] = (DWORD)g_this;
    run_site();
    printf("%s captures 1-3 state (78h=1, flags 47h, count 1) -> %s\n", g_where == 2 ? "PASS" : "FAIL",
           g_where == 2 ? "011E0F35 (no index-2 texture)" : "011E0ECC (index 2: crash path)");
    if (g_where != 2) fails++;
    printf("matrix: 120 cases, %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
