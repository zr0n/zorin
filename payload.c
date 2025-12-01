#include <windows.h>
#include <stdio.h>

void payload() {
    // 1. Create log file
    char log_path[MAX_PATH];
    GetTempPathA(MAX_PATH, log_path);
    strcat_s(log_path, MAX_PATH, "zorin_injected.log");
    
    FILE* f;
    fopen_s(&f, log_path, "w");
    if (f) {
        fprintf(f, "Zorin DLL Injection Active\n");
        fprintf(f, "Process: %d\n", GetCurrentProcessId());
        fprintf(f, "Time: %s\n", __TIME__);
        fclose(f);
    }
    
    // 2. Show main injection message box
    MessageBoxA(
        NULL,
        "Zorin DLL Injection Successful!\n\n"
        "This executable has been modified with custom code.\n"
        "All original functionality is preserved.\n\n"
        "This is for educational cybersecurity purposes.",
        "DLL Injection Active",
        MB_OK | MB_ICONINFORMATION | MB_SYSTEMMODAL
    );
    
    // 3. Debug output
    OutputDebugStringA("[ZORIN] Injection payload executed\n");
}