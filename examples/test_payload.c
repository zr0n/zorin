#include <windows.h>
#include <stdio.h>

void payload() {
    FILE* f = fopen("zorin_test_result.txt", "w");
    if (f) {
        fprintf(f, "ZORIN_INJECTION_SUCCESS\n");
        fprintf(f, "PID=%lu\n", GetCurrentProcessId());
        fclose(f);
    }
    OutputDebugStringA("[ZORIN TEST PAYLOAD] Injected successfully!\n");
}
