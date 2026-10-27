#pragma once

#include "pe_parser.hpp"
#include <filesystem>
#include <string>

namespace zorin {

struct GeneratorOptions {
    std::filesystem::path target_dll;
    std::filesystem::path payload_file;
    std::filesystem::path output_dir;
    std::string backup_suffix = "_org"; // e.g. target_org.dll
    bool enable_exception_handling = true;
    bool delayed_execution = false;
    uint32_t delay_ms = 100;
};

struct GeneratedFiles {
    std::filesystem::path proxy_source;
    std::filesystem::path def_file;
    std::filesystem::path build_script;
};

class Generator {
public:
    static bool generate(const PeInfo& pe_info, 
                         const GeneratorOptions& options, 
                         GeneratedFiles& out_files, 
                         std::string& out_error);

private:
    static std::string generate_def_content(const PeInfo& pe_info, const std::string& original_base_name);
    static std::string generate_proxy_code(const GeneratorOptions& options);
    static std::string generate_build_script(const GeneratorOptions& options, const GeneratedFiles& files);
};

} // namespace zorin
