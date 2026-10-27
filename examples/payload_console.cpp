#include <windows.h>
#include <iostream>

void payload() {
    if (AllocConsole()) {
        FILE* fp = nullptr;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONIN$", "r", stdin);

        std::cout << "\n============================================\n";
        std::cout << " [ZORIN] Injected Interactive Console Shell\n";
        std::cout << " PID: " << GetCurrentProcessId() << "\n";
        std::cout << " Target process hijacked successfully!\n";
        std::cout << "============================================\n\n";

        Sleep(2000);
    }
}
