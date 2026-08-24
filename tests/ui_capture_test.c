#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/**
 * ui_capture_test.c
 * 
 * 纯 C 语言实现的自动化黑盒 UI 打开与窗口截图测试工具。
 * 流程：
 *  1. 启动目标程序 (Photo Patch.exe，支持 ShellExecuteEx 提权启动)；
 *  2. 轮询探测窗口句柄 (Dup2PatcherClass)；
 *  3. 等待窗口完成布局与 GDI 绘制；
 *  4. 截取窗口位图并保存为标准 24 位 BMP 图像文件；
 *  5. 向窗口发送 WM_CLOSE 消息优雅退出；
 *  6. 验证退出状态并回收系统资源。
 */

// 保存 HBITMAP 为 24位 BMP 文件
static bool SaveBitmapToFile(HBITMAP hBitmap, HDC hDC, const char* filename) {
    BITMAP bmp;
    if (!GetObject(hBitmap, sizeof(BITMAP), &bmp)) {
        printf("[-] GetObject(hBitmap) 失败 (错误码: %lu)\n", GetLastError());
        return false;
    }

    BITMAPFILEHEADER bfh;
    BITMAPINFOHEADER bih;

    int width = bmp.bmWidth;
    int height = bmp.bmHeight;
    WORD bitCount = 24; // 使用 24位 RGB 保存

    // 行对齐：每行字节数必须是 4 的倍数
    DWORD rowSize = ((width * bitCount + 31) / 32) * 4;
    DWORD imageSize = rowSize * height;

    memset(&bih, 0, sizeof(BITMAPINFOHEADER));
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = width;
    bih.biHeight = height; // 正数表示自底向上
    bih.biPlanes = 1;
    bih.biBitCount = bitCount;
    bih.biCompression = BI_RGB;
    bih.biSizeImage = imageSize;

    memset(&bfh, 0, sizeof(BITMAPFILEHEADER));
    bfh.bfType = 0x4D42; // 'BM'
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + imageSize;

    unsigned char* pPixels = (unsigned char*)malloc(imageSize);
    if (!pPixels) {
        printf("[-] 内存分配失败 (大小: %lu 字节)\n", imageSize);
        return false;
    }

    if (!GetDIBits(hDC, hBitmap, 0, height, pPixels, (BITMAPINFO*)&bih, DIB_RGB_COLORS)) {
        printf("[-] GetDIBits 失败 (错误码: %lu)\n", GetLastError());
        free(pPixels);
        return false;
    }

    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        printf("[-] 无法创建输出文件: %s\n", filename);
        free(pPixels);
        return false;
    }

    fwrite(&bfh, sizeof(BITMAPFILEHEADER), 1, fp);
    fwrite(&bih, sizeof(BITMAPINFOHEADER), 1, fp);
    fwrite(pPixels, 1, imageSize, fp);
    fclose(fp);
    free(pPixels);

    return true;
}

// 捕获指定窗口的画面
static bool CaptureWindowToFile(HWND hWnd, const char* outputFile) {
    if (!IsWindow(hWnd)) {
        printf("[-] 无效的窗口句柄\n");
        return false;
    }

    // 确保窗口处于前台并激活
    SetForegroundWindow(hWnd);
    BringWindowToTop(hWnd);
    UpdateWindow(hWnd);
    Sleep(200);

    RECT rc;
    GetWindowRect(hWnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    if (width <= 0 || height <= 0) {
        printf("[-] 窗口尺寸异常: %dx%d\n", width, height);
        return false;
    }

    printf("[+] 窗口坐标: (%ld, %ld) - (%ld, %ld), 尺寸: %dx%d\n", 
           rc.left, rc.top, rc.right, rc.bottom, width, height);

    HDC hScreenDC = GetDC(NULL);
    if (!hScreenDC) {
        printf("[-] 获取屏幕 DC 失败\n");
        return false;
    }

    HDC hMemDC = CreateCompatibleDC(hScreenDC);
    if (!hMemDC) {
        printf("[-] 创建内存 DC 失败\n");
        ReleaseDC(NULL, hScreenDC);
        return false;
    }

    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    if (!hBitmap) {
        printf("[-] 创建兼容位图失败\n");
        DeleteDC(hMemDC);
        ReleaseDC(NULL, hScreenDC);
        return false;
    }

    HGDIOBJ hOldBmp = SelectObject(hMemDC, hBitmap);

    // 优先尝试 PrintWindow (PW_RENDERFULLCONTENT 模式支持后台渲染，兼容 Win8.1+)
    BOOL printed = PrintWindow(hWnd, hMemDC, 2);
    if (!printed) {
        // 退化为屏幕 BitBlt 抓取
        BitBlt(hMemDC, 0, 0, width, height, hScreenDC, rc.left, rc.top, SRCCOPY);
    }

    SelectObject(hMemDC, hOldBmp);

    bool ok = SaveBitmapToFile(hBitmap, hMemDC, outputFile);

    DeleteObject(hBitmap);
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hScreenDC);

    return ok;
}

