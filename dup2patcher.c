#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "zh_models_data.h"

/**
 * dup2patcher.c - v1.1.2 (WinUI 3 Fluent Edition)
 * 
 * Topaz Photo AI 生产级修补与汉化增强引擎
 * 包含：WinUI 3 现代 Fluent Design 风格界面 (ClearType 高清排版、DWM 动态圆角、
 *      卡片式容器、Accent 交互状态)、进程冲突检测、安装路径自适应探测、
 *      特征码内存补丁、全量遥测切断与 73 款 AI 模型全量中文汉化。
 */

// 控件 ID 定义
#define IDC_BTN_PATCH     1001
#define IDC_BTN_RESTORE   1002
#define IDC_BTN_ABOUT     1003
#define IDC_BTN_EXIT      1004
#define IDC_EDIT_LOG      1005
#define IDC_CHK_LOCALIZE  1006

// 全局句柄与状态
static HWND g_hDlg = NULL;
static HWND g_hEditLog = NULL;
static HWND g_hChkLocalize = NULL;
static HINSTANCE g_hInstance = NULL;

// WinUI 3 现代排版字体句柄
static HFONT g_hFontTitle = NULL;
static HFONT g_hFontSubtitle = NULL;
static HFONT g_hFontBadge = NULL;
static HFONT g_hFontCardTitle = NULL;
static HFONT g_hFontHint = NULL;
static HFONT g_hFontCheckbox = NULL;
static HFONT g_hFontLog = NULL;
static HFONT g_hFontButton = NULL;

// 常用 GDI 笔刷缓存
static HBRUSH g_hBrushWindowBg = NULL; // #F3F3F3 WinUI 背景
static HBRUSH g_hBrushCardBg = NULL;   // #FFFFFF 卡片背景
static HPEN   g_hPenCardBorder = NULL; // #E5E5E5 卡片边框

// 布局尺寸缓存 (基准 96 DPI，按当前 DPI 动态缩放)
static int g_Dpi = 96;
static int g_WinW = 560;
static int g_WinH = 490;
static int g_PadX = 20;
static int g_HeaderH = 68;
static int g_OptCardY = 82;
static int g_OptCardH = 56;
static int g_LogCardY = 150;
static int g_LogCardH = 236;
static int g_BtnY = 402;
static int g_BtnH = 36;

// 补丁规则结构
typedef struct {
    size_t length;
    const unsigned char* search_bytes;
    const unsigned char* search_mask;   // NULL: 全部精确匹配; 1: 通配符 ??, 0: 精确匹配
    const unsigned char* replace_bytes;  // NULL: 全部填充 0x00
    const unsigned char* replace_mask;  // NULL: 全部覆盖写入; 1: 保留原值, 0: 写入新值
    int max_occurrences;
    const char* description;
} PatchRule;

// ======================= network.dll 核心增强补丁规则 =======================

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
static const unsigned char r5a[] = "\xB0\x01\xC3\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90";

// 规则 5b: Backtrace 崩溃诊断采集入口 (mov al, 1; ret)
static const unsigned char s5b[] = "\x48\x89\x5C\x24\x18\x48\x89\x74\x24\x20\x55\x57\x41\x54\x41\x56";
static const unsigned char r5b[] = "\xB0\x01\xC3\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90";

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

static const PatchRule g_NetworkRules[] = {
    {sizeof(s1) - 1,   s1, sm1,  r1,   rm1,  1, "免登录授权校验强制通过"},
    {sizeof(s2) - 1,   s2, sm2,  r2,   rm2,  1, "跳过未登录状态检测"},
    {sizeof(s3) - 1,   s3, sm3,  r3,   rm3,  1, "消除授权失败判断分支"},
    {sizeof(s4) - 1,   s4, sm4,  r4,   rm4,  1, "屏蔽升级提醒弹窗"},
    {sizeof(s5a) - 1,  s5a, NULL, r5a,  NULL, 1, "拦截 TEventTracker 遥测采集入口"},
    {sizeof(s5b) - 1,  s5b, NULL, r5b,  NULL, 1, "拦截 Backtrace 崩溃诊断采集入口"},
    {sizeof(s6) - 1,   s6, sm6,  r6,   rm6,  2, "反转在线权益状态判定"},
    {sizeof(u_et) - 1,   u_et,   NULL, NULL, NULL, 1, "抹除旧版遥测地址 (et.topazlabs.com)"},
    {sizeof(u_bt) - 1,   u_bt,   NULL, NULL, NULL, 1, "抹除 Backtrace 崩溃上报地址"},
    {sizeof(u_amp1) - 1, u_amp1, NULL, NULL, NULL, 1, "抹除 Amplitude 用户追踪地址"},
    {sizeof(u_amp2) - 1, u_amp2, NULL, NULL, NULL, 1, "抹除 Amplitude 实验数据地址"},
    {sizeof(u_amp3) - 1, u_amp3, NULL, NULL, NULL, 1, "抹除 Amplitude 用户画像地址"}
};

#define NETWORK_RULE_COUNT (sizeof(g_NetworkRules) / sizeof(g_NetworkRules[0]))

// ======================= Topaz Photo AI.exe 界面汉化规则 =======================

// 汉化规则: Preferences -> 首选项\0\0\0 (12 字节定长)
static const unsigned char s_pref[] = "Preferences\x00";
static const unsigned char r_pref[] = "\xE9\xA6\x96\xE9\x80\x89\xE9\xA1\xB9\x00\x00\x00";

// 汉化规则: Resolution -> 分辨率\0\0 (11 字节定长)
static const unsigned char s_reso[] = "Resolution\x00";
static const unsigned char r_reso[] = "\xE5\x88\x86\xE8\xBE\xA8\xE7\x8E\x87\x00\x00";

// 汉化规则: Sharpen -> 锐化\0\0 (8 字节定长)
static const unsigned char s_shrp[] = "Sharpen\x00";
static const unsigned char r_shrp[] = "\xE9\x94\x90\xE5\x8C\x96\x00\x00";

