# Zorin DLL Proxy Injector & Generator v1.0

![Windows](https://img.shields.io/badge/Platform-Windows-blue?logo=windows)
![C++17](https://img.shields.io/badge/Language-C%2B%2B17%20%2F%20C-00599C?logo=c%2B%2B)
![Python](https://img.shields.io/badge/Python-3.8+-yellow?logo=python)
![License](https://img.shields.io/badge/License-MIT-green)

A professional, high-performance tool for **DLL Proxying / DLL Hijacking with Export Forwarding** and custom payload execution, now rewritten in **Native C++ (C++17)** with a heavily upgraded Python edition.

> **WARNING:** This project is strictly intended for educational purposes, authorized red teaming, and cybersecurity research. Always obtain explicit authorization before testing.

---

## ⚡ What is Zorin?

Zorin creates an authentic proxy DLL that seamlessly replaces a target dynamic link library:
1. **Full Export Forwarding**: Resolves and forwards all original exported functions (by name, ordinal, or pre-existing forwarder) directly through the Windows PE Loader.
2. **Anti-Loader-Lock Architecture**: Executes custom user payloads in an isolated worker thread spawned outside the fragile `DllMain` loader lock window.
3. **Preserves Original Application Stability**: Host applications continue running with 100% functionality and stability while your payload executes.

---

## 🚀 Key Improvements in v1.0

- **Native C++ Standalone Binary (`zorin.exe`)**:
  - No Python runtime required on target machines.
  - **Zero-Dependency Native PE Parser**: Directly inspects PE structures (`IMAGE_DOS_HEADER`, `IMAGE_NT_HEADERS32/64`, `IMAGE_EXPORT_DIRECTORY`) without needing Microsoft `dumpbin.exe` or Visual Studio developer tools.
- **Automatic Architecture Detection**:
  - Automatically identifies target PE architecture (`x86` vs `x64` vs `ARM64`) and configures appropriate compilation flags (`-m32` / `-m64`).
- **Compiler Auto-Discovery**:
  - Automatically detects installed toolchains (`g++`, `gcc`, `clang++`, `cl.exe`).
- **Comprehensive CLI Suite**:
  - `--list-exports` / `-l`: Audit and inspect exported functions, RVAs, and forwarders for any Windows DLL.
  - `--restore` / `-r`: Safely revert changes and restore original DLLs with a single command.
  - `--generate-only` / `-g`: Generate proxy `.cpp`, `.def`, and `build.bat` files for manual review without modifying files.
  - `--compile-only` / `-c` & `-o`: Output the compiled proxy DLL to a custom location without replacing the target.
- **Pure Python PE Parser in `zorin.py`**:
  - The Python edition now uses Python's built-in `struct` module to parse PE files, removing the previous external dependency on `dumpbin.exe`.
  - Fixed Windows terminal CP1252 / UTF-8 encoding crashes.

---

## 🛠️ Building `zorin.exe`

You can build the native C++ executable using `g++` (MinGW / MSYS2) or `CMake`:

### Quick Build (Windows Batch)
```cmd
build.bat
```

### Direct GCC Command
```bash
g++ -std=c++17 -O2 -s src/pe_parser.cpp src/generator.cpp src/compiler.cpp src/main.cpp -o zorin.exe -static-libgcc -static-libstdc++
```

### Via CMake
```bash
cmake -B build
cmake --build build --config Release
```

---

## 📖 Usage Guide

```text
USAGE:
  zorin.exe <target_dll> <payload.c|cpp> [options]
  zorin.exe --list-exports <target_dll>
  zorin.exe --restore <target_dll>

OPTIONS:
  -o, --output <path>        Set custom output proxy DLL path (does not modify target)
  -b, --backup-suffix <sfx>  Backup suffix for original DLL (default: '_org')
  -g, --generate-only        Only generate source (.cpp), .def, and build script
  -c, --compile-only         Compile proxy DLL without replacing target
  -l, --list-exports         List all exports, ordinals, and architecture of a DLL
  -r, --restore              Restore original DLL from backup (<name><suffix>.dll)
  -d, --delay <ms>           Payload execution delay in milliseconds (default: 0)
  -f, --force                Overwrite existing backup or output files without prompting
  --dry-run                  Simulate actions and display analysis without writing
  -h, --help                 Display help menu
```

### 1. Inspect Exports of a DLL
```bash
.\zorin.exe -l "C:\Program Files\Nmap\libssh2.dll"
# Or using Python:
python zorin.py -l "C:\Program Files\Nmap\libssh2.dll"
```

### 2. Inject Payload and Proxy Target DLL
```bash
# Injects payload and automatically backs up original to libssh2_org.dll
.\zorin.exe "C:\Program Files\Nmap\libssh2.dll" payload.c
```

### 3. Generate Code Only (Inspect Before Building)
```bash
.\zorin.exe target.dll payload.c -g
```

### 4. Restore the Original DLL
```bash
.\zorin.exe --restore "C:\Program Files\Nmap\libssh2.dll"
# Or using Python:
python zorin.py --restore "C:\Program Files\Nmap\libssh2.dll"
```

---

## 📦 Payload Examples

Check the [examples/](examples/) directory for ready-to-use payloads:

- [examples/payload_msgbox.c](examples/payload_msgbox.c): Classic Windows MessageBox demo.
- [examples/payload_console.cpp](examples/payload_console.cpp): Allocates an interactive debugging console.
- [examples/payload_logger.c](examples/payload_logger.c): Logs host process activity and timestamp to `%TEMP%`.
- [examples/test_app.c](examples/test_app.c) & [examples/test_lib.c](examples/test_lib.c): Automated verification test suite.

---

## 📂 Project Structure

```text
├── src/
│   ├── pe_parser.hpp / .cpp     # Native PE32/PE32+ export reader
│   ├── generator.hpp / .cpp     # Proxy C++ source and DEF export forwarder generator
│   ├── compiler.hpp / .cpp      # Toolchain auto-discovery (g++, gcc, clang, cl)
│   └── main.cpp                 # Command-line interface with ANSI colors
├── examples/                    # Sample payloads and verification test harness
├── templates/                   # Clean code templates
├── zorin.py                     # Enhanced Python script with pure PE parser
├── build.bat                    # Quick build script for Windows
├── CMakeLists.txt               # Modern CMake configuration
├── PULL_REQUEST.md              # Detailed pull request summary
└── README.md                    # Project documentation
```

---

## ⚖️ Legal Disclaimer

**For authorized educational and security testing purposes only.** Always obtain written permission prior to testing on systems or applications you do not own.

## 📄 License

Distributed under the MIT License. See `LICENSE` for details.