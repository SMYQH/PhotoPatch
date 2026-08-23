#!/bin/bash
set -e

echo "=================================================="
echo "  Topaz Photo AI 补丁工程自动化构建流水线 (纯 C 语言)"
echo "=================================================="

# 检查编译器工具链
HOST_CC="gcc"
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

echo "[1/3] 编译 64 位核心修补动态库: dup2patcher.dll..."
$CC_X64 -O2 -s -shared -o dup2patcher.dll dup2patcher.c dup2patcher.def -lcomctl32 -luser32 -lgdi32

echo "[2/3] 编译 64 位 Windows 资源文件 (图标与清单)..."
$WINDRES_X64 -O coff resource.rc -o resource.res

echo "[3/3] 构建独立版单文件程序: Photo Patch.exe (生产首选，零 Dropper 误报)..."
$CC_X64 -O2 -s -DSTANDALONE_EXE -mwindows -o "Photo Patch.exe" dup2patcher.c resource.res -lcomctl32 -luser32 -lgdi32

echo "=================================================="
echo "[+] 全部构建成功！"
echo "  - Photo Patch.exe : 独立完整单文件 GUI 补丁与全量汉化程序"
echo "  - dup2patcher.dll : 核心补丁动态库 (导出 load_patcher)"
echo "=================================================="
