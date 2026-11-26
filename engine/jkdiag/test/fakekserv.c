/* stand-in "kserv.dll" so the kserv-image.bin path is exercised */
#include <windows.h>
__declspec(dllexport) int fake_kserv_marker(void) { return 0x4B534552; }
BOOL WINAPI DllMain(HINSTANCE i, DWORD r, LPVOID p) { (void)i; (void)r; (void)p; return TRUE; }