// 汉化规则: Denoise -> 降噪\0\0 (8 字节定长)
static const unsigned char s_dens[] = "Denoise\x00";
static const unsigned char r_dens[] = "\xE9\x99\x8D\xE5\x99\xAA\x00\x00";

// 汉化规则: Enhance -> 增强\0\0 (8 字节定长)
static const unsigned char s_enhc[] = "Enhance\x00";
static const unsigned char r_enhc[] = "\xE5\xA2\x9E\xE5\xBC\xBA\x00\x00";

// 汉化规则: Cancel -> 取消\0 (7 字节定长)
static const unsigned char s_cncl[] = "Cancel\x00";
static const unsigned char r_cncl[] = "\xE5\x8F\x96\xE6\xB6\x88\x00";

// 汉化规则: Apply -> 应用 (6 字节定长)
static const unsigned char s_appl[] = "Apply\x00";
static const unsigned char r_appl[] = "\xE5\xBA\x94\xE7\x94\xA8";

static const PatchRule g_ExeRules[] = {
    {sizeof(s_pref), s_pref, NULL, r_pref, NULL, 1, "汉化: Preferences -> 首选项"},
    {sizeof(s_reso), s_reso, NULL, r_reso, NULL, 2, "汉化: Resolution -> 分辨率"},
    {sizeof(s_shrp), s_shrp, NULL, r_shrp, NULL, 1, "汉化: Sharpen -> 锐化"},
    {sizeof(s_dens), s_dens, NULL, r_dens, NULL, 1, "汉化: Denoise -> 降噪"},
    {sizeof(s_enhc), s_enhc, NULL, r_enhc, NULL, 1, "汉化: Enhance -> 增强"},
    {sizeof(s_cncl), s_cncl, NULL, r_cncl, NULL, 1, "汉化: Cancel -> 取消"},
    {sizeof(s_appl), s_appl, NULL, r_appl, NULL, 1, "汉化: Apply -> 应用"}
};

#define EXE_RULE_COUNT (sizeof(g_ExeRules) / sizeof(g_ExeRules[0]))

// ======================= 补丁引擎与辅助函数 =======================

/**
 * 追加文本信息到界面的 Edit 日志框并平滑自动滚屏
 */
__declspec(dllexport) void AddMsg(const char* msg) {
    if (!g_hEditLog) return;
    int len = GetWindowTextLengthA(g_hEditLog);
    SendMessageA(g_hEditLog, EM_SETSEL, len, len);
    SendMessageA(g_hEditLog, EM_REPLACESEL, FALSE, (LPARAM)msg);
    SendMessageA(g_hEditLog, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");
    SendMessageA(g_hEditLog, EM_SCROLLCARET, 0, 0);
}

/**
 * 检测目标主程序是否正在运行
 */
static bool IsTopazRunning(void) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    bool running = false;

    if (Process32First(hSnap, &pe32)) {
        do {
            if (_stricmp(pe32.szExeFile, "Topaz Photo AI.exe") == 0 ||
                _stricmp(pe32.szExeFile, "tpai.exe") == 0) {
                running = true;
                break;
            }
        } while (Process32Next(hSnap, &pe32));
    }

    CloseHandle(hSnap);
    return running;
}

/**
 * 递归创建目录
 */
static void CreateDirectoryRecursive(const char* path) {
    char temp[MAX_PATH];
    snprintf(temp, sizeof(temp), "%s", path);
    for (char* p = temp + 1; *p; p++) {
        if (*p == '\\' || *p == '/') {
            char c = *p;
            *p = '\0';
            CreateDirectoryA(temp, NULL);
            *p = c;
        }
    }
    CreateDirectoryA(temp, NULL);
}

/**
 * 确保定位到 Topaz Photo AI 安装根目录 (通过注册表与系统路径智能探测)
 */
static bool EnsureTargetDirectory(void) {
    char targetDir[MAX_PATH] = {0};

    // 1. 检查注册表 HKLM\SOFTWARE\Topaz Labs LLC\Topaz Photo AI (64位视图)
    HKEY hKey = NULL;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Topaz Labs LLC\\Topaz Photo AI", 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
        DWORD dwType = REG_SZ;
        DWORD dwSize = sizeof(targetDir);
        if (RegQueryValueExA(hKey, "InstallDir", NULL, &dwType, (LPBYTE)targetDir, &dwSize) == ERROR_SUCCESS ||
            RegQueryValueExA(hKey, "Path", NULL, &dwType, (LPBYTE)targetDir, &dwSize) == ERROR_SUCCESS ||
            RegQueryValueExA(hKey, "InstallLocation", NULL, &dwType, (LPBYTE)targetDir, &dwSize) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            char netPath[MAX_PATH];
            snprintf(netPath, sizeof(netPath), "%s\\network.dll", targetDir);
            if (GetFileAttributesA(netPath) != INVALID_FILE_ATTRIBUTES) {
                SetCurrentDirectoryA(targetDir);
                char msg[512];
                snprintf(msg, sizeof(msg), "[+] 自动探测到安装目录: %s", targetDir);
                AddMsg(msg);
                return true;
            }
        } else {
            RegCloseKey(hKey);
        }
    }

    // 2. 检查默认标准安装路径 Program Files
    char progFiles[MAX_PATH] = {0};
    if (GetEnvironmentVariableA("ProgramFiles", progFiles, sizeof(progFiles)) > 0) {
        snprintf(targetDir, sizeof(targetDir), "%s\\Topaz Labs LLC\\Topaz Photo AI", progFiles);
        char netPath[MAX_PATH];
        snprintf(netPath, sizeof(netPath), "%s\\network.dll", targetDir);
        if (GetFileAttributesA(netPath) != INVALID_FILE_ATTRIBUTES) {
            SetCurrentDirectoryA(targetDir);
            char msg[512];
            snprintf(msg, sizeof(msg), "[+] 自动定位到安装目录: %s", targetDir);
            AddMsg(msg);
            return true;
        }
    }

    return false;
}

