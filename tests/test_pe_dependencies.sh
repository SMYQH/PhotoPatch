#!/usr/bin/env bash
# ==============================================================================
# Regression Test: PE Import Dependency & Standalone Verification
# Prevents regression of missing GCC runtime DLLs (e.g. libssp-0.dll, libgcc, etc.)
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

OBJDUMP="x86_64-w64-mingw32-objdump"
if ! command -v "$OBJDUMP" &>/dev/null; then
    if command -v objdump &>/dev/null; then
        OBJDUMP="objdump"
    else
        echo "[-] Error: objdump or x86_64-w64-mingw32-objdump not found." >&2
        exit 1
    fi
fi

# Prohibited runtime DLL patterns (MinGW/GCC dependencies that must not be dynamically linked)
FORBIDDEN_DLLS=("libssp-0.dll" "libgcc_s_seh-1.dll" "libgcc_s_dw2-1.dll" "libwinpthread-1.dll" "libstdc++-6.dll")

# Allowed standard Windows system DLLs (case-insensitive whitelist)
ALLOWED_DLL_REGEX="^(advapi32|comctl32|gdi32|kernel32|msvcrt|ntdll|ole32|oleaut32|shell32|shlwapi|user32|version|ws2_32)\.dll$"

FAILED=0

check_binary() {
    local target="$1"
    local is_dll="$2"
    local full_path="${ROOT_DIR}/${target}"

    echo "=================================================="
    echo "[*] Testing Binary: ${target}"
    echo "=================================================="

    if [ ! -f "$full_path" ]; then
        echo "[-] FAIL: File not found: ${full_path}" >&2
        FAILED=1
        return
    fi

    # 1. Extract imported DLLs
    local imports
    imports=$("$OBJDUMP" -p "$full_path" | grep "DLL Name:" | awk '{print $3}' || true)

    if [ -z "$imports" ]; then
        echo "[-] FAIL: No imported DLLs found in ${target} (Invalid PE format?)" >&2
        FAILED=1
        return
    fi

    echo "[+] Found Imported DLLs:"
    echo "$imports" | sed 's/^/    - /'

    # 2. Check for prohibited DLLs (specifically libssp-0.dll regression)
    for forbidden in "${FORBIDDEN_DLLS[@]}"; do
        if echo "$imports" | grep -iq "^${forbidden}$"; then
            echo "[-] FAIL: Prohibited dependency detected: ${forbidden} in ${target}!" >&2
            echo "    -> Reason: Target must be statically linked to avoid external MinGW runtime dependencies." >&2
            FAILED=1
        fi
    done

    # 3. Whitelist validation
    while IFS= read -r dll; do
        [ -z "$dll" ] && continue
        local dll_lower
        dll_lower=$(echo "$dll" | tr '[:upper:]' '[:lower:]')
        if [[ ! "$dll_lower" =~ $ALLOWED_DLL_REGEX ]]; then
            echo "[!] WARNING / FAIL: Non-whitelisted DLL dependency: ${dll}" >&2
            FAILED=1
        fi
    done <<< "$imports"

    # 4. If DLL, verify required exported symbols
    if [ "$is_dll" -eq 1 ]; then
        echo "[*] Verifying exported symbols in DLL..."
        local exports
        exports=$("$OBJDUMP" -p "$full_path" | grep -A 20 "\[Ordinal/Name Pointer" || true)
        
        local required_exports=("load_patcher" "SearchAndReplace" "AddMsg")
        for sym in "${required_exports[@]}"; do
            if echo "$exports" | grep -q "$sym"; then
                echo "    [+] Export found: ${sym}"
            else
                echo "[-] FAIL: Missing required export symbol: ${sym}" >&2
                FAILED=1
            fi
        done
    fi

    echo ""
}

echo "Running PE Import Dependency & Standalone Tests..."
echo "Toolchain: $OBJDUMP"
echo ""

check_binary "Photo Patch.exe" 0
check_binary "dup2patcher.dll" 1

if [ "$FAILED" -ne 0 ]; then
    echo "=================================================="
    echo "[-] REGRESSION TEST FAILED!"
    echo "=================================================="
    exit 1
else
    echo "=================================================="
    echo "[+] ALL REGRESSION TESTS PASSED! Clean standalone binaries."
    echo "=================================================="
    exit 0
fi
