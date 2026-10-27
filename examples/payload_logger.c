#include <windows.h>
#include <stdio.h>

void payload() {
    char temp_dir[MAX_PATH];
    GetTempPathA(MAX_PATH, temp_dir);

    char log_file[MAX_PATH];
    sprintf_s(log_file, MAX_PATH, "%szorin_activity.log", temp_dir);

    FILE* f = NULL;
    fopen_s(&f, log_file, "a");
    if (f) {
        char proc_name[MAX_PATH];
        GetModuleFileNameA(NULL, proc_name, MAX_PATH);

        fprintf(f, "[ZORIN LOG] Timestamp: %s %s\n", __DATE__, __TIME__);
        fprintf(f, "Process PID: %lu\n", GetCurrentProcessId());
        fprintf(f, "Executable:  %s\n", proc_name);
        fprintf(f, "Status:      DLL Proxy Active & Forwarding\n\n");
        fclose(f);
    }
}
