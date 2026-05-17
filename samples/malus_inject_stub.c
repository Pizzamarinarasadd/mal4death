/* Stub DLL: malus_inject.dll
   This shows up in malus_sample.exe's import table.
   Phase 1b key: player finds "malus_inject.dll" in IDA Pro imports. */
#include <windows.h>

__declspec(dllexport) void inject_payload(void) {
    /* stub — does nothing */
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL;
    (void)fdwReason;
    (void)lpvReserved;
    return TRUE;
}
