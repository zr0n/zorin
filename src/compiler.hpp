#pragma once

#include "pe_parser.hpp"
#include <filesystem>
#include <string>

namespace zorin {

enum class CompilerType {
    None,
    GCC,
    GPlusPlus,
    Clang,
    MSVC
};

struct CompilerInfo {
    CompilerType type = CompilerType::None;
    std::string executable_name;
    std::string path;
    std::string version;
};

class Compiler {
public:
    static CompilerInfo detect_compiler();
    
    static bool compile_proxy_dll(
        const CompilerInfo& compiler,
        const std::filesystem::path& proxy_cpp,
        const std::filesystem::path& def_file,
        const std::filesystem::path& output_dll,
        Architecture target_arch,
        std::string& out_log
    );
};

} // namespace zorin
