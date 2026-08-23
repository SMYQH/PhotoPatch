#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

/**
 * loader.c
 * 
 * Topaz Photo AI 资源区加密 DLL 解密加载器 (x64 / x86 通用)
 */

void DecryptPayload(unsigned char* buffer, unsigned int size, unsigned int key) {
    unsigned int ebx = key;
    for (unsigned int ecx = size; ecx > 0; ecx--) {
        unsigned char dl = *buffer;

        // 异或解密当前字节
        *buffer = dl ^ (unsigned char)(ebx & 0xFF);
        buffer++;

        // 32 位循环右移 1 位
        ebx = ((ebx >> 1) | (ebx << 31)) & 0xFFFFFFFF;

        // 异或更新低位密钥
        ebx = (ebx & ~0xFF) | (((ebx & 0xFF) ^ dl) & 0xFF);

        // 密钥累加剩余计数
        ebx = (ebx + ecx) & 0xFFFFFFFF;
    }
}

DWORD WriteToDisk(LPCSTR lpFileName, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite) {
    HANDLE hFile = CreateFileA(
        lpFileName,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (hFile == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD bytesWritten = 0;
    WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, &bytesWritten, NULL);
    FlushFileBuffers(hFile);
    CloseHandle(hFile);
    return bytesWritten;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    HMODULE hModule = GetModuleHandleA(NULL);

    // 1. 定位资源区中的加密 DLL (Type: "DLL", ID: 10)
    HRSRC hResInfo = FindResourceA(hModule, (LPCSTR)10, "DLL");
    if (!hResInfo) {
        MessageBoxA(NULL, "[-] 找不到内嵌补丁资源，程序已损坏！", "加载失败", MB_OK | MB_ICONERROR);
        ExitProcess(1);
    }

    DWORD resSize = SizeofResource(hModule, hResInfo);
    HGLOBAL hResData = LoadResource(hModule, hResInfo);
    if (!hResData || resSize == 0) {
        MessageBoxA(NULL, "[-] 加载内嵌补丁资源失败！", "加载失败", MB_OK | MB_ICONERROR);
        ExitProcess(1);
    }

    // 2. 分配内存并拷贝资源数据
    void* buffer = VirtualAlloc(NULL, resSize, MEM_COMMIT, PAGE_READWRITE);
    if (!buffer) {
        MessageBoxA(NULL, "[-] 内存分配失败！", "加载失败", MB_OK | MB_ICONERROR);
        ExitProcess(1);
    }
    RtlMoveMemory(buffer, hResData, resSize);

    // 3. 执行解密 (固定密钥: 0xDEADBEEF)
    DecryptPayload((unsigned char*)buffer, resSize, 0xDEADBEEF);

    // 4. 获取系统 %TEMP% 路径并拼接具有 PID 隔离的文件名
    char tempPath[MAX_PATH];
    GetTempPathA(sizeof(tempPath), tempPath);
    char fullDllPath[MAX_PATH];
    snprintf(fullDllPath, sizeof(fullDllPath), "%s\\tpai_patcher_%lu.dll", tempPath, GetCurrentProcessId());

    // 5. 写入解密后的 DLL 到临时目录
    if (WriteToDisk(fullDllPath, buffer, resSize) == 0) {
        VirtualFree(buffer, 0, MEM_RELEASE);
        MessageBoxA(NULL, "[-] 无法写入临时修补模块，请检查磁盘写权限！", "加载失败", MB_OK | MB_ICONERROR);
        ExitProcess(1);
    }

    // 擦除内存明文
    SecureZeroMemory(buffer, resSize);
    VirtualFree(buffer, 0, MEM_RELEASE);

    // 6. 动态加载并调用 load_patcher() 导出函数
    HMODULE hDll = LoadLibraryA(fullDllPath);
    if (hDll) {
        typedef void (*LOAD_PATCHER_FN)(void);
        LOAD_PATCHER_FN pfnLoadPatcher = (LOAD_PATCHER_FN)GetProcAddress(hDll, "load_patcher");
        if (pfnLoadPatcher) {
            pfnLoadPatcher();
        } else {
            MessageBoxA(NULL, "[-] 无法定位修补引擎入口函数！", "加载失败", MB_OK | MB_ICONERROR);
        }
        // 7. 运行结束，释放模块并擦除临时文件
        FreeLibrary(hDll);
        DeleteFileA(fullDllPath);
    } else {
        DeleteFileA(fullDllPath);
        MessageBoxA(NULL, "[-] 无法加载修补引擎模块！", "加载失败", MB_OK | MB_ICONERROR);
    }

    ExitProcess(0);
    return 0;
}
