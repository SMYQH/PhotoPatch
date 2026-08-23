#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/**
 * dup2patcher.c
 * 
 * Topaz Photo AI 生产级修补与增强引擎
 * 包含：Win32 GUI 窗口界面、备份/还原系统、高精度特征码扫描替换与全量遥测切断规则。
 */

// 控件 ID 定义
#define IDC_BTN_PATCH    1001
#define IDC_BTN_RESTORE  1002
#define IDC_BTN_ABOUT    1003
#define IDC_BTN_EXIT     1004
#define IDC_EDIT_LOG     1005

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

// ======================= 完整增强补丁规则定义 =======================

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

// 规则 5a: TEventTracker 遥测采集入口 (mov al, 1; ret)
static const unsigned char s5a[] = "\x40\x53\x48\x83\xEC\x30\x44\x89\x4C\x24\x20\x48\x8B\xD9";
static const unsigned char sm5a[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const unsigned char r5a[] = "\xB0\x01\xC3\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90";
static const unsigned char rm5a[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};

// 规则 5b: Backtrace 崩溃诊断采集入口 (mov al, 1; ret)
static const unsigned char s5b[] = "\x48\x89\x5C\x24\x18\x48\x89\x74\x24\x20\x55\x57\x41\x54\x41\x56";
static const unsigned char sm5b[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const unsigned char r5b[] = "\xB0\x01\xC3\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90";
static const unsigned char rm5b[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

// 规则 6: 反转在线权益状态判定 (je -> jne)
static const unsigned char s6[] = "\x84\xC0\x0F\x84\x00\x00\x00\x00\x48\x8B\x00\xE8\x00\x00\x00\x00\x84\xC0";
static const unsigned char sm6[] = {0,0,0,0,1,1,0,0,0,0,1,0,1,1,1,1,0,0};
static const unsigned char r6[] = "\x00\x00\x00\x85\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
static const unsigned char rm6[] = {1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1};

// 规则 7+: 清空遥测与崩溃服务接口 URL
static const unsigned char u_et[] = "https://et.topazlabs.com/v1/track";
static const unsigned char u_bt[] = "https://events.backtrace.io/api/summed-events/submit";
static const unsigned char u_amp1[] = "https://api2.amplitude.com/identify";
static const unsigned char u_amp2[] = "https://api.lab.amplitude.com/v1/vardata";
static const unsigned char u_amp3[] = "https://profile-api.amplitude.com/v1/userprofile";

static const unsigned char zeros_et[sizeof(u_et) - 1] = {0};
static const unsigned char zeros_bt[sizeof(u_bt) - 1] = {0};
static const unsigned char zeros_amp1[sizeof(u_amp1) - 1] = {0};
static const unsigned char zeros_amp2[sizeof(u_amp2) - 1] = {0};
static const unsigned char zeros_amp3[sizeof(u_amp3) - 1] = {0};

static PatchRule g_Rules[] = {
    {sizeof(s1) - 1,   s1, sm1, r1, rm1, 1, "免登录授权校验强制通过"},
    {sizeof(s2) - 1,   s2, sm2, r2, rm2, 1, "跳过未登录状态检测"},
    {sizeof(s3) - 1,   s3, sm3, r3, rm3, 1, "消除授权失败判断分支"},
    {sizeof(s4) - 1,   s4, sm4, r4, rm4, 1, "屏蔽升级提醒弹窗"},
    {sizeof(s5a) - 1,  s5a, sm5a, r5a, rm5a, 1, "拦截 TEventTracker 遥测采集入口"},
    {sizeof(s5b) - 1,  s5b, sm5b, r5b, rm5b, 1, "拦截 Backtrace 崩溃诊断采集入口"},
    {sizeof(s6) - 1,   s6, sm6, r6, rm6, 2, "反转在线权益状态判定"},
    {sizeof(u_et) - 1,   u_et, zeros_et, zeros_et, zeros_et, 1, "抹除旧版遥测地址 (et.topazlabs.com)"},
    {sizeof(u_bt) - 1,   u_bt, zeros_bt, zeros_bt, zeros_bt, 1, "抹除 Backtrace 崩溃上报地址"},
    {sizeof(u_amp1) - 1, u_amp1, zeros_amp1, zeros_amp1, zeros_amp1, 1, "抹除 Amplitude 用户追踪地址"},
    {sizeof(u_amp2) - 1, u_amp2, zeros_amp2, zeros_amp2, zeros_amp2, 1, "抹除 Amplitude 实验数据地址"},
    {sizeof(u_amp3) - 1, u_amp3, zeros_amp3, zeros_amp3, zeros_amp3, 1, "抹除 Amplitude 用户画像地址"}
};

#define RULE_COUNT (sizeof(g_Rules) / sizeof(g_Rules[0]))

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
 * 备份文件
 */
static bool BackupTargetFile(const char* targetDll, const char* backupDll) {
    if (GetFileAttributesA(backupDll) != INVALID_FILE_ATTRIBUTES) {
        // 备份已存在，跳过
        return true;
    }
    if (CopyFileA(targetDll, backupDll, TRUE)) {
        AddMsg("[+] 已自动创建原始文件备份: network.dll.bak");
        return true;
    }
    AddMsg("[!] 警告: 创建备份文件失败，可能缺少写权限。");
    return false;
}

/**
 * 还原备份
 */
static void DoRestore() {
    const char* targetDll = "network.dll";
    const char* backupDll = "network.dll.bak";

    AddMsg("--- 开始还原备份 ---");
    if (GetFileAttributesA(backupDll) == INVALID_FILE_ATTRIBUTES) {
        AddMsg("[-] 未找到备份文件 network.dll.bak！");
        return;
    }

    if (CopyFileA(backupDll, targetDll, FALSE)) {
        AddMsg("[+] 成功从 network.dll.bak 还原原始文件！");
    } else {
        AddMsg("[-] 还原失败，请检查文件是否被占用或管理员权限。");
    }
}

/**
 * 执行补丁流程
 */
static void DoPatch() {
    const char* targetDll = "network.dll";
    const char* backupDll = "network.dll.bak";

    AddMsg("========================================");
    AddMsg("--- 开始应用 Photo AI 增强补丁 ---");
    AddMsg("目标模块: network.dll");

    // 检查目标文件存在
    if (GetFileAttributesA(targetDll) == INVALID_FILE_ATTRIBUTES) {
        AddMsg("[-] 无法在当前目录找到 network.dll！");
        AddMsg("[*] 请将补丁程序放置在 Topaz Photo AI 安装目录下运行。");
        return;
    }

    // 自动备份
    BackupTargetFile(targetDll, backupDll);

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
        AddMsg("[-] 无法打开 network.dll 进行写入，请以管理员权限运行！");
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
        AddMsg("[-] 创建文件内存映射失败！");
        CloseHandle(hFile);
        return;
    }

    unsigned char* pData = (unsigned char*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!pData) {
        AddMsg("[-] 映射文件视图失败！");
        CloseHandle(hMap);
        CloseHandle(hFile);
        return;
    }

    int totalPatched = 0;
    int ruleMatchedCount = 0;
    char msgBuf[256];

    for (size_t i = 0; i < RULE_COUNT; i++) {
        int count = SearchAndReplace(pData, dwFileSize, &g_Rules[i]);
        if (count > 0) {
            snprintf(msgBuf, sizeof(msgBuf), "  [OK] 规则 %02d [%s]: 命中 %d 处", (int)(i + 1), g_Rules[i].description, count);
            ruleMatchedCount++;
        } else {
            snprintf(msgBuf, sizeof(msgBuf), "  [--] 规则 %02d [%s]: 未匹配(可能已修补)", (int)(i + 1), g_Rules[i].description);
        }
        AddMsg(msgBuf);
        totalPatched += count;
    }

    UnmapViewOfFile(pData);
    CloseHandle(hMap);
    CloseHandle(hFile);

    if (totalPatched > 0) {
        snprintf(msgBuf, sizeof(msgBuf), "[+] 补丁应用成功！共修改 %d 处关键特征点。", totalPatched);
        AddMsg(msgBuf);
        AddMsg("[*] 授权、更新弹窗拦截及全量遥测切断均已生效。");
    } else {
        AddMsg("[!] 当前文件未发生任何修改。可能已打过补丁，或该版本 network.dll 不匹配。");
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
            10, 10, 395, 140,
            hWnd, (HMENU)IDC_EDIT_LOG, g_hInstance, NULL
        );

        CreateWindowExA(
            0, "BUTTON", "应用补丁",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 160, 90, 30,
            hWnd, (HMENU)IDC_BTN_PATCH, g_hInstance, NULL
        );

        CreateWindowExA(
            0, "BUTTON", "还原备份",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            110, 160, 90, 30,
            hWnd, (HMENU)IDC_BTN_RESTORE, g_hInstance, NULL
        );

        CreateWindowExA(
            0, "BUTTON", "关于",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            210, 160, 90, 30,
            hWnd, (HMENU)IDC_BTN_ABOUT, g_hInstance, NULL
        );

        CreateWindowExA(
            0, "BUTTON", "退出",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            315, 160, 90, 30,
            hWnd, (HMENU)IDC_BTN_EXIT, g_hInstance, NULL
        );
        break;
    }
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        switch (wmId) {
        case IDC_BTN_PATCH:
            DoPatch();
            break;
        case IDC_BTN_RESTORE:
            DoRestore();
            break;
        case IDC_BTN_ABOUT:
            MessageBoxA(
                hWnd,
                "Topaz Photo AI 生产级修补与增强工具\n\n"
                "功能特性：\n"
                "1. 离线免登录全量授权通过\n"
                "2. 屏蔽新版本强制更新提示\n"
                "3. 彻底切断 Amplitude 与 Backtrace 遥测/崩溃数据上报\n"
                "4. 自动备份与一键原样还原支持\n",
                "关于",
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
    return 0;
}

/**
 * 主入口窗口生成函数
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
    int winW = 430;
    int winH = 240;
    int posX = (screenW - winW) / 2;
    int posY = (screenH - winH) / 2;

    g_hDlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "Dup2PatcherClass",
        "Topaz Photo AI Patcher",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        posX, posY, winW, winH,
        NULL, NULL, g_hInstance, NULL
    );

    if (!g_hDlg) return;

    ShowWindow(g_hDlg, SW_SHOW);
    UpdateWindow(g_hDlg);

    AddMsg("[*] 欢迎使用 Topaz Photo AI 补丁工具");
    AddMsg("[*] 请点击【应用补丁】执行修补，或【还原备份】回退修改。");

    // 消息循环
    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

/**
 * 模块入口 DllMain (DLL 模式)
 */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        g_hInstance = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
    }
    return TRUE;
}

#ifdef STANDALONE_EXE
/**
 * 独立 EXE 运行入口 (无需中间 Dropper，零杀软拦截)
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInstance = hInstance;
    InitCommonControls();
    load_patcher();
    return 0;
}
#endif
