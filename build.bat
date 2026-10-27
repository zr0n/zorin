@echo off
setlocal
echo ============================================================
echo   Building Zorin (C++ Edition)
echo ============================================================

where g++ >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [✗] Error: g++ (MinGW) not found in PATH.
    echo     Please install MinGW / MSYS2 or add g++ to PATH.
    pause
    exit /b 1
)

echo [*] Compiling zorin.exe with g++ (C++17)...
g++ -std=c++17 -O2 -s src\pe_parser.cpp src\generator.cpp src\compiler.cpp src\main.cpp -o zorin.exe -static-libgcc -static-libstdc++

if %ERRORLEVEL% EQU 0 (
    echo [✓] Build successful! Binary generated: zorin.exe
    zorin.exe --help
) else (
    echo [✗] Build failed!
)
