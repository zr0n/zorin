#include "generator.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

namespace zorin {

std::string Generator::generate_def_content(const PeInfo& pe_info, const std::string& original_base_name) {
    std::ostringstream ss;
    
    std::string lib_name = pe_info.internal_name;
    if (lib_name.empty()) {
        lib_name = pe_info.file_path.stem().string();
    }
    
    ss << "; ================================================================\n";
    ss << "; Zorin DEF Export Forwarding File\n";
    ss << "; Target: " << pe_info.file_path.filename().string() << "\n";
    ss << "; Architecture: " << pe_info.get_arch_string() << "\n";
    ss << "; Exports count: " << pe_info.exports.size() << "\n";
    ss << "; ================================================================\n\n";

    ss << "LIBRARY \"" << lib_name << "\"\n";
    ss << "EXPORTS\n";

    for (const auto& sym : pe_info.exports) {
        if (sym.is_forwarded && !sym.forwarder_str.empty()) {
            if (!sym.name.empty()) {
                ss << "    " << sym.name << " = " << sym.forwarder_str << " @" << sym.ordinal << "\n";
            } else {
                ss << "    ord_" << sym.ordinal << " = " << sym.forwarder_str << " @" << sym.ordinal << " NONAME\n";
            }
        } else {
            if (!sym.name.empty()) {
                ss << "    " << sym.name << " = " << original_base_name << "." << sym.name << " @" << sym.ordinal << "\n";
            } else {
                ss << "    ord_" << sym.ordinal << " = " << original_base_name << ".#" << sym.ordinal << " @" << sym.ordinal << " NONAME\n";
            }
        }
    }

    return ss.str();
}

std::string Generator::generate_proxy_code(const GeneratorOptions& options) {
    std::ostringstream ss;
    
    // Read payload file
    std::string payload_content;
    std::ifstream payload_in(options.payload_file);
    if (payload_in.is_open()) {
        std::ostringstream pss;
        pss << payload_in.rdbuf();
        payload_content = pss.str();
    }

    // Determine payload entry function name
    std::string call_target = "payload()";
    if (payload_content.find("user_payload") != std::string::npos && 
        payload_content.find("payload()") == std::string::npos) {
        call_target = "user_payload()";
    } else if (payload_content.find("zorin_payload") != std::string::npos && 
               payload_content.find("payload()") == std::string::npos) {
        call_target = "zorin_payload()";
    }

    ss << "/* ================================================================\n";
    ss << " * Zorin Proxy DLL Source\n";
    ss << " * Generated automatically by Zorin (C++ Edition)\n";
    ss << " * ================================================================ */\n\n";

    ss << "#define WIN32_LEAN_AND_MEAN\n";
    ss << "#include <windows.h>\n";
    ss << "#include <process.h>\n";
    ss << "#include <stdio.h>\n\n";

    ss << "/* ----------------------------------------------------------------\n";
    ss << " * Embedded User Payload\n";
    ss << " * ---------------------------------------------------------------- */\n";
    ss << "#line 1 \"" << options.payload_file.filename().string() << "\"\n";
    ss << payload_content << "\n\n";

    ss << "/* ----------------------------------------------------------------\n";
    ss << " * Zorin Runtime Engine & Anti-Loader-Lock Thread\n";
    ss << " * ---------------------------------------------------------------- */\n";
    ss << "static volatile LONG g_payload_executed = 0;\n";
    ss << "static HMODULE g_hOriginalModule = NULL;\n\n";

    ss << "static DWORD WINAPI ZorinWorkerThread(LPVOID lpParam) {\n";
    ss << "    (void)lpParam;\n\n";

    if (options.delayed_execution && options.delay_ms > 0) {
        ss << "    // Wait for the host process initialization to complete\n";
        ss << "    Sleep(" << options.delay_ms << ");\n\n";
    }

    ss << "    // Execute user payload safely\n";
    ss << "    #ifdef _MSC_VER\n";
    ss << "    __try {\n";
    ss << "    #endif\n";
    ss << "        " << call_target << ";\n";
    ss << "    #ifdef _MSC_VER\n";
    ss << "    } __except (EXCEPTION_EXECUTE_HANDLER) {\n";
    ss << "        OutputDebugStringA(\"[ZORIN] Exception intercepted in payload thread!\\n\");\n";
    ss << "    }\n";
    ss << "    #endif\n\n";

    ss << "    return 0;\n";
    ss << "}\n\n";

    ss << "BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {\n";
    ss << "    (void)lpReserved;\n";
    ss << "    switch (fdwReason) {\n";
    ss << "        case DLL_PROCESS_ATTACH: {\n";
    ss << "            DisableThreadLibraryCalls(hinstDLL);\n\n";
    ss << "            // Ensure payload runs only once\n";
    ss << "            if (InterlockedCompareExchange(&g_payload_executed, 1, 0) == 0) {\n";
    ss << "                HANDLE hThread = CreateThread(NULL, 0, ZorinWorkerThread, NULL, 0, NULL);\n";
    ss << "                if (hThread) {\n";
    ss << "                    CloseHandle(hThread);\n";
    ss << "                }\n";
    ss << "            }\n";
    ss << "            break;\n";
    ss << "        }\n";
    ss << "        case DLL_PROCESS_DETACH: {\n";
    ss << "            if (g_hOriginalModule) {\n";
    ss << "                FreeLibrary(g_hOriginalModule);\n";
    ss << "                g_hOriginalModule = NULL;\n";
    ss << "            }\n";
    ss << "            break;\n";
    ss << "        }\n";
    ss << "    }\n";
    ss << "    return TRUE;\n";
    ss << "}\n";

    return ss.str();
}

std::string Generator::generate_build_script(const GeneratorOptions& options, const GeneratedFiles& files) {
    std::ostringstream ss;
    std::string out_dll = options.target_dll.filename().string();
    
    ss << "@echo off\n";
    ss << "echo [*] Compiling Zorin Proxy DLL: " << out_dll << "\n\n";
    ss << "where g++ >nul 2>nul\n";
    ss << "if %ERRORLEVEL% EQU 0 (\n";
    ss << "    echo [+] Using g++ (MinGW)\n";
    ss << "    g++ -shared -O2 -s -o \"" << out_dll << "\" ^\n";
    ss << "        \"" << files.proxy_source.filename().string() << "\" ^\n";
    ss << "        \"" << files.def_file.filename().string() << "\" ^\n";
    ss << "        -Wl,--enable-stdcall-fixup -static-libgcc -static-libstdc++ -luser32 -lkernel32\n";
    ss << "    if %ERRORLEVEL% EQU 0 (\n";
    ss << "        echo [✓] Build successful: " << out_dll << "\n";
    ss << "        exit /b 0\n";
    ss << "    )\n";
    ss << ")\n\n";
    ss << "echo [!] Failed to compile. Please ensure g++ or gcc is in PATH.\n";
    ss << "exit /b 1\n";

    return ss.str();
}

bool Generator::generate(const PeInfo& pe_info, 
                         const GeneratorOptions& options, 
                         GeneratedFiles& out_files, 
                         std::string& out_error) {
    try {
        std::filesystem::path out_dir = options.output_dir.empty() ? 
            std::filesystem::current_path() : options.output_dir;

        std::filesystem::create_directories(out_dir);

        std::string base_stem = options.target_dll.stem().string();
        std::string org_base_name = base_stem + options.backup_suffix;

        // 1. Generate DEF file
        out_files.def_file = out_dir / (base_stem + "_proxy.def");
        std::string def_content = generate_def_content(pe_info, org_base_name);
        std::ofstream def_out(out_files.def_file);
        if (!def_out.is_open()) {
            out_error = "Failed to create DEF file: " + out_files.def_file.string();
            return false;
        }
        def_out << def_content;
        def_out.close();

        // 2. Generate Proxy C++ source (with embedded payload)
        out_files.proxy_source = out_dir / (base_stem + "_proxy.cpp");
        std::string proxy_content = generate_proxy_code(options);
        std::ofstream proxy_out(out_files.proxy_source);
        if (!proxy_out.is_open()) {
            out_error = "Failed to create proxy source file: " + out_files.proxy_source.string();
            return false;
        }
        proxy_out << proxy_content;
        proxy_out.close();

        // 3. Generate Build script
        out_files.build_script = out_dir / ("build_" + base_stem + ".bat");
        std::string build_content = generate_build_script(options, out_files);
        std::ofstream build_out(out_files.build_script);
        if (build_out.is_open()) {
            build_out << build_content;
            build_out.close();
        }

        return true;
    } catch (const std::exception& e) {
        out_error = std::string("Exception during generation: ") + e.what();
        return false;
    }
}

} // namespace zorin
