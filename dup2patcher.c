#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/**
 * dup2patcher.c
 * 
 * 包含：Win32 GUI 窗口界面、日志输出框、应用补丁核心引擎及 7 组特征码补丁规则。
 */

// 控件 ID 定义
#define IDC_BTN_PATCH   1001
#define IDC_BTN_ABOUT   1002
#define IDC_BTN_EXIT    1003
#define IDC_EDIT_LOG    1004

// 全局变量
static HWND g_hDlg = NULL;
static HWND g_hEditLog = NULL;
static HINSTANCE g_hInstance = NULL;

// 补丁规则结构
typedef struct {
    size_t length;
    const unsigned char* search_bytes;
    const unsigned char* search_mask;   // 1: 通配符 ??, 0: 精确匹配
    const unsigned char* replace_bytes;
    const unsigned char* replace_mask;  // 1: 保留原值, 0: 写入新值
    int max_occurrences;
    const char* description;
} PatchRule;

// ======================= 7 组补丁规则定义 =======================

// 规则 1: 授权状态验证强制返回 True (mov al, 1)
static const unsigned char s1[] = "\x74\x00\x48\x8D\x00\x00\x00\x00\x00\x48\x8B\xCB\xFF\x15\x00\x00\x00\x00\x84\xC0\x00\x00\x48\x8D\x00\x00\x00\x00\x00\x48\x8B\xCB\xFF\x15\x00\x00\x00\x00\x84\xC0\x00\x00\xB0\x01\x48\x83\xC4\x00\x5B\xC3\x32\xC0";
static const unsigned char sm1[] = {0,1,0,0,1,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,1,1,0,0,1,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,1,1,0,0,0,0,0,1,0,0,0,0};
static const unsigned char r1[] = "\x90\x90\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xB0\x01";
static const unsigned char rm1[] = {0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0};

// 规则 2: 跳过未登录状态检测
static const unsigned char s2[] = "\x74\x00\x80\xBF\x00\x00\x00\x00\x00\x75\x00\x45";
static const unsigned char sm2[] = {0,1,0,0,1,1,1,1,0,0,1,0};
static const unsigned char r2[] = "\xEB\x00\x00\x00\x00\x00\x00\x00\x00\x90\x90\x00";
static const unsigned char rm2[] = {0,1,1,1,1,1,1,1,1,0,0,1};

// 规则 3: 消除授权校验失败跳转
static const unsigned char s3[] = "\x84\xC0\x74\x00\xBA\x00\x00\x00\x00\x48\x00\x00\xE8";
static const unsigned char sm3[] = {0,0,0,1,0,1,1,1,1,0,1,1,0};
static const unsigned char r3[] = "\x00\x00\x90\x90\x00\x00\x00\x00\x00\x00\x00\x00\x00";
static const unsigned char rm3[] = {1,1,0,0,1,1,1,1,1,1,1,1,1};

// 规则 4: 屏蔽升级与提示弹窗
static const unsigned char s4[] = "\x00\x74\x00\x48\x8D\x15\x00\x00\x00\x00\x48\x00\x00\x00\x00\x00\x00\xFF\x15";
static const unsigned char sm4[] = {0,0,1,0,0,0,1,1,1,1,0,1,1,1,1,1,1,0,0};
static const unsigned char r4[] = "\x00\xEB\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
static const unsigned char rm4[] = {1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};

// 规则 5: 遥测数据采集入口直接 RET (0xC3)
static const unsigned char s5[] = "\x48\x00\x00\x00\x00\x48\x00\x00\x00\x20\x57\x48\x83\xEC\x00\x48\x8B\xF1";
static const unsigned char sm5[] = {0,1,1,1,1,0,1,1,1,0,0,0,0,0,1,0,0,0};
static const unsigned char r5[] = "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xC3\x00\x00\x00\x00\x00\x00\x00";
static const unsigned char rm5[] = {1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1};

// 规则 6: 反转在线权益状态判定 (je -> jne)
static const unsigned char s6[] = "\x84\xC0\x0F\x84\x00\x00\x00\x00\x48\x8B\x00\xE8\x00\x00\x00\x00\x84\xC0";
static const unsigned char sm6[] = {0,0,0,0,1,1,0,0,0,0,1,0,1,1,1,1,0,0};
static const unsigned char r6[] = "\x00\x00\x00\x85\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
static const unsigned char rm6[] = {1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1};

// 规则 7: 抹除遥测服务器地址
static const unsigned char s7[] = "https://et.topazlabs.com/v1/track";
static const unsigned char sm7[33] = {0};
static const unsigned char r7[33] = {0};
static const unsigned char rm7[33] = {0};

static PatchRule g_Rules[] = {
    {sizeof(s1) - 1, s1, sm1, r1, rm1, 1, "免登录授权校验强制通过"},
    {sizeof(s2) - 1, s2, sm2, r2, rm2, 1, "跳过未登录状态检测"},
    {sizeof(s3) - 1, s3, sm3, r3, rm3, 1, "消除授权失败判断分支"},
    {sizeof(s4) - 1, s4, sm4, r4, rm4, 1, "屏蔽升级提醒弹窗"},
    {sizeof(s5) - 1, s5, sm5, r5, rm5, 1, "拦截匿名数据采集入口"},
    {sizeof(s6) - 1, s6, sm6, r6, rm6, 2, "反转在线权益状态判定"},
    {33,             s7, sm7, r7, rm7, 1, "清空遥测上报接口 URL"}
};

