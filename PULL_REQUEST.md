# Pull Request: Port to Native C++17, Zero-Dependency PE Parser, Anti-Loader-Lock & CLI Overhaul

## 📝 Description

This PR brings a major architectural upgrade to **Zorin**, porting the core functionality to **native C++ (C++17)** into a standalone binary (`zorin.exe`) while preserving and heavily improving the existing Python script (`zorin.py`).

Prior to this PR, Zorin relied on Visual Studio's `dumpbin.exe` to inspect DLL exports, which caused the script to fail on most environments where only MinGW/GCC was installed. Additionally, brittle string replacements in payload templates could cause syntax errors, and there was no automatic architecture detection or safe rollback command.

---

## 🚀 Key Improvements & What's New

### 1. Standalone Native C++ Implementation (`zorin.exe`)
- **Native PE Parser (`src/pe_parser.cpp`)**: 
  - Directly parses PE headers (`IMAGE_DOS_HEADER`, `IMAGE_NT_HEADERS32/64`, `IMAGE_EXPORT_DIRECTORY`, and section headers) in low-level memory.
  - **Zero external dependencies**: Works 100% standalone on any Windows system without `dumpbin.exe`, MSVC SDKs, or Python runtimes.
- **Auto Architecture Detection**:
  - Automatically identifies target PE architecture (`x86` vs `x64` vs `ARM64`) and sets the corresponding compiler flags (`-m32` / `-m64`).
- **Compiler Auto-Detection (`src/compiler.cpp`)**:
  - Dynamically discovers available compilers (`g++`, `gcc`, `clang++`, `cl.exe`) and compiles the proxy DLL directly.

### 2. Anti-Loader-Lock Thread Architecture (`src/generator.cpp`)
- Replaces naive payload injection inside `DllMain` with a detached worker thread (`CreateThread`), preventing deadlocks and host process crashes during DLL loading.
- Wraps payload execution with structured exception handling to protect the host process.
- Eliminates brittle string search-and-replace by cleanly embedding payloads with `#line` pragmas for accurate compiler diagnostics.

### 3. Expanded CLI Features
- `-l, --list-exports <dll>`: Inspect any DLL on disk, listing exported functions, RVAs, ordinals, and forwarders.
- `-r, --restore <dll>`: Safely rollback and restore the original DLL from its backup in a single command.
- `-g, --generate-only`: Generates `.cpp`, `.def`, and `build.bat` files for manual inspection without compiling or swapping.
- `-c, --compile-only` / `-o, --output <path>`: Compiles the proxy DLL to a custom location without touching the target DLL.
- `-f, --force`: Non-interactive mode that bypasses confirmation prompts.

### 4. Overhauled Python Version (`zorin.py`)
- Added a pure-Python PE parser using the standard `struct` module, removing the hard dependency on `dumpbin.exe`.
- Fixed Windows terminal character encoding issues (UTF-8 / CP1252 `UnicodeEncodeError`).
- Added support for `--list-exports` and `--restore` matching the C++ tool.

### 5. Build Automation & Test Suite
- Added `build.bat` and `CMakeLists.txt` for easy 1-click builds.
- Added [`examples/`](file:///examples/) with sample payloads (`payload_msgbox.c`, `payload_console.cpp`, `payload_logger.c`) and a local verification suite (`test_lib.c` and `test_app.c`).

---

## 🧪 Verification & Testing

The new C++ engine and the enhanced Python script were validated end-to-end:
1. **Compilation**: Built cleanly with `g++` 15.2.0 (MSYS2 UCRT64) and CMake with zero warnings.
2. **PE Parsing**: Successfully inspected System DLLs (e.g. `version.dll`, `kernel32.dll`) correctly identifying 64-bit architecture, ordinals, and existing forwarders.
3. **End-to-End Proxying**:
   - Created `test_lib.dll` with exported functions `add()` and `get_message()`.
   - Injected payload using `zorin.exe test_lib.dll test_payload.c`.
   - Executed `test_app.exe`: verified payload execution in background thread while all forwarded function calls resolved and returned expected values.
4. **Restoration**: Verified `--restore` correctly restored original backup DLLs.

---

## 📦 Changed & Added Files

- `src/` - Complete C++ source tree (`pe_parser`, `generator`, `compiler`, `main`).
- `zorin.py` - Updated with pure Python PE parser and argument parser.
- `build.bat` / `CMakeLists.txt` - Build automation.
- `examples/` - Ready-to-use payloads and validation tests.
- `readme.md` - Updated documentation.