/**
 * 内存特征码搜索与替换 (支持通配符、零填充与整段覆盖)
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
            if ((pRule->search_mask == NULL || pRule->search_mask[j] == 0) &&
                pMemory[i + j] != pRule->search_bytes[j]) {
                match = false;
                break;
            }
        }

        if (match) {
            for (size_t j = 0; j < pRule->length; j++) {
                if (pRule->replace_bytes == NULL) {
                    pMemory[i + j] = 0x00; // 零填充
                } else if (pRule->replace_mask == NULL || pRule->replace_mask[j] == 0) {
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
 * 备份目标文件 (具备幂等保护)
 */
static bool BackupFile(const char* targetFile, const char* backupFile) {
    if (GetFileAttributesA(backupFile) != INVALID_FILE_ATTRIBUTES) {
        return true; // 备份已存在，保留原版
    }
    return CopyFileA(targetFile, backupFile, TRUE);
}

/**
 * 对指定二进制文件应用 PatchRule 规则集
 */
static int PatchBinaryFile(const char* filename, const PatchRule* rules, size_t ruleCount) {
    char backupName[MAX_PATH];
    snprintf(backupName, sizeof(backupName), "%s.bak", filename);

    if (GetFileAttributesA(filename) == INVALID_FILE_ATTRIBUTES) {
        return -1; // 文件不存在
    }

    // 确保去除只读属性
    SetFileAttributesA(filename, FILE_ATTRIBUTE_NORMAL);

    BackupFile(filename, backupName);

    HANDLE hFile = CreateFileA(
        filename, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    );
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD dwErr = GetLastError();
        char errBuf[256];
        if (dwErr == ERROR_SHARING_VIOLATION) {
            snprintf(errBuf, sizeof(errBuf), "[-] 写入 %s 失败：文件正被占用，请先退出 Topaz Photo AI！", filename);
        } else if (dwErr == ERROR_ACCESS_DENIED) {
            snprintf(errBuf, sizeof(errBuf), "[-] 写入 %s 失败：权限不足，请右键以管理员身份运行！", filename);
        } else {
            snprintf(errBuf, sizeof(errBuf), "[-] 打开 %s 失败 (错误代码: %lu)", filename, dwErr);
        }
        AddMsg(errBuf);
        return -2;
    }

    DWORD dwFileSize = GetFileSize(hFile, NULL);
    if (dwFileSize == 0 || dwFileSize == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        return -3;
    }

    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READWRITE, 0, 0, NULL);
    if (!hMap) {
        CloseHandle(hFile);
        return -4;
    }

    unsigned char* pData = (unsigned char*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (!pData) {
        CloseHandle(hMap);
        CloseHandle(hFile);
        return -5;
    }

    int totalPatched = 0;
    char msgBuf[256];

    for (size_t i = 0; i < ruleCount; i++) {
        int count = SearchAndReplace(pData, dwFileSize, &rules[i]);
        if (count > 0) {
            snprintf(msgBuf, sizeof(msgBuf), "  [OK] %s: 命中 %d 处", rules[i].description, count);
            AddMsg(msgBuf);
            totalPatched += count;
        }
    }

    if (totalPatched == 0) {
        snprintf(msgBuf, sizeof(msgBuf), "  [!] %s 未匹配到待修补特征码 (可能已修补或版本不兼容)", filename);
        AddMsg(msgBuf);
    }

    UnmapViewOfFile(pData);
    CloseHandle(hMap);
    CloseHandle(hFile);
    return totalPatched;
}

/**
 * 定位 ProgramData models 路径 (通过系统环境变量与绝对路径智能探测)
 */
static bool GetModelsDirectory(char* outPath, size_t maxLen) {
    char programData[MAX_PATH] = {0};
    if (GetEnvironmentVariableA("ProgramData", programData, sizeof(programData)) > 0) {
        snprintf(outPath, maxLen, "%s\\Topaz Labs LLC\\Topaz Photo AI\\models", programData);
        if (GetFileAttributesA(outPath) == INVALID_FILE_ATTRIBUTES) {
            CreateDirectoryRecursive(outPath);
        }
        if (GetFileAttributesA(outPath) != INVALID_FILE_ATTRIBUTES) {
            return true;
        }
    }
    return false;
}

/**
 * 应用 73 款 AI 模型 JSON 中文汉化包
 */
static int ApplyModelsLocalization(void) {
    char modelsDir[MAX_PATH];
    if (!GetModelsDirectory(modelsDir, sizeof(modelsDir))) {
        AddMsg("[!] 未找到 ProgramData 模型目录，跳过模型 JSON 汉化。");
        return 0;
    }

    char msgBuf[512];
    snprintf(msgBuf, sizeof(msgBuf), "[+] 定位到 AI 模型目录: %s", modelsDir);
    AddMsg(msgBuf);

    int successCount = 0;
    for (size_t i = 0; i < ZH_MODEL_COUNT; i++) {
        char jsonPath[MAX_PATH];
        char bakPath[MAX_PATH];
        snprintf(jsonPath, sizeof(jsonPath), "%s\\%s", modelsDir, g_ZhModelFiles[i].filename);
        snprintf(bakPath, sizeof(bakPath), "%s\\%s.bak", modelsDir, g_ZhModelFiles[i].filename);

        // 如果存在原文件且尚未备份，则备份
        if (GetFileAttributesA(jsonPath) != INVALID_FILE_ATTRIBUTES) {
            BackupFile(jsonPath, bakPath);
        }

        // 写入汉化后的 JSON 内容
        FILE* fp = fopen(jsonPath, "wb");
        if (fp) {
            fputs(g_ZhModelFiles[i].content, fp);
            fclose(fp);
            successCount++;
        }
    }

    snprintf(msgBuf, sizeof(msgBuf), "[+] 成功部署 %d 个 AI 模型与控制面板中文描述符！", successCount);
    AddMsg(msgBuf);
    return successCount;
}

/**
 * 还原 73 款 AI 模型 JSON 原始备份
 */
