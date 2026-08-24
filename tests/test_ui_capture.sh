#!/usr/bin/env bash
# ==============================================================================
# Topaz Photo AI 纯 C 语言自动化黑盒 UI 打开与截图测试执行脚本
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

CC_X64="x86_64-w64-mingw32-gcc"
if ! command -v "$CC_X64" &>/dev/null; then
    echo "[-] 错误: 未找到 64 位交叉编译器 $CC_X64" >&2
    exit 1
fi

TEST_SRC="tests/ui_capture_test.c"
TEST_EXE="tests/ui_capture_test.exe"
EXE_TARGET="Photo Patch.exe"
OUTPUT_BMP="tests/ui_screenshot.bmp"

echo "=================================================="
echo "  Topaz Photo AI 纯 C 语言 UI 自动化测试套件"
echo "=================================================="

cd "$ROOT_DIR"

# 1. 检查目标程序是否存在
if [ ! -f "$EXE_TARGET" ]; then
    echo "[*] 正在构建最新版本程序..."
    ./build.sh
fi

# 2. 编译纯 C 黑盒截图测试工具
echo "[1/2] 正在编译纯 C 黑盒测试工具: ${TEST_EXE}..."
$CC_X64 -Wall -Wextra -O2 -s -static "$TEST_SRC" -o "$TEST_EXE" -lgdi32 -luser32 -lshell32

# 3. 运行黑盒测试
echo "[2/2] 正在执行黑盒测试 (自动唤起程序、高保真像素捕获、保存 BMP 截图并优雅退出)..."
rm -f "$OUTPUT_BMP"
"./${TEST_EXE}" "$EXE_TARGET" "$OUTPUT_BMP" < /dev/null

# 4. 验证截图产物
if [ -f "$OUTPUT_BMP" ] && [ -s "$OUTPUT_BMP" ]; then
    BMP_SIZE=$(stat -c%s "$OUTPUT_BMP" 2>/dev/null || stat -f%z "$OUTPUT_BMP")
    echo "=================================================="
    echo "[+] UI 自动化截图黑盒测试全部通过！"
    echo "    - 截图文件: ${OUTPUT_BMP}"
    echo "    - 图像大小: ${BMP_SIZE} 字节"
    echo "=================================================="
    exit 0
else
    echo "[-] 错误: 未能生成有效的 UI 截图文件！" >&2
    exit 1
fi
