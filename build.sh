#!/bin/bash
set -e

echo "=================================================="
echo "    Topaz Photo AI 补丁工程自动化构建流水线"
echo "=================================================="

# 检查编译器工具链
CC_X64="x86_64-w64-mingw32-gcc"
WINDRES_X64="x86_64-w64-mingw32-windres"

if ! command -v $CC_X64 &> /dev/null; then
    echo "[-] 未找到 64 位 MinGW-w64 编译器: $CC_X64"
    echo "[*] 请执行: apt install -y gcc-mingw-w64-x86-64 binutils-mingw-w64-x86-64"
    exit 1
fi

if ! command -v $WINDRES_X64 &> /dev/null; then
    echo "[-] 未找到 64 位 windres: $WINDRES_X64"
    exit 1
fi

if ! command -v python3 &> /dev/null; then
    echo "[-] 未找到 python3"
    exit 1
fi

echo "[1/5] 编译 64 位核心修补模块: dup2patcher.dll..."
$CC_X64 -O2 -s -shared -o dup2patcher.dll dup2patcher.c dup2patcher.def -lcomctl32 -luser32 -lgdi32

echo "[2/5] 加密 payload 生成 encrypted_payload.bin..."
python3 encrypt.py dup2patcher.dll encrypted_payload.bin

echo "[3/5] 编译 64 位 Windows 资源文件 (图标、清单与内嵌 Payload)..."
$WINDRES_X64 -O coff resource.rc -o resource.res

echo "[4/5] 构建模块化加载器: Use_Loader.exe..."
$CC_X64 -O2 -s -mwindows -o Use_Loader.exe loader.c resource.res

echo "[5/5] 构建独立版纯净程序: Use.exe (推荐生产使用，零 Dropper 拦截)..."
$CC_X64 -O2 -s -DSTANDALONE_EXE -mwindows -o Use.exe dup2patcher.c resource.res -lcomctl32 -luser32 -lgdi32

echo "=================================================="
echo "[+] 全部构建成功！"
echo "  - Use.exe         : 独立完整单文件 GUI 补丁程序 (推荐)"
echo "  - Use_Loader.exe  : 资源区加密 Payload 模块化加载器"
echo "  - dup2patcher.dll : 核心补丁动态库 (导出 load_patcher)"
echo "=================================================="
