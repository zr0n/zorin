#include <windows.h>

__declspec(dllexport) int add(int a, int b) {
    return a + b;
}

__declspec(dllexport) const char* get_message(void) {
    return "Hello from original DLL!";
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    return TRUE;
}