static void RestoreModelsLocalization(void) {
    char modelsDir[MAX_PATH];
    if (!GetModelsDirectory(modelsDir, sizeof(modelsDir))) {
        return;
    }

    int restoredCount = 0;
    for (size_t i = 0; i < ZH_MODEL_COUNT; i++) {
        char jsonPath[MAX_PATH];
        char bakPath[MAX_PATH];
        snprintf(jsonPath, sizeof(jsonPath), "%s\\%s", modelsDir, g_ZhModelFiles[i].filename);
        snprintf(bakPath, sizeof(bakPath), "%s\\%s.bak", modelsDir, g_ZhModelFiles[i].filename);

        if (GetFileAttributesA(bakPath) != INVALID_FILE_ATTRIBUTES) {
            if (CopyFileA(bakPath, jsonPath, FALSE)) {
                restoredCount++;
            }
        }
    }
    if (restoredCount > 0) {
        char msgBuf[256];
        snprintf(msgBuf, sizeof(msgBuf), "[+] 成功从备份还原 %d 个原始英文模型描述符。", restoredCount);
        AddMsg(msgBuf);
    }
}

/**
 * 还原全部备份
 */
static void DoRestore(void) {
    AddMsg("==================================================");
    AddMsg("--- 开始执行一键原样还原 ---");

    if (IsTopazRunning()) {
        AddMsg("[-] 检测到 Topaz Photo AI 正在运行！");
        AddMsg("[*] 请先完全退出主程序后再执行还原。");
        return;
    }

    if (!EnsureTargetDirectory()) {
        AddMsg("[-] 未能自动探测到 Topaz Photo AI 安装目录！");
        return;
    }

    // 1. 还原 network.dll
    if (GetFileAttributesA("network.dll.bak") != INVALID_FILE_ATTRIBUTES) {
        SetFileAttributesA("network.dll", FILE_ATTRIBUTE_NORMAL);
        if (CopyFileA("network.dll.bak", "network.dll", FALSE)) {
            AddMsg("[+] 成功还原原始 network.dll");
        } else {
            AddMsg("[-] 还原 network.dll 失败，请检查文件占用或权限。");
        }
    }

    // 2. 还原 Topaz Photo AI.exe
    if (GetFileAttributesA("Topaz Photo AI.exe.bak") != INVALID_FILE_ATTRIBUTES) {
        SetFileAttributesA("Topaz Photo AI.exe", FILE_ATTRIBUTE_NORMAL);
        if (CopyFileA("Topaz Photo AI.exe.bak", "Topaz Photo AI.exe", FALSE)) {
            AddMsg("[+] 成功还原原始 Topaz Photo AI.exe");
        } else {
            AddMsg("[-] 还原 Topaz Photo AI.exe 失败，请检查文件占用或权限。");
        }
    }

    // 3. 还原 AI 模型 JSON
    RestoreModelsLocalization();
    AddMsg("[+] 还原流程完毕！");
}

/**
 * 执行补丁与汉化流程
 */
static void DoPatch(void) {
    AddMsg("==================================================");
    AddMsg("--- 开始应用 Topaz Photo AI 增强与汉化 ---");

    if (IsTopazRunning()) {
        AddMsg("[-] 检测到 Topaz Photo AI 正在运行！");
        AddMsg("[*] 请先完全退出主程序后再点击应用补丁。");
        return;
    }

    if (!EnsureTargetDirectory()) {
        AddMsg("[-] 未能自动探测到 Topaz Photo AI 安装目录！");
        AddMsg("[*] 请确认软件已正确安装到系统。");
        return;
    }

    // 1. 修补 network.dll
    AddMsg("[*] 正在修补授权鉴权与遥测截断模块: network.dll ...");
    int netRes = PatchBinaryFile("network.dll", g_NetworkRules, NETWORK_RULE_COUNT);
    if (netRes < 0) {
        return;
    }

    // 检查是否勾选了汉化包
    bool bEnableZh = true;
    if (g_hChkLocalize) {
        bEnableZh = (SendMessageA(g_hChkLocalize, BM_GETCHECK, 0, 0) == BST_CHECKED);
    }

    if (bEnableZh) {
        AddMsg("--------------------------------------------------");
        AddMsg("[*] 正在应用中文语言包与模型汉化 ...");
        
        // 2. 汉化 Topaz Photo AI.exe 界面关键菜单
        if (GetFileAttributesA("Topaz Photo AI.exe") != INVALID_FILE_ATTRIBUTES) {
            AddMsg("[*] 正在修补主程序界面词条: Topaz Photo AI.exe ...");
            PatchBinaryFile("Topaz Photo AI.exe", g_ExeRules, EXE_RULE_COUNT);
        }

        // 3. 部署 73 个 AI 模型的全量中文描述符
        ApplyModelsLocalization();
    }

    AddMsg("==================================================");
    AddMsg("[+] 全部操作执行完毕！");
    AddMsg("[*] 离线免登录授权、遥测切断与中文汉化已全部就绪。");
}

// ======================= WinUI 3 现代界面渲染子系统 =======================

/**
 * 动态加载 DWM API 设置现代 Windows 11 圆角窗口属性
 */