// ======================= 补丁引擎与 GUI 辅助函数 =======================

/**
 * 追加文本信息到界面的 Edit 日志框
 */
__declspec(dllexport) void AddMsg(const char* msg) {
    if (!g_hEditLog) return;
    int len = GetWindowTextLengthA(g_hEditLog);
    SendMessageA(g_hEditLog, EM_SETSEL, len, len);
    SendMessageA(g_hEditLog, EM_REPLACESEL, FALSE, (LPARAM)msg);
    SendMessageA(g_hEditLog, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");
}

/**
 * 内存特征码搜索与替换
 */
__declspec(dllexport) int SearchAndReplace(
    unsigned char* pMemory,
    size_t memorySize,
    const PatchRule* pRule
) {
    int count = 0;
    for (size_t i = 0; i + pRule->length <= memorySize; i++) {
        bool match = true;
        for (size_t j = 0; j < pRule->length; j++) {
            if (pRule->search_mask[j] == 0 && pMemory[i + j] != pRule->search_bytes[j]) {
                match = false;
                break;
            }
        }

        if (match) {
            for (size_t j = 0; j < pRule->length; j++) {
                if (pRule->replace_mask[j] == 0) {
                    pMemory[i + j] = pRule->replace_bytes[j];
                }
            }
            count++;
            if (pRule->max_occurrences > 0 && count >= pRule->max_occurrences) {
                break;
            }
        }
    }
    return count;
}

/**
 * 执行补丁流程
 */
static void DoPatch() {
    const char* targetDll = "network.dll";
    AddMsg("--- 开始修补 ---");
    AddMsg("目标文件: network.dll");

    HANDLE hFile = CreateFileA(
        targetDll,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        AddMsg("[-] 无法打开 network.dll，请确保补丁放在软件安装目录！");
        return;
    }

    DWORD dwFileSize = GetFileSize(hFile, NULL);
    if (dwFileSize == 0 || dwFileSize == INVALID_FILE_SIZE) {
        AddMsg("[-] 文件大小异常！");
        CloseHandle(hFile);
        return;
    }

    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READWRITE, 0, 0, NULL);
    if (!hMap) {
        AddMsg("[-] 创建文件映射失败！");
        CloseHandle(hFile);
        return;
    }

    unsigned char* pData = (unsigned char*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!pData) {
        AddMsg("[-] 映射视图失败！");
        CloseHandle(hMap);
        CloseHandle(hFile);
        return;
    }

    int totalPatched = 0;
    char msgBuf[256];

    for (int i = 0; i < 7; i++) {
        int count = SearchAndReplace(pData, dwFileSize, &g_Rules[i]);
        snprintf(msgBuf, sizeof(msgBuf), "  规则 %d [%s]: 修改 %d 处", i + 1, g_Rules[i].description, count);
        AddMsg(msgBuf);
        totalPatched += count;
    }

    UnmapViewOfFile(pData);
    CloseHandle(hMap);
    CloseHandle(hFile);

    if (totalPatched > 0) {
        AddMsg("[+] 补丁应用成功！共修改了全部特征点。");
    } else {
        AddMsg("[!] 未匹配到特征码，该文件可能已打过补丁或版本不匹配。");
    }
}

// 窗口过程函数
static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        // 创建 UI 控件
        g_hEditLog = CreateWindowExA(
            WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            10, 10, 365, 140,
            hWnd, (HMENU)IDC_EDIT_LOG, g_hInstance, NULL
        );

        CreateWindowExA(
            0, "BUTTON", "应用补丁",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 160, 110, 30,
            hWnd, (HMENU)IDC_BTN_PATCH, g_hInstance, NULL
        );

        CreateWindowExA(
            0, "BUTTON", "关于",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            135, 160, 110, 30,
            hWnd, (HMENU)IDC_BTN_ABOUT, g_hInstance, NULL
        );

        CreateWindowExA(
            0, "BUTTON", "退出",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            265, 160, 110, 30,
            hWnd, (HMENU)IDC_BTN_EXIT, g_hInstance, NULL
        );
    }
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        switch (wmId) {
        case IDC_BTN_PATCH:
            DoPatch();
            break;
        case IDC_BTN_ABOUT:
            MessageBoxA(
                hWnd,
                "Photo 逆向学习补丁工具\n"
                MB_OK | MB_ICONINFORMATION
            );
            break;
        case IDC_BTN_EXIT:
            DestroyWindow(hWnd);
            break;
        }
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcA(hWnd, uMsg, wParam, lParam);
    }
}

/**
 * 主入口导出函数
 */
__declspec(dllexport) void load_patcher(void) {
    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = g_hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "Dup2PatcherClass";

    RegisterClassExA(&wc);

    // 计算居中窗口坐标
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int winW = 400;
    int winH = 240;
    int posX = (screenW - winW) / 2;
    int posY = (screenH - winH) / 2;

    g_hDlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "Dup2PatcherClass",
        "Topaz Photo AI Patch",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        posX, posY, winW, winH,
        NULL, NULL, g_hInstance, NULL
    );

    if (!g_hDlg) return;

    ShowWindow(g_hDlg, SW_SHOW);
    UpdateWindow(g_hDlg);

    // 消息循环
    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

/**
 * 模块入口 DllMain
 */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        g_hInstance = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
    }
    return TRUE;
}
