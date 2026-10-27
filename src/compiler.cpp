#include "compiler.hpp"

#include <windows.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <array>

namespace zorin {

static bool execute_command(const std::string& cmd_line, std::string& out_output, int& out_exit_code) {
    out_output.clear();
    out_exit_code = -1;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hStdOutRead = NULL;
    HANDLE hStdOutWrite = NULL;

    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0)) {
        return false;
    }
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = hStdOutWrite;
    si.hStdOutput = hStdOutWrite;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::vector<char> cmd_buf(cmd_line.begin(), cmd_line.end());
    cmd_buf.push_back('\0');

    if (!CreateProcessA(
        NULL,
        cmd_buf.data(),
        NULL,
        NULL,
        TRUE,
        0,
        NULL,
        NULL,
        &si,
        &pi
    )) {
        CloseHandle(hStdOutRead);
        CloseHandle(hStdOutWrite);
        return false;
    }

    CloseHandle(hStdOutWrite);

    std::array<char, 512> buffer;
    DWORD bytes_read = 0;
    while (ReadFile(hStdOutRead, buffer.data(), static_cast<DWORD>(buffer.size() - 1), &bytes_read, NULL) && bytes_read != 0) {
        buffer[bytes_read] = '\0';
        out_output.append(buffer.data(), bytes_read);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    out_exit_code = static_cast<int>(exit_code);

    CloseHandle(hStdOutRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return true;
}

CompilerInfo Compiler::detect_compiler() {
    CompilerInfo info;

    std::string out;
    int code = -1;
    if (execute_command("g++ --version", out, code) && code == 0) {
        info.type = CompilerType::GPlusPlus;
        info.executable_name = "g++";
        info.version = out.substr(0, out.find('\n'));
        return info;
    }

    if (execute_command("gcc --version", out, code) && code == 0) {
        info.type = CompilerType::GCC;
        info.executable_name = "gcc";
        info.version = out.substr(0, out.find('\n'));
        return info;
    }

    if (execute_command("clang++ --version", out, code) && code == 0) {
        info.type = CompilerType::Clang;
        info.executable_name = "clang++";
        info.version = out.substr(0, out.find('\n'));
        return info;
    }

    if (execute_command("cl.exe", out, code) && (code == 0 || code == 2)) {
        info.type = CompilerType::MSVC;
        info.executable_name = "cl.exe";
        info.version = out.substr(0, out.find('\n'));
        return info;
    }

    return info;
}

bool Compiler::compile_proxy_dll(
    const CompilerInfo& compiler,
    const std::filesystem::path& proxy_cpp,
    const std::filesystem::path& def_file,
    const std::filesystem::path& output_dll,
    Architecture target_arch,
    std::string& out_log
) {
    if (compiler.type == CompilerType::None) {
        out_log = "No supported C/C++ compiler found in PATH (tested g++, gcc, clang++, cl.exe).";
        return false;
    }

    std::ostringstream cmd;

    if (compiler.type == CompilerType::GPlusPlus || compiler.type == CompilerType::GCC || compiler.type == CompilerType::Clang) {
        cmd << compiler.executable_name << " -shared -O2 -s ";
        
        if (target_arch == Architecture::x86) {
            cmd << "-m32 ";
        } else if (target_arch == Architecture::x64) {
            cmd << "-m64 ";
        }

        cmd << "-o \"" << output_dll.string() << "\" ";
        cmd << "\"" << proxy_cpp.string() << "\" ";
        
        if (std::filesystem::exists(def_file)) {
            cmd << "\"" << def_file.string() << "\" ";
        }

        cmd << "-Wl,--enable-stdcall-fixup -static-libgcc -static-libstdc++ -luser32 -lkernel32";
    } else if (compiler.type == CompilerType::MSVC) {
        cmd << "cl.exe /LD /O2 /EHsc ";
        cmd << "\"" << proxy_cpp.string() << "\" ";
        if (std::filesystem::exists(def_file)) {
            cmd << "/DEF:\"" << def_file.string() << "\" ";
        }
        cmd << "/Fe:\"" << output_dll.string() << "\" user32.lib kernel32.lib";
    }

    int exit_code = -1;
    bool success = execute_command(cmd.str(), out_log, exit_code);

    if (!success || exit_code != 0) {
        out_log = "Compilation command failed (exit code " + std::to_string(exit_code) + "):\n" + 
                  "Command: " + cmd.str() + "\n" +
                  "Output:\n" + out_log;
        return false;
    }

    return true;
}

} // namespace zorin