static void ApplyModernDwmAttributes(HWND hWnd) {
    HMODULE hDwm = LoadLibraryA("dwmapi.dll");
    if (hDwm) {
        typedef HRESULT (WINAPI *pfnDwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
        pfnDwmSetWindowAttribute fnSetAttr = (pfnDwmSetWindowAttribute)(void*)GetProcAddress(hDwm, "DwmSetWindowAttribute");
        if (fnSetAttr) {
            // DWMWA_WINDOW_CORNER_PREFERENCE = 33, DWMWCP_ROUND = 2
            DWORD cornerPreference = 2;
            fnSetAttr(hWnd, 33, &cornerPreference, sizeof(cornerPreference));
        }
        FreeLibrary(hDwm);
    }
}

/**
 * DPI 感知与动态尺寸缩放计算
 */
static int GetDpiForHwnd(HWND hWnd) {
    HMODULE hUser = GetModuleHandleA("user32.dll");
    if (hUser) {
        typedef UINT (WINAPI *pfnGetDpiForWindow)(HWND);
        pfnGetDpiForWindow fnGetDpi = (pfnGetDpiForWindow)(void*)GetProcAddress(hUser, "GetDpiForWindow");
        if (fnGetDpi) {
            UINT dpi = fnGetDpi(hWnd);
            if (dpi > 0) return (int)dpi;
        }
    }
    HDC hdc = GetDC(hWnd);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(hWnd, hdc);
    return dpi > 0 ? dpi : 96;
}

static int ScaleDpi(int val, int dpi) {
    return MulDiv(val, dpi, 96);
}

/**
 * 创建 ClearType 平滑抗锯齿现代字体
 */
static HFONT CreateClearTypeFont(const char* face, int ptSize, int weight) {
    HDC hdc = GetDC(NULL);
    int logPixelsY = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(NULL, hdc);
    int height = -MulDiv(ptSize, logPixelsY, 72);

    return CreateFontA(
        height, 0, 0, 0,
        weight,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        face
    );
}

/**
 * 初始化 WinUI 3 风格字体系统与 GDI 笔刷
 */
static void InitUIResources(void) {
    // 优先使用 Microsoft YaHei UI 保证中文字符的现代排版清晰度
    const char* mainFont = "Microsoft YaHei UI";
    
    g_hFontTitle     = CreateClearTypeFont(mainFont, 14, FW_SEMIBOLD);
    g_hFontSubtitle  = CreateClearTypeFont(mainFont, 9,  FW_NORMAL);
    g_hFontBadge     = CreateClearTypeFont(mainFont, 9,  FW_SEMIBOLD);
    g_hFontCardTitle = CreateClearTypeFont(mainFont, 9,  FW_SEMIBOLD);
    g_hFontHint      = CreateClearTypeFont(mainFont, 8,  FW_NORMAL);
    g_hFontCheckbox  = CreateClearTypeFont(mainFont, 9,  FW_NORMAL);
    g_hFontLog       = CreateClearTypeFont(mainFont, 9,  FW_NORMAL);
    g_hFontButton    = CreateClearTypeFont(mainFont, 9,  FW_SEMIBOLD);

    g_hBrushWindowBg = CreateSolidBrush(RGB(243, 243, 243)); // WinUI 3 Light Mica Background
    g_hBrushCardBg   = CreateSolidBrush(RGB(255, 255, 255)); // WinUI 3 White Card Surface
    g_hPenCardBorder = CreatePen(PS_SOLID, 1, RGB(229, 229, 229)); // WinUI 3 Card Border
}

/**
 * 释放 UI 资源
 */
static void CleanupUIResources(void) {
    if (g_hFontTitle)     DeleteObject(g_hFontTitle);
    if (g_hFontSubtitle)  DeleteObject(g_hFontSubtitle);
    if (g_hFontBadge)     DeleteObject(g_hFontBadge);
    if (g_hFontCardTitle) DeleteObject(g_hFontCardTitle);
    if (g_hFontHint)      DeleteObject(g_hFontHint);
    if (g_hFontCheckbox)  DeleteObject(g_hFontCheckbox);
    if (g_hFontLog)       DeleteObject(g_hFontLog);
    if (g_hFontButton)    DeleteObject(g_hFontButton);

    if (g_hBrushWindowBg) DeleteObject(g_hBrushWindowBg);
    if (g_hBrushCardBg)   DeleteObject(g_hBrushCardBg);
    if (g_hPenCardBorder) DeleteObject(g_hPenCardBorder);
}

/**
 * 按钮子类化：捕获鼠标悬浮 (Hover) 与离开 (Leave) 动态交互事件
 */
static LRESULT CALLBACK ButtonSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    WNDPROC oldProc = (WNDPROC)GetPropA(hWnd, "OLD_WNDPROC");
    switch (uMsg) {
    case WM_MOUSEMOVE: {
        if (!GetPropA(hWnd, "BTN_HOVER")) {
            SetPropA(hWnd, "BTN_HOVER", (HANDLE)1);
            TRACKMOUSEEVENT tme;
            tme.cbSize = sizeof(TRACKMOUSEEVENT);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hWnd;
            TrackMouseEvent(&tme);
            InvalidateRect(hWnd, NULL, FALSE);
        }
        break;
    }
    case WM_MOUSELEAVE: {
        RemovePropA(hWnd, "BTN_HOVER");
        InvalidateRect(hWnd, NULL, FALSE);
        break;
    }
    case WM_SETCURSOR: {
        SetCursor(LoadCursor(NULL, IDC_HAND));
        return TRUE;
    }
    case WM_NCDESTROY: {
        RemovePropA(hWnd, "BTN_HOVER");
        RemovePropA(hWnd, "OLD_WNDPROC");
        break;
    }
    }
    if (oldProc) {
        return CallWindowProcA(oldProc, hWnd, uMsg, wParam, lParam);
    }
    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

/**
 * 注册按钮子类化
 */
static void SubclassButton(HWND hBtn) {
    WNDPROC old = (WNDPROC)SetWindowLongPtrA(hBtn, GWLP_WNDPROC, (LONG_PTR)ButtonSubclassProc);
    SetPropA(hBtn, "OLD_WNDPROC", (HANDLE)old);
}

/**
 * 绘制 WinUI 3 风格现代圆角交互按钮
 */
static void DrawModernButton(const DRAWITEMSTRUCT* pDIS) {
    HDC hdc = pDIS->hDC;
    RECT rc = pDIS->rcItem;
    UINT id = pDIS->CtlID;

    bool isPressed = (pDIS->itemState & ODS_SELECTED) != 0;
    bool isHovered = (GetPropA(pDIS->hwndItem, "BTN_HOVER") != NULL);
    bool isPrimary = (id == IDC_BTN_PATCH);
    bool isExit    = (id == IDC_BTN_EXIT);

    COLORREF clrBg, clrBorder, clrText;

    if (isPrimary) {
        // WinUI 3 主视觉 Accent 按钮 (Microsoft Blue)
        if (isPressed) {
            clrBg = RGB(0, 82, 156);       // #00529C 按下深蓝
            clrBorder = RGB(0, 74, 140);
            clrText = RGB(235, 235, 235);
        } else if (isHovered) {
            clrBg = RGB(25, 117, 210);     // #1975D2 悬浮明亮蓝
            clrBorder = RGB(15, 108, 189);
            clrText = RGB(255, 255, 255);
        } else {
            clrBg = RGB(0, 103, 192);      // #0067C0 默认 WinUI Accent 蓝
            clrBorder = RGB(0, 95, 184);
            clrText = RGB(255, 255, 255);
        }
    } else if (isExit) {
        // 退出按钮 (悬浮时呈现柔和警示红)
        if (isPressed) {
            clrBg = RGB(249, 220, 220);
            clrBorder = RGB(210, 150, 150);
            clrText = RGB(168, 30, 15);
        } else if (isHovered) {
            clrBg = RGB(253, 242, 242);
            clrBorder = RGB(224, 180, 180);
            clrText = RGB(196, 43, 28);
        } else {
            clrBg = RGB(255, 255, 255);
            clrBorder = RGB(209, 209, 209);
            clrText = RGB(32, 32, 32);
        }
    } else {
        // 次级 Standard 按钮 (纯白背景 + 细腻描边)
        if (isPressed) {
            clrBg = RGB(234, 234, 234);
            clrBorder = RGB(180, 180, 180);
            clrText = RGB(20, 20, 20);
        } else if (isHovered) {
            clrBg = RGB(245, 245, 245);
            clrBorder = RGB(199, 199, 199);
            clrText = RGB(20, 20, 20);
        } else {
            clrBg = RGB(255, 255, 255);
            clrBorder = RGB(209, 209, 209);
            clrText = RGB(32, 32, 32);
        }
    }

    // 内存双缓冲绘制按钮
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hBmp = CreateCompatibleBitmap(hdc, w, h);
    HGDIOBJ hOldBmp = SelectObject(hdcMem, hBmp);

    // 填充父容器背景色消除边缘锯齿
    HBRUSH hParentBg = CreateSolidBrush(RGB(243, 243, 243));
    RECT localRc = {0, 0, w, h};
    FillRect(hdcMem, &localRc, hParentBg);
    DeleteObject(hParentBg);

    // 绘制现代圆角矩形按钮体 (Corner Radius = 6px)
    HBRUSH hBtnBrush = CreateSolidBrush(clrBg);
    HPEN hBtnPen = CreatePen(PS_SOLID, 1, clrBorder);
    HGDIOBJ hOldBrush = SelectObject(hdcMem, hBtnBrush);
    HGDIOBJ hOldPen = SelectObject(hdcMem, hBtnPen);

    RoundRect(hdcMem, 0, 0, w, h, 6, 6);

    // 绘制居中按钮文本
    char btnText[128] = {0};
    GetWindowTextA(pDIS->hwndItem, btnText, sizeof(btnText));

    SetBkMode(hdcMem, TRANSPARENT);
    SetTextColor(hdcMem, clrText);
    HGDIOBJ hOldFont = SelectObject(hdcMem, g_hFontButton);

    RECT textRc = localRc;
    if (isPressed) {
        // 轻微下沉触感反馈
        OffsetRect(&textRc, 0, 1);
    }
    DrawTextA(hdcMem, btnText, -1, &textRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // 贴图到目标 DC
    BitBlt(hdc, rc.left, rc.top, w, h, hdcMem, 0, 0, SRCCOPY);

    // 释放 GDI 句柄
    SelectObject(hdcMem, hOldFont);
    SelectObject(hdcMem, hOldBrush);
    SelectObject(hdcMem, hOldPen);
    DeleteObject(hBtnBrush);
    DeleteObject(hBtnPen);
    SelectObject(hdcMem, hOldBmp);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
}

// 窗口过程函数
static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        g_Dpi = GetDpiForHwnd(hWnd);

        // 允许非提权测试工具跨 UIPI 发送 WM_CLOSE 优雅关闭消息
        HMODULE hUser32 = GetModuleHandleA("user32.dll");
        if (hUser32) {
            typedef BOOL (WINAPI *pfnChangeWindowMessageFilter)(UINT, DWORD);
            pfnChangeWindowMessageFilter fnFilter = (pfnChangeWindowMessageFilter)(void*)GetProcAddress(hUser32, "ChangeWindowMessageFilter");
            if (fnFilter) {
                fnFilter(WM_CLOSE, 1);
            }
        }

        // 重新计算 DPI 适配尺寸
        int padX      = ScaleDpi(g_PadX, g_Dpi);
        int optCardY  = ScaleDpi(g_OptCardY, g_Dpi);
        int logCardY  = ScaleDpi(g_LogCardY, g_Dpi);
        int logCardH  = ScaleDpi(g_LogCardH, g_Dpi);
        int btnY      = ScaleDpi(g_BtnY, g_Dpi);
        int btnH      = ScaleDpi(g_BtnH, g_Dpi);

        RECT clientRc;
        GetClientRect(hWnd, &clientRc);
        int totalW = clientRc.right - clientRc.left;
        if (totalW <= 0) totalW = ScaleDpi(g_WinW, g_Dpi);

        int cardW = totalW - 2 * padX;

        // 1. 汉化勾选框 (置于选项卡片内)
        g_hChkLocalize = CreateWindowExA(
            0, "BUTTON", "启用全量 AI 模型与界面深度汉化 (推荐)",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            padX + ScaleDpi(14, g_Dpi), optCardY + ScaleDpi(8, g_Dpi),
            cardW - ScaleDpi(28, g_Dpi), ScaleDpi(20, g_Dpi),
            hWnd, (HMENU)IDC_CHK_LOCALIZE, g_hInstance, NULL
        );
        SendMessageA(g_hChkLocalize, WM_SETFONT, (WPARAM)g_hFontCheckbox, TRUE);
        SendMessageA(g_hChkLocalize, BM_SETCHECK, BST_CHECKED, 0);

        // 2. 日志 Edit 控件 (置于日志卡片内，消除老式立体边框)
        int editX = padX + ScaleDpi(10, g_Dpi);
        int editY = logCardY + ScaleDpi(32, g_Dpi);
        int editW = cardW - ScaleDpi(20, g_Dpi);
        int editH = logCardH - ScaleDpi(40, g_Dpi);

        g_hEditLog = CreateWindowExA(
            0, "EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            editX, editY, editW, editH,
            hWnd, (HMENU)IDC_EDIT_LOG, g_hInstance, NULL
        );
        SendMessageA(g_hEditLog, WM_SETFONT, (WPARAM)g_hFontLog, TRUE);

        // 3. 底部 WinUI 3 风格交互按钮组
        int btnPatchW   = ScaleDpi(160, g_Dpi);
        int btnRestoreW = ScaleDpi(120, g_Dpi);
        int btnAboutW   = ScaleDpi(96, g_Dpi);
        int btnExitW    = ScaleDpi(96, g_Dpi);
        int spacing     = ScaleDpi(10, g_Dpi);

        int curX = padX;
        HWND hBtnPatch = CreateWindowExA(
            0, "BUTTON", "应用补丁与汉化",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
            curX, btnY, btnPatchW, btnH,
            hWnd, (HMENU)IDC_BTN_PATCH, g_hInstance, NULL
        );
        SubclassButton(hBtnPatch);

        curX += btnPatchW + spacing;
        HWND hBtnRestore = CreateWindowExA(
            0, "BUTTON", "一键还原备份",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
            curX, btnY, btnRestoreW, btnH,
            hWnd, (HMENU)IDC_BTN_RESTORE, g_hInstance, NULL
        );
        SubclassButton(hBtnRestore);

        curX += btnRestoreW + spacing;
        HWND hBtnAbout = CreateWindowExA(
            0, "BUTTON", "关于",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
            curX, btnY, btnAboutW, btnH,
            hWnd, (HMENU)IDC_BTN_ABOUT, g_hInstance, NULL
        );
        SubclassButton(hBtnAbout);

        curX += btnAboutW + spacing;
        HWND hBtnExit = CreateWindowExA(
            0, "BUTTON", "退出",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
            curX, btnY, btnExitW, btnH,
            hWnd, (HMENU)IDC_BTN_EXIT, g_hInstance, NULL
        );
        SubclassButton(hBtnExit);

        ApplyModernDwmAttributes(hWnd);
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT clientRc;
        GetClientRect(hWnd, &clientRc);
        int w = clientRc.right - clientRc.left;
        int h = clientRc.bottom - clientRc.top;

        // 双缓冲渲染
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
        HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

        // 1. 全局背景填充 (#F3F3F3)
        FillRect(memDC, &clientRc, g_hBrushWindowBg);

        // 2. 绘制顶部 Header 区域 (#FFFFFF 卡片底色 + 1px 分隔线)
        int headerH = ScaleDpi(g_HeaderH, g_Dpi);
        int padX    = ScaleDpi(g_PadX, g_Dpi);

        RECT headerRc = {0, 0, w, headerH};
        FillRect(memDC, &headerRc, g_hBrushCardBg);

        HPEN hSepPen = CreatePen(PS_SOLID, 1, RGB(229, 229, 229));
        HGDIOBJ oldPen = SelectObject(memDC, hSepPen);
        MoveToEx(memDC, 0, headerH, NULL);
        LineTo(memDC, w, headerH);

        // 主标题: Topaz Photo AI 增强与汉化工具
        SetBkMode(memDC, TRANSPARENT);
        SetTextColor(memDC, RGB(26, 26, 26));
        HGDIOBJ oldFont = SelectObject(memDC, g_hFontTitle);
        RECT titleRc = {padX, ScaleDpi(12, g_Dpi), w - padX - ScaleDpi(120, g_Dpi), ScaleDpi(36, g_Dpi)};
        DrawTextA(memDC, "Topaz Photo AI 增强与汉化工具", -1, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // 版本徽章 Pill Badge (v1.1.2 增强版)
        int badgeW = ScaleDpi(110, g_Dpi);
        int badgeH = ScaleDpi(22, g_Dpi);
        RECT badgeRc = {w - padX - badgeW, ScaleDpi(15, g_Dpi), w - padX, ScaleDpi(15, g_Dpi) + badgeH};

        HBRUSH hBadgeBrush = CreateSolidBrush(RGB(235, 243, 252));
        HPEN hBadgePen = CreatePen(PS_SOLID, 1, RGB(204, 224, 248));
        SelectObject(memDC, hBadgeBrush);
        SelectObject(memDC, hBadgePen);
        RoundRect(memDC, badgeRc.left, badgeRc.top, badgeRc.right, badgeRc.bottom, 10, 10);

        SetTextColor(memDC, RGB(0, 95, 184));
        SelectObject(memDC, g_hFontBadge);
        DrawTextA(memDC, "v1.1.2 增强版", -1, &badgeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DeleteObject(hBadgeBrush);
        DeleteObject(hBadgePen);

        // 副标题描述
        SetTextColor(memDC, RGB(100, 100, 100));
        SelectObject(memDC, g_hFontSubtitle);
        RECT subRc = {padX, ScaleDpi(38, g_Dpi), w - padX, ScaleDpi(58, g_Dpi)};
        DrawTextA(memDC, "离线免登录授权 · 抹除遥测追踪 · 73款AI深度学习模型全量中文汉化", -1, &subRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // 3. 绘制选项卡片 (Options Card)
        int optY = ScaleDpi(g_OptCardY, g_Dpi);
        int optH = ScaleDpi(g_OptCardH, g_Dpi);
        int cardW = w - 2 * padX;
        RECT optCardRc = {padX, optY, padX + cardW, optY + optH};

        SelectObject(memDC, g_hBrushCardBg);
        SelectObject(memDC, g_hPenCardBorder);
        RoundRect(memDC, optCardRc.left, optCardRc.top, optCardRc.right, optCardRc.bottom, 8, 8);

        // 选项卡片次级提示文本
        SetTextColor(memDC, RGB(120, 120, 120));
        SelectObject(memDC, g_hFontHint);
        RECT hintRc = {padX + ScaleDpi(34, g_Dpi), optY + ScaleDpi(30, g_Dpi), padX + cardW - ScaleDpi(10, g_Dpi), optY + optH - ScaleDpi(4, g_Dpi)};
        DrawTextA(memDC, "已包含 Denoise, Sharpen, Upscale, Face Recovery 等 73 款深度模型中文描述及主面板", -1, &hintRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // 4. 绘制日志卡片 (Log Card)
        int logY = ScaleDpi(g_LogCardY, g_Dpi);
        int logH = ScaleDpi(g_LogCardH, g_Dpi);
        RECT logCardRc = {padX, logY, padX + cardW, logY + logH};

        SelectObject(memDC, g_hBrushCardBg);
        SelectObject(memDC, g_hPenCardBorder);
        RoundRect(memDC, logCardRc.left, logCardRc.top, logCardRc.right, logCardRc.bottom, 8, 8);

        // 日志卡片头部指示灯 (Fluent Green Dot)
        HBRUSH hDotBrush = CreateSolidBrush(RGB(16, 124, 65));
        HPEN hDotPen = CreatePen(PS_SOLID, 1, RGB(16, 124, 65));
        SelectObject(memDC, hDotBrush);
        SelectObject(memDC, hDotPen);
        int dotX = padX + ScaleDpi(14, g_Dpi);
        int dotY = logY + ScaleDpi(11, g_Dpi);
        int dotR = ScaleDpi(4, g_Dpi);
        Ellipse(memDC, dotX, dotY, dotX + 2 * dotR, dotY + 2 * dotR);
        DeleteObject(hDotBrush);
        DeleteObject(hDotPen);

        // 日志卡片标题
        SetTextColor(memDC, RGB(50, 49, 48));
        SelectObject(memDC, g_hFontCardTitle);
        RECT cardTitleRc = {dotX + ScaleDpi(12, g_Dpi), logY + ScaleDpi(4, g_Dpi), padX + cardW - ScaleDpi(10, g_Dpi), logY + ScaleDpi(26, g_Dpi)};
        DrawTextA(memDC, "运行状态与实时日志", -1, &cardTitleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // 日志卡片头部内部分隔线
        SelectObject(memDC, hSepPen);
        MoveToEx(memDC, padX + 1, logY + ScaleDpi(26, g_Dpi), NULL);
        LineTo(memDC, padX + cardW - 1, logY + ScaleDpi(26, g_Dpi));

        // 贴图呈现
        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

        // 释放 GDI 内存
        SelectObject(memDC, oldFont);
        SelectObject(memDC, oldPen);
        DeleteObject(hSepPen);
        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1; // 双缓冲接管消除画面闪烁

    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)wParam;
        HWND hwndStatic = (HWND)lParam;
        if (hwndStatic == g_hEditLog || hwndStatic == g_hChkLocalize) {
            SetTextColor(hdcStatic, RGB(36, 36, 36));
            SetBkColor(hdcStatic, RGB(255, 255, 255));
            return (INT_PTR)g_hBrushCardBg;
        }
        SetBkMode(hdcStatic, TRANSPARENT);
        return (INT_PTR)g_hBrushWindowBg;
    }

    case WM_CTLCOLOREDIT: {
        HDC hdcEdit = (HDC)wParam;
        SetTextColor(hdcEdit, RGB(36, 36, 36));
        SetBkColor(hdcEdit, RGB(255, 255, 255));
        return (INT_PTR)g_hBrushCardBg;
    }

    case WM_DRAWITEM: {
        DrawModernButton((const DRAWITEMSTRUCT*)lParam);
        return TRUE;
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
                "Topaz Photo AI 增强与全量汉化工具 v1.1.2\n\n"
                "核心功能特性：\n"
                "1. 离线免登录全量授权通过 (永久生效)\n"
                "2. 屏蔽新版本强制升级提示弹窗\n"
                "3. 彻底抹除 Amplitude / Backtrace 遥测与崩溃上报\n"
                "4. 包含 73 款 AI 深度学习模型全量中文汉化描述\n"
                "5. 包含右侧控制面板与核心界面菜单中文化\n"
                "6. 进程冲突检测与安装路径双向智能感知保护\n"
                "7. 自动原子备份与一键原样还原保障\n\n"
                "界面风格：WinUI 3 Fluent Design 现代轻量化引擎",
                "关于本工具",
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
    InitUIResources();

    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = g_hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = g_hBrushWindowBg;
    wc.lpszClassName = "Dup2PatcherClass";

    RegisterClassExA(&wc);

    // 计算居中窗口坐标
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int winW = g_WinW;
    int winH = g_WinH;
    int posX = (screenW - winW) / 2;
    int posY = (screenH - winH) / 2;

    g_hDlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "Dup2PatcherClass",
        "Topaz Photo AI 增强与全量汉化工具",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        posX, posY, winW, winH,
        NULL, NULL, g_hInstance, NULL
    );

    if (!g_hDlg) {
        CleanupUIResources();
        return;
    }

    ShowWindow(g_hDlg, SW_SHOW);
    UpdateWindow(g_hDlg);

    AddMsg("[*] 欢迎使用 Topaz Photo AI 增强与中文汉化工具 (v1.1.2)");
    AddMsg("[*] 点击【应用补丁与汉化】执行修补，或【一键还原备份】回退。");

    // 消息循环
    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    CleanupUIResources();
}

/**
 * 模块入口 DllMain (DLL 模式)
 */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)lpvReserved;
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
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;
    g_hInstance = hInstance;
    InitCommonControls();
    load_patcher();
    ExitProcess(0);
    return 0;
}
#endif
