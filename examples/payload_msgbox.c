#include <windows.h>

void payload() {
    MessageBoxA(
        NULL,
        "Zorin DLL Proxy Injection Successful!\n\n"
        "All original DLL functions are forwarded and active.\n"
        "This execution is isolated in an anti-loader-lock worker thread.",
        "Zorin v1.0",
        MB_OK | MB_ICONINFORMATION | MB_SYSTEMMODAL
    );
}
