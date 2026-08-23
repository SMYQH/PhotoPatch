# Topaz Photo AI 修补、增强与全量汉化工程 (x64 Production-Ready)

## 🌟 核心特性

1. **免登录授权**：强制 `TAuth` 鉴权状态返回成功状态（`AuthState=2`），任意账户或离线状态均可使用。
2. **跳过未登录与离线检测**：绕过 `useCachedCredentials` 的离线判定分支，防止离线超时失效。
3. **消除校验失败分支**：移除鉴权异常跳转，失败分支自动重定向至成功处理逻辑。
4. **屏蔽升级弹窗**：拦截版本对比逻辑，屏蔽强制升级提醒。
5. **全量遥测与崩溃上报截断**：
   - 入口级拦截 `TEventTracker::track` 与 `trackBacktrace`，直接返回 True 避免多余网络请求。
   - 彻底清空 Backtrace 崩溃诊断上报地址 (`events.backtrace.io`)。
   - 彻底清空 Amplitude 行为与画像追踪接口 (`api2.amplitude.com`, `api.lab.amplitude.com`, `profile-api.amplitude.com`)。
   - 清空旧版遥测域名 (`et.topazlabs.com`)。
6. **73 款 AI 模型与控制面板全量中文汉化**：
   - 包含全部 73 款 AI 深度学习模型名称、算法描述、滑块控制项与状态动词全量翻译。
   - 主程序核心菜单与状态条目中文化，打造原生中文摄影降噪/锐化工作流。
7. **自动备份与容灾恢复**：
   - 首次修补自动生成 `network.dll.bak`、`Topaz Photo AI.exe.bak` 与全部模型描述符 `*.json.bak`。
   - 支持 GUI 界面一键原样还原。
8. **纯 64 位单文件绿色架构**：
   - 原生 x86_64 编译，独立单文件程序（无需外挂 JSON 或中间 Dropper 落地，零杀软误报）。

---

## 🚀 快速使用

1. 安装 **Photo.msi**，安装完成后彻底退出软件。
2. 将构建生成的 `Photo Patch.exe` 复制到软件安装根目录下（默认路径为：`C:\Program Files\Topaz Labs LLC\Topaz Photo AI`）。
3. 右键选择 **以管理员身份运行** `Photo Patch.exe`。
4. 勾选 **启用完整中文汉化**（默认已勾选）。
5. 点击 **应用补丁与汉化** 按钮（程序会自动备份原始文件并应用 12 组增强规则与 73 款模型中文描述符）。
6. 看到日志提示修补成功后即可退出工具。
7. 打开 Topaz Photo AI 即可正常离线使用中文界面。如需回退，可点击 **还原原始备份** 恢复原版文件。

---

## 💡 生产环境与离线 AI 模型部署说明

> [!IMPORTANT]
> Topaz Photo AI 采用按需下载 AI 模型的机制。基础安装包不含全部硬件后端的模型权重。
> 若在**完全断网 / 纯离线生产环境**下运行：
> 1. 请在联网环境下先打开软件并加载常用处理功能（RAW 去噪、面部修复、超分辨率等），让软件自动下载对应的 AI 模型权重到本地缓存目录。
> 2. 模型默认存储路径：
>    - `C:\ProgramData\Topaz Labs LLC\Topaz Photo AI\models`
> 3. 将下载好的 `models` 目录整体打包拷贝到离线生产机对应目录，即可实现 100% 离线 AI 图像处理。

---

## 🛠️ 构建指南

项目支持在 Linux / WSL 环境下通过 MinGW-w64 交叉编译为原生 Windows 64 位二进制。

### 1. 环境准备

```bash
apt update
apt install -y gcc gcc-mingw-w64-x86-64 binutils-mingw-w64-x86-64
```

### 2. 执行一键构建

```bash
chmod +x build.sh
./build.sh
```

构建产物：
- `Photo Patch.exe`：独立完整单文件 GUI 补丁与汉化程序（生产首选，零 Dropper 报毒风险）。
- `dup2patcher.dll`：64 位核心补丁动态链接库。

---

> 本项目仅用于逆向工程、安全审计与软件架构学习研究。
