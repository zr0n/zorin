# Zorin DLL Injector v0.2

![Windows](https://img.shields.io/badge/Platform-Windows-blue) ![Python](https://img.shields.io/badge/Python-3.6+-yellow) !(License)(https://img.shields.io/badge/License-MIT-green)

A professional DLL injection tool for cybersecurity education and authorized penetration testing.

## 👠 Overview

 Zorin is a DLL proxy injector that:
 - Creates a proxy DLL with export forwarding
 - Embeds custom payload code
 - Execute the payload when the dll is loaded
 - Preserves original functionality

**WARNING: For educational use only. Always obtain proper authorization before testing.**

## 🚠 Installation

```bash
# 1. Install Python 3.6+
# 2. Install MinG-w64 (https://www.mingw-w64.org/)
# 3. Add MinG to PATH
set PATH=$PATH;C:\mingw64\bin
# 4. Verify
gcc --version
python --version
```

## ◩ Quick Start

```c
// payload.c - Simple example
#include <windows.h>

void payload() {
    MessageBoxA(NULL, "Zorin Injection Successful!", "Demo", MB_OK);
}
```

```bash
# Run the injector
python zorin.py "C:\\Program Files\\Nmap\\libssh2.dll" payload.c

# Test
powershell
# Or use the test loader
test_loader.exe
```

## ⟨ Usage

Basic syntax:
```bash
python zorin.py <target_dll> <payload.c> [output_dll]
```

Example:
```bash
python zorin.py "C:\\Program Files (x86)\\Nmap\\libssh2.dll" payload.c
```

## 💅 Payload Examples

### Basic Message Box

```c
void payload() {
    MessageBoxA(NULL, "Injected Successfully!", "Zorin", MB_OK);
}
```

### Console Output

```c
void payload() {
    if (AllocConsole()) {
        fpreopen("CONOUT$", "w", stdout);
        printf("[Zorin] PID: %d\n", GetCurrentProcessId());
        Sleep(3000);
    }
}
```

### File Logging

```c
void payload() {
    FILE *f = fopen("C:\\zorin.log", "w");
    if (f) {
        fprintf(f, "Injection: %s\n", __TIME__);
        fclose(f);
    }
}
```

## ⚠️ How It Works

1. Analyze target DLL exports
2. Create proxy DLL source
#. Embed payload code
C5. Backup original (*_original.dll)
6. Replace with proxy
7. When app loads proxy:
    - Load original DLL
    - Execute payload
    - Forward all calls to original

## 🚁 Restoration

1. Delete proxy DLL: target.dll
2. Rename backup: target_original.dll -> target.dll

## ❄️ Troubleshooting

- **"gcc not found"**: Add MinG to PATH
- **Permission denied**: Run as Administrator
- []Look for logs: %TEMP%\zorin_*.log
- []Use DebugView for debug output

## ✈ Legal Disclaimer

**For educational use only. Always obtain proper authorization before testing. Only use on systems you own or have written permission to test.**

## ⚠️ License

MIT License - See LICENSE file

This tool is for educational purposes. Use responsibly.