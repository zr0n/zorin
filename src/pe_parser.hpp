#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>

namespace zorin {

enum class Architecture {
    Unknown,
    x86,
    x64,
    ARM64
};

struct ExportSymbol {
    std::string name;           // Export name (empty if exported by ordinal only)
    uint16_t ordinal = 0;       // Export ordinal
    uint32_t rva = 0;           // Relative Virtual Address
    bool is_forwarded = false;  // True if already forwarded in original DLL
    std::string forwarder_str;  // Forwarder string (e.g. "NTDLL.RtlAllocateHeap")
};

struct PeInfo {
    std::filesystem::path file_path;
    Architecture arch = Architecture::Unknown;
    std::string internal_name;
    uint32_t ordinal_base = 1;
    uint32_t total_functions = 0;
    uint32_t total_names = 0;
    std::vector<ExportSymbol> exports;

    [[nodiscard]] std::string get_arch_string() const {
        switch (arch) {
            case Architecture::x86:   return "x86 (32-bit)";
            case Architecture::x64:   return "x64 (64-bit)";
            case Architecture::ARM64: return "ARM64";
            default:                  return "Unknown";
        }
    }
};

class PeParser {
public:
    static bool parse(const std::filesystem::path& path, PeInfo& out_info, std::string& out_error);

private:
    static uint32_t rva_to_file_offset(uint32_t rva, const std::vector<uint8_t>& file_data);
};

} // namespace zorin
