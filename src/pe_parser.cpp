#include "pe_parser.hpp"

#include <windows.h>
#include <fstream>
#include <iostream>
#include <algorithm>

namespace zorin {

uint32_t PeParser::rva_to_file_offset(uint32_t rva, const std::vector<uint8_t>& file_data) {
    if (file_data.size() < sizeof(IMAGE_DOS_HEADER)) return 0;
    
    const auto* dos_header = reinterpret_cast<const IMAGE_DOS_HEADER*>(file_data.data());
    if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) return 0;

    const size_t nt_offset = dos_header->e_lfanew;
    if (nt_offset + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) > file_data.size()) return 0;

    const auto* file_header = reinterpret_cast<const IMAGE_FILE_HEADER*>(file_data.data() + nt_offset + sizeof(DWORD));
    const WORD num_sections = file_header->NumberOfSections;
    const WORD opt_header_size = file_header->SizeOfOptionalHeader;

    const size_t section_headers_offset = nt_offset + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + opt_header_size;
    if (section_headers_offset + (num_sections * sizeof(IMAGE_SECTION_HEADER)) > file_data.size()) return 0;

    const auto* sections = reinterpret_cast<const IMAGE_SECTION_HEADER*>(file_data.data() + section_headers_offset);

    for (WORD i = 0; i < num_sections; ++i) {
        const auto& sec = sections[i];
        uint32_t sec_size = sec.Misc.VirtualSize;
        if (sec_size == 0) sec_size = sec.SizeOfRawData;

        if (rva >= sec.VirtualAddress && rva < sec.VirtualAddress + sec_size) {
            return (rva - sec.VirtualAddress) + sec.PointerToRawData;
        }
    }

    return 0;
}