int main(int argc, char* argv[]) {
    const char* exePath = "Photo Patch.exe";
    const char* outputPath = "tests/ui_screenshot.bmp";

    if (argc > 1) {
        exePath = argv[1];
    }
    if (argc > 2) {
        outputPath = argv[2];
    }

    printf("==================================================\n");
    printf("  Topaz Photo AI UI 自动化黑盒截图测试 (纯 C 语言)\n");
    printf("==================================================\n");
    printf("[*] 目标程序: %s\n", exePath);
    printf("[*] 输出图像: %s\n", outputPath);

    // 1. 检查可执行文件是否存在
    if (GetFileAttributesA(exePath) == INVALID_FILE_ATTRIBUTES) {
        printf("[-] 错误：找不到目标可执行文件 %s\n", exePath);
        return 1;
    }

    // 2. 启动子进程 (使用 ShellExecuteExA 以支持 UAC 提权要求)
    SHELLEXECUTEINFOA sei;
    memset(&sei, 0, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = "open";
    sei.lpFile = exePath;
    sei.nShow = SW_SHOWNORMAL;

    HANDLE hProcess = NULL;
    if (!ShellExecuteExA(&sei)) {
        DWORD dwErr = GetLastError();
        printf("[-] ShellExecuteExA 启动失败 (错误码: %lu)，尝试 CreateProcessA...\n", dwErr);
        
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        memset(&si, 0, sizeof(si));
        si.cb = sizeof(si);
        memset(&pi, 0, sizeof(pi));

        char cmdLine[MAX_PATH];
        snprintf(cmdLine, sizeof(cmdLine), "\"%s\"", exePath);

        if (!CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            printf("[-] CreateProcessA 也失败 (错误码: %lu)\n", GetLastError());
            return 2;
        }
        hProcess = pi.hProcess;
        CloseHandle(pi.hThread);
    } else {
        hProcess = sei.hProcess;
    }

    printf("[+] 目标进程已成功唤起 (Process Handle: %p)\n", (void*)hProcess);

    // 3. 轮询查找窗口句柄 (最多等待 6 秒)
    HWND hWnd = NULL;
    int waitLimit = 60; // 60 * 100ms = 6s
    for (int i = 0; i < waitLimit; i++) {
        hWnd = FindWindowA("Dup2PatcherClass", NULL);
        if (hWnd && IsWindowVisible(hWnd)) {
            break;
        }
        Sleep(100);
    }

    if (!hWnd) {
        printf("[-] 错误：在超时时间内未检测到目标窗口 (Dup2PatcherClass)！\n");
        if (hProcess) {
            TerminateProcess(hProcess, 1);
            CloseHandle(hProcess);
        }
        return 3;
    }

    DWORD windowPid = 0;
    GetWindowThreadProcessId(hWnd, &windowPid);
    HANDLE hExactProcess = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, windowPid);

    printf("[+] 成功定位到目标窗口句柄 (HWND: %p, PID: %lu)\n", (void*)hWnd, windowPid);

    // 等待窗口完全完成 GDI 绘制与日志流初始化
    Sleep(800);

    // 4. 执行截图并保存
    printf("[*] 正在执行窗口高保真像素捕获...\n");
    bool capSuccess = CaptureWindowToFile(hWnd, outputPath);

    if (capSuccess) {
        printf("[+] 截图测试成功！位图已保存至: %s\n", outputPath);
    } else {
        printf("[-] 截图测试失败！\n");
    }

    // 5. 优雅关闭窗口
    printf("[*] 正在向窗口发送退出消息...\n");
    PostMessageA(hWnd, WM_COMMAND, 1004 /* IDC_BTN_EXIT */, 0);
    PostMessageA(hWnd, WM_CLOSE, 0, 0);

    // 轮询等待窗口销毁与进程退出 (最多 3 秒)
    bool isClosed = false;
    for (int i = 0; i < 30; i++) {
        if (!IsWindow(hWnd)) {
            isClosed = true;
            break;
        }
        Sleep(100);
    }

    if (isClosed) {
        printf("[+] 目标窗口已安全正常关闭。\n");
    } else {
        printf("[!] 目标窗口关闭超时。\n");
    }

    if (hExactProcess) CloseHandle(hExactProcess);
    if (hProcess && hProcess != hExactProcess) CloseHandle(hProcess);

    printf("==================================================\n");
    printf("%s\n", capSuccess ? "[+] 黑盒 UI 测试全部通过！" : "[-] 黑盒 UI 测试失败！");
    printf("==================================================\n");

    return capSuccess ? 0 : 4;
}
