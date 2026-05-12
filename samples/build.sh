#!/usr/bin/env bash
# build.sh — Cross-compile all sample .exe files for the game.
# Run this on Linux with MinGW + UPX installed:
#   sudo pacman -S mingw-w64-gcc upx
# Then:
#   cd /home/geeg/Project/samples && bash build.sh

set -e

CC="x86_64-w64-mingw32-gcc"
ASSETS="../assets"
LEVELS="../src/levels.c"

echo "=== Malware Outbreak — Sample Builder ==="
echo ""

# ── Check dependencies ──────────────────────────────────────
if ! command -v "$CC" &>/dev/null; then
    echo "ERROR: $CC not found. Install with: sudo pacman -S mingw-w64-gcc"
    exit 1
fi

if ! command -v upx &>/dev/null; then
    echo "ERROR: upx not found. Install with: sudo pacman -S upx"
    exit 1
fi

mkdir -p "$ASSETS"

# ── Build dummy.exe (Tutorial 1 — UPX packing) ─────────────
echo "[1/5] Compiling dummy.exe..."
$CC -std=c11 dummy.c -o "$ASSETS/dummy.exe" -lkernel32 -luser32

echo "[2/5] Packing dummy.exe with UPX..."
upx "$ASSETS/dummy.exe"
echo "      Key T1 = UPX  (already set in levels.c)"

# ── Build dummy2.exe (Tutorial 2 — PE headers) ─────────────
echo "[3/5] Compiling dummy2.exe..."
$CC -std=c11 dummy2.c -o "$ASSETS/dummy2.exe" -lkernel32

# Extract TimeDateStamp from PE header
TIMESTAMP=$(python3 - <<'EOF'
import struct, sys
with open("../assets/dummy2.exe", "rb") as f:
    data = f.read()
pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
ts = struct.unpack_from("<I", data, pe_offset + 8)[0]
print(f"{ts:08X}")
EOF
)
echo "      TimeDateStamp = $TIMESTAMP"

# ── Build malus_inject.dll (Phase 1b — import table) ───────
echo "[4/5] Compiling malus_inject.dll..."
$CC -shared malus_inject_stub.c \
    -o "$ASSETS/malus_inject.dll" \
    -Wl,--out-implib,libmalus_inject.a \
    -lkernel32

# ── Build malus_sample.exe (main boss) ─────────────────────
echo "[5/5] Compiling malus_sample.exe..."
$CC -std=c11 malus_sample.c \
    -L. -lmalus_inject \
    -o "$ASSETS/malus_sample.exe" \
    -lkernel32 -lws2_32

echo ""
echo "=== All samples built in $ASSETS/ ==="
echo ""

# ── Auto-update levels.c with known keys ───────────────────
echo "Updating src/levels.c with answer keys..."

python3 - <<PYEOF
import re

keys = {
    "DEV_KEY_T2_PEVIEW": "$TIMESTAMP",
    "DEV_KEY_1A_URL":    "http://malus-command.evil/payload",
    "DEV_KEY_1B_DLL":    "malus_inject.dll",
    "DEV_KEY_2A_IP":     "192.168.13.37",
    "DEV_KEY_2B_KILL":   "MALUS_STOP_99",
}

with open("$LEVELS", "r") as f:
    content = f.read()

for key, value in keys.items():
    pattern = rf'(#define\s+{key}\s+)"[^"]*"'
    replacement = rf'\1"{value}"'
    new_content = re.sub(pattern, replacement, content)
    if new_content != content:
        print(f"  Updated {key} = {value}")
        content = new_content
    else:
        print(f"  WARNING: {key} not found in levels.c")

with open("$LEVELS", "w") as f:
    f.write(content)

print("")
print("levels.c updated. Don't forget to recompile the game with: make")
PYEOF

echo ""
echo "=== Done! ==="
echo ""
echo "Summary of answer keys:"
echo "  T1  (UPX packing)    : UPX"
echo "  T2  (PE timestamp)   : $TIMESTAMP"
echo "  1a  (hidden URL)     : http://malus-command.evil/payload"
echo "  1b  (import DLL)     : malus_inject.dll"
echo "  2a  (C2 IP)          : 192.168.13.37"
echo "  2b  (kill switch)    : MALUS_STOP_99"
echo ""
echo "Next step: cd .. && make"
