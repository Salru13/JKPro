/* RED baseline: a jkdiag.dll that does nothing. test.sh must FAIL with it. */
#include <windows.h>
BOOL WINAPI DllMain(HINSTANCE i, DWORD r, LPVOID p) { (void)i; (void)r; (void)p; return TRUE; }