bool PeParser::parse(const std::filesystem::path& path, PeInfo& out_info, std::string& out_error) {
    out_info = PeInfo();
    out_info.file_path = path;

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        out_error = "Failed to open DLL file: " + path.string();
        return false;
    }

    const std::streamsize file_size = file.tellg();
    if (file_size < static_cast<std::streamsize>(sizeof(IMAGE_DOS_HEADER) + sizeof(IMAGE_NT_HEADERS32))) {
        out_error = "File is too small to be a valid PE file";
        return false;
    }

    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(file_size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), file_size)) {
        out_error = "Failed to read DLL file contents";
        return false;
    }

    // 1. Verify DOS Header
    const auto* dos_header = reinterpret_cast<const IMAGE_DOS_HEADER*>(buffer.data());
    if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
        out_error = "Invalid DOS signature (not 'MZ')";
        return false;
    }

    // 2. Locate NT Headers
    const size_t nt_offset = dos_header->e_lfanew;
    if (nt_offset + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) > buffer.size()) {
        out_error = "Invalid e_lfanew offset in DOS header";
        return false;
    }

    const DWORD* pe_sig = reinterpret_cast<const DWORD*>(buffer.data() + nt_offset);
    if (*pe_sig != IMAGE_NT_SIGNATURE) {
        out_error = "Invalid PE signature (not 'PE\\0\\0')";
        return false;
    }

    const auto* file_header = reinterpret_cast<const IMAGE_FILE_HEADER*>(buffer.data() + nt_offset + sizeof(DWORD));

    // Architecture identification
    switch (file_header->Machine) {
        case IMAGE_FILE_MACHINE_I386:
            out_info.arch = Architecture::x86;
            break;
        case IMAGE_FILE_MACHINE_AMD64:
            out_info.arch = Architecture::x64;
            break;
        case IMAGE_FILE_MACHINE_ARM64:
            out_info.arch = Architecture::ARM64;
            break;
        default:
            out_info.arch = Architecture::Unknown;
            break;
    }

    uint32_t export_dir_rva = 0;
    uint32_t export_dir_size = 0;

    const size_t opt_header_offset = nt_offset + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER);
    const WORD opt_magic = *reinterpret_cast<const WORD*>(buffer.data() + opt_header_offset);

    if (opt_magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        if (opt_header_offset + sizeof(IMAGE_OPTIONAL_HEADER32) > buffer.size()) {
            out_error = "Truncated 32-bit Optional Header";
            return false;
        }
        const auto* opt32 = reinterpret_cast<const IMAGE_OPTIONAL_HEADER32*>(buffer.data() + opt_header_offset);
        if (opt32->NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_EXPORT) {
            export_dir_rva = opt32->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
            export_dir_size = opt32->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
        }
    } else if (opt_magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        if (opt_header_offset + sizeof(IMAGE_OPTIONAL_HEADER64) > buffer.size()) {
            out_error = "Truncated 64-bit Optional Header";
            return false;
        }
        const auto* opt64 = reinterpret_cast<const IMAGE_OPTIONAL_HEADER64*>(buffer.data() + opt_header_offset);
        if (opt64->NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_EXPORT) {
            export_dir_rva = opt64->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
            export_dir_size = opt64->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
        }
    } else {
        out_error = "Unsupported Optional Header magic: " + std::to_string(opt_magic);
        return false;
    }

    if (export_dir_rva == 0 || export_dir_size == 0) {
        // No export directory (DLL has no exported functions)
        return true;
    }

    const uint32_t export_offset = rva_to_file_offset(export_dir_rva, buffer);
    if (export_offset == 0 || export_offset + sizeof(IMAGE_EXPORT_DIRECTORY) > buffer.size()) {
        out_error = "Export directory RVA points to an invalid file offset";
        return false;
    }

    const auto* exp_dir = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(buffer.data() + export_offset);
    out_info.ordinal_base = exp_dir->Base;
    out_info.total_functions = exp_dir->NumberOfFunctions;
    out_info.total_names = exp_dir->NumberOfNames;

    // Read DLL internal name if present
    if (exp_dir->Name != 0) {
        uint32_t name_offset = rva_to_file_offset(exp_dir->Name, buffer);
        if (name_offset != 0 && name_offset < buffer.size()) {
            out_info.internal_name = reinterpret_cast<const char*>(buffer.data() + name_offset);
        }
    }

    const uint32_t func_table_offset = rva_to_file_offset(exp_dir->AddressOfFunctions, buffer);
    const uint32_t name_table_offset = rva_to_file_offset(exp_dir->AddressOfNames, buffer);
    const uint32_t ord_table_offset = rva_to_file_offset(exp_dir->AddressOfNameOrdinals, buffer);

    if (func_table_offset == 0) {
        out_error = "Invalid AddressOfFunctions offset";
        return false;
    }

    const auto* func_rvas = reinterpret_cast<const DWORD*>(buffer.data() + func_table_offset);
    const auto* name_rvas = (name_table_offset != 0) ? reinterpret_cast<const DWORD*>(buffer.data() + name_table_offset) : nullptr;
    const auto* ordinals = (ord_table_offset != 0) ? reinterpret_cast<const WORD*>(buffer.data() + ord_table_offset) : nullptr;

    // Map ordinal index to name
    std::vector<std::string> names_by_ord_idx(exp_dir->NumberOfFunctions);
    if (name_rvas && ordinals) {
        for (DWORD i = 0; i < exp_dir->NumberOfNames; ++i) {
            WORD ord_idx = ordinals[i];
            if (ord_idx < exp_dir->NumberOfFunctions) {
                uint32_t n_offset = rva_to_file_offset(name_rvas[i], buffer);
                if (n_offset != 0 && n_offset < buffer.size()) {
                    names_by_ord_idx[ord_idx] = reinterpret_cast<const char*>(buffer.data() + n_offset);
                }
            }
        }
    }

    for (DWORD i = 0; i < exp_dir->NumberOfFunctions; ++i) {
        DWORD func_rva = func_rvas[i];
        if (func_rva == 0) {
            // Unused ordinal slot
            continue;
        }

        ExportSymbol sym;
        sym.ordinal = static_cast<uint16_t>(exp_dir->Base + i);
        sym.rva = func_rva;
        sym.name = names_by_ord_idx[i]; // May be empty for ordinal-only exports

        // Check if forwarded: RVA points inside export directory
        if (func_rva >= export_dir_rva && func_rva < export_dir_rva + export_dir_size) {
            sym.is_forwarded = true;
            uint32_t fwd_offset = rva_to_file_offset(func_rva, buffer);
            if (fwd_offset != 0 && fwd_offset < buffer.size()) {
                sym.forwarder_str = reinterpret_cast<const char*>(buffer.data() + fwd_offset);
            }
        }

        out_info.exports.push_back(sym);
    }

    return true;
}

} // namespace zorin
