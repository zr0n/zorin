#include <windows.h>
#include <stdio.h>

typedef int (*AddFunc)(int, int);
typedef const char* (*GetMsgFunc)(void);

int main() {
    printf("[*] Loading test_lib.dll...\n");
    HMODULE hLib = LoadLibraryA("test_lib.dll");
    if (!hLib) {
        printf("[✗] LoadLibrary failed, error: %lu\n", GetLastError());
        return 1;
    }

    AddFunc pAdd = (AddFunc)GetProcAddress(hLib, "add");
    GetMsgFunc pGetMsg = (GetMsgFunc)GetProcAddress(hLib, "get_message");

    if (!pAdd || !pGetMsg) {
        printf("[✗] GetProcAddress failed, error: %lu\n", GetLastError());
        FreeLibrary(hLib);
        return 1;
    }

    int res = pAdd(40, 2);
    const char* msg = pGetMsg();

    printf("[✓] Called add(40, 2) = %d\n", res);
    printf("[✓] Called get_message() = \"%s\"\n", msg);

    // Give background payload thread a moment to finish logging
    Sleep(500);

    FreeLibrary(hLib);
    printf("[✓] Test app finished successfully.\n");
    return 0;
}
