# Codex Quota for TrafficMonitor

读取 Codex 已登录 ChatGPT 账号的真实剩余额度。提供两个独立的 TrafficMonitor 显示项：

- **Codex 5h 剩余额度**：300 分钟窗口的剩余百分比。
- **Codex 周剩余额度**：10080 分钟窗口的剩余百分比。

可分别启用、排序，并在支持插件项的皮肤中分别定位。插件本身不限制两项必须相邻；具体布局能力由 TrafficMonitor 和使用的皮肤决定。

## 安装

1. 退出 TrafficMonitor。
2. 根据 **TrafficMonitor 程序的位数**，选择 `bin/x64/CodexQuota.dll` 或 `bin/x86/CodexQuota.dll`，复制到 TrafficMonitor 程序目录下的 `plugins` 文件夹。只复制一个版本。
3. 启动 TrafficMonitor，在“其他功能 → 插件管理”中确认 **Codex Quota** 已加载。
4. 在主窗口或任务栏窗口的显示项设置中，分别启用两个 Codex 显示项，调整顺序或皮肤位置。
5. “插件管理 → Codex Quota → 选项”可设置查询间隔及可选的 Codex 程序路径。

需要 Windows 10/11、支持此接口的 TrafficMonitor（随包使用官方 API v8 头文件；旧版不识别时请更新 TrafficMonitor），以及支持 `app-server` 的 Codex。DLL 静态链接 C++ 运行库，不需要额外安装 VC++ Redistributable。无需 Python、Node.js、PowerShell 脚本或其他伴随程序。

Codex 必须已使用 ChatGPT 账号登录；API Key 登录不能提供该订阅额度。如果插件报告登录失败，请在相同 Windows 用户、相同 CODEX_HOME 下完成 `codex login`。查询的账号是该 Codex 本地登录账号，不是其他浏览器账号或远程主机账号。

## 刷新设置

原生设置窗口支持：

- 单位选“秒”：输入 **5–1800**，例如 5、17、90。
- 单位选“分钟”：输入 **1–30**，例如 1、5、30。
- 默认 60 秒；保存后立即重新查询。请求进行中不会并发叠加。
- 查询完成后等待设定间隔再发起下一次查询；实际显示更新也受 TrafficMonitor 调用周期和网络耗时影响。

设置保存在 `%APPDATA%/TrafficMonitor/CodexQuota.ini`，无需向 TrafficMonitor 安装目录写入。用户配置不存在时，读取 DLL 同目录的同名 INI；都不存在时自动使用默认值。随包 INI 仅为可选样例，不需要填写额度。

```ini
[CodexQuota]
CodexPath=
RefreshSeconds=60
```

`CodexPath` 留空时依次检测常见 Codex 桌面版安装位置、PATH 中的原生 codex.exe 和标准 npm 包目录。不在这些位置的安装，可在设置窗口指定实际 `codex.exe` 的绝对路径；不能填 `codex.cmd`。

## 数据含义

插件通过本地标准输入输出启动官方 `codex app-server`，完成 `initialize` / `initialized` 后调用 `account/rateLimits/read`。不启动对话、不调用模型、不消耗重置券。

优先使用 `rateLimitsByLimitId.codex`；老接口仅提供 `rateLimits` 时使用该字段。按 `windowDurationMins` 识别窗口，**不假设 primary 一定是 5 小时**。剩余百分比为服务器实际返回的 `100 - usedPercent`，限定在 0–100；不是根据本地 Token 数、套餐价格或手填总额推算。

悬浮提示含上次成功查询时间和两个窗口的本地重置时间。

| 显示 | 含义 |
| --- | --- |
| `79.0%` | 服务器返回的真实剩余比例 |
| `N/A` | 接口没有返回对应窗口，不能理解为 0% 或 100% |
| `...` | 正在首次查询，或重置时间已到、等待服务器新数据 |
| `ERR` | 查询失败；悬浮提示说明原因，下个周期自动重试 |
| `STALE` | 超过刷新间隔加 45 秒未获得有效更新 |

网络/权限/登录失败时清除有效数值状态，不把旧数据伪装成当前额度。缺失窗口永远不自行估算。刷新不会根据倒计时自动填成 100%。

## 构建与验证

安装 Visual Studio C++ Build Tools（MSVC x86/x64、Windows SDK），在 Windows 中运行 `build.cmd`。脚本使用 vswhere 定位工具链，构建两个架构，并自动运行额度解析测试。源代码和全部编译依赖头文件均包含在 `src` 中，不需要联网下载依赖。

`QuotaSmoke.exe` 是按官方 C++ ABI 加载 DLL 的验证程序：

```powershell
.\bin\x64\QuotaSmoke.exe "$PWD\bin\x64\CodexQuota.dll"
```

该命令查询当前真实账号。`SettingsTests.exe` 的第二个参数是测试专用 APPDATA 目录，以免改动实际用户设置。测试文件和可执行程序仅供验证，安装时只需要 DLL。

已完成：x64/x86 编译、额度解析测试、DLL 导出和双显示项加载、两个架构的真实账号读取、两个架构的设置窗口保存及边界校验、控件范围检查，以及程序路径缺失时的错误状态测试。测试时账号只返回周窗口，初次显示 `N/A`、`79.0%`，最后一次显示 `N/A`、`78.0%`；这些是测试时快照，不是代码内置值。未将 DLL 安装进用户正在运行的 TrafficMonitor，也未声称验证过其所有皮肤布局。

## 来源与许可

- TrafficMonitor 官方开发规范：https://github.com/zhongyang219/TrafficMonitor/wiki/Plugin-Development-Guide
- 官方接口头文件：https://github.com/zhongyang219/TrafficMonitor/blob/master/include/PluginInterface.h
- Codex 官方账号接口：https://learn.chatgpt.com/docs/app-server
- JSON 库：nlohmann/json 3.11.3。

原创实现按 MIT 发布，见 LICENSE。TrafficMonitor 接口头文件保留原作者版权及其上游 Anti-996 许可（TrafficMonitor-LICENSE）；nlohmann/json 使用其 MIT 许可（nlohmann-json-LICENSE）。
