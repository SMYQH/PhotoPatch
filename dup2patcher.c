#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "zh_models_data.h"

/**
 * dup2patcher.c - v1.1.2 (Production Release)
 * 
 * Topaz Photo AI 生产级修补与汉化增强引擎
 * 包含：Win32 原生 GUI、进程冲突检测、安装路径自适应探测、特征码内存补丁、
 *      全量遥测切断与 73 款 AI 模型全量中文汉化。
 */

// 控件 ID 定义
#define IDC_BTN_PATCH     1001
#define IDC_BTN_RESTORE   1002
#define IDC_BTN_ABOUT     1003
#define IDC_BTN_EXIT      1004
#define IDC_EDIT_LOG      1005
#define IDC_CHK_LOCALIZE  1006

// 全局变量
static HWND g_hDlg = NULL;
static HWND g_hEditLog = NULL;
static HWND g_hChkLocalize = NULL;
static HINSTANCE g_hInstance = NULL;

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
    AddMsg("========================================");
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
    AddMsg("========================================");
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
        AddMsg("----------------------------------------");
        AddMsg("[*] 正在应用中文语言包与模型汉化 ...");
        
        // 2. 汉化 Topaz Photo AI.exe 界面关键菜单
        if (GetFileAttributesA("Topaz Photo AI.exe") != INVALID_FILE_ATTRIBUTES) {
            AddMsg("[*] 正在修补主程序界面词条: Topaz Photo AI.exe ...");
            PatchBinaryFile("Topaz Photo AI.exe", g_ExeRules, EXE_RULE_COUNT);
        }

        // 3. 部署 73 个 AI 模型的全量中文描述符
        ApplyModelsLocalization();
    }

    AddMsg("========================================");
    AddMsg("[+] 全部操作执行完毕！");
    AddMsg("[*] 离线免登录授权、遥测切断与中文汉化已全部就绪。");
}

// 窗口过程函数
static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        // 创建日志 Edit 控件
        g_hEditLog = CreateWindowExA(
            WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            10, 10, 420, 140,
            hWnd, (HMENU)IDC_EDIT_LOG, g_hInstance, NULL
        );

        // 创建汉化勾选框 (默认选中)
        g_hChkLocalize = CreateWindowExA(
            0, "BUTTON", "启用完整中文汉化 (73款AI模型+控制面板+界面菜单)",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            12, 158, 415, 20,
            hWnd, (HMENU)IDC_CHK_LOCALIZE, g_hInstance, NULL
        );
        SendMessageA(g_hChkLocalize, BM_SETCHECK, BST_CHECKED, 0);

        // 按钮组
        CreateWindowExA(
            0, "BUTTON", "应用补丁与汉化",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 188, 120, 30,
            hWnd, (HMENU)IDC_BTN_PATCH, g_hInstance, NULL
        );

        CreateWindowExA(
            0, "BUTTON", "还原原始备份",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            140, 188, 100, 30,
            hWnd, (HMENU)IDC_BTN_RESTORE, g_hInstance, NULL
        );

        CreateWindowExA(
            0, "BUTTON", "关于",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            250, 188, 85, 30,
            hWnd, (HMENU)IDC_BTN_ABOUT, g_hInstance, NULL
        );

        CreateWindowExA(
            0, "BUTTON", "退出",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            345, 188, 85, 30,
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
                "Topaz Photo AI 增强与全量汉化工具 v1.1.2\n\n"
                "核心功能：\n"
                "1. 离线免登录全量授权通过 (永久生效)\n"
                "2. 屏蔽新版本强制升级提示弹窗\n"
                "3. 彻底抹除 Amplitude/Backtrace 遥测与崩溃上报\n"
                "4. 包含 73 款 AI 深度学习模型全量中文汉化\n"
                "5. 包含右侧控制面板与核心界面菜单中文化\n"
                "6. 进程冲突与安装路径智能感知保护\n"
                "7. 自动全量备份与一键原样还原保障\n",
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
    int winW = 455;
    int winH = 270;
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

    if (!g_hDlg) return;

    ShowWindow(g_hDlg, SW_SHOW);
    UpdateWindow(g_hDlg);

    AddMsg("[*] 欢迎使用 Topaz Photo AI 增强与中文汉化工具 (v1.1.2)");
    AddMsg("[*] 点击【应用补丁与汉化】执行修补，或【还原原始备份】回退。");

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
    return 0;
}
#endif

