# Codex Quota 1.1.0

TrafficMonitor 的 Codex 真实账号额度插件，四个单行显示项、两个双行显示项。源码和 x86/x64 DLL 均已包含。

## 六个显示项

| 在显示设置中的名称 | 内容示例 | 类型 |
| --- | --- | --- |
| 5h额度 | `5h: 82.0%` | 单行 |
| 周额度 | `week: 77.0%` | 单行 |
| 5h刷新时间 | `5h reset: 0d 3h` | 单行 |
| 周额度刷新时间 | `week reset: 6d 14h` | 单行 |
| 5h两行块 | 上行 `5h 82.0%`，下行 `0d 3h` | 双行自绘 |
| 周额度两行块 | 上行 `week 77.0%`，下行 `6d 14h` | 双行自绘 |

显示项名称保留用户指定的中文名称；实际周额度标签统一使用 `week`。四个单行项和两个组合块可以任意选择，通常选四个单行项或两个组合块，避免重复显示。

“刷新时间”表示服务端额度重置倒计时，不是插件下次请求的时间。倒计时始终按 `xd xh` 显示，按整天、整小时向下取整；不足一小时为 `0d 0h`。悬浮提示保留准确的本地重置时刻。重置时刻已到时，额度等待服务端新数据，不自行变为 100%。

## 安装与升级

当前已检查到的用户 TrafficMonitor 1.86 是 **32 位**，请使用 **bin/x86/CodexQuota.dll**。

1. 退出 TrafficMonitor。
2. 将对应程序位数的 DLL 复制到 TrafficMonitor 的 `plugins` 文件夹，替换旧 `CodexQuota.dll`。不要将两种位数的 DLL 同时放入该文件夹。
3. 重启，在显示设置中选择需要的项目。
4. 原有两个额度项 ID 保持不变，配置文件和查询间隔沿用原设置。

依据 TrafficMonitor 程序位数选择 DLL，而不是依据 Windows 位数。x64 DLL 无法加载到 x86 TrafficMonitor，会产生 Windows 错误 193。

## 2×2 布局与版本兼容

**四个单行项**：在双行排列下，把 `5h额度`、`5h刷新时间` 相邻排列，再把 `周额度`、`周额度刷新时间` 相邻排列，可形成左侧 5h、右侧 week 的两列。

**两个双行块**：在 TrafficMonitor **1.86** 中开启任务栏窗口设置的 **水平排列**，选择 `5h两行块`、`周额度两行块`，两个块内部各自绘制两行，可并排形成 2×2。字体自动适应主程序分配的高度，空间较小时字号会缩小。

TrafficMonitor 官方 **V1.86 发布标签尚无 IsDoubleLineExclusive 布局支持**，不能由 DLL 强制要求它为单个显示项分配正常双行高度。1.1.0 同时实现了最新接口的 `IsDoubleLineExclusive()`；支持此接口的新版主程序可以在普通双行布局中让组合块独占两行。不会为了兼容强行画出主程序分配区域，也不会修改主程序。

主悬浮窗由皮肤定义坐标及大小，组合块所在的皮肤区域需要留出两行高度。

## 数据和刷新设置

通过官方 `codex app-server` 的 `account/rateLimits/read` 查询本地已登录 ChatGPT 账号，不需要手填额度，不用本地 Token 用量估算。所有六项共享一次请求的数据，不会因为启用更多项而重复请求。按服务器窗口长度 300/10080 分钟区分 5h/week；不假设 primary 一定代表 5h。

- `N/A`：接口没有返回相应窗口或重置时刻。
- `...`：首次请求中，或额度重置后等待服务端更新。
- `ERR`：查询失败，提示信息中给出原因，下一周期重试。
- `STALE`：数据超过刷新间隔加超时宽限，不能作为当前额度。

插件管理 → Codex Quota → 选项：可按秒输入 5–1800，或按分钟输入 1–30，默认 60 秒。另可设置原生 codex.exe 绝对路径；留空自动查找桌面版、PATH 和 npm 原生程序。查询期间不会叠加请求。

设置仍保存在 `%APPDATA%/TrafficMonitor/CodexQuota.ini`；无用户设置时读取 DLL 同目录的样例配置。程序使用 Codex 自己保存的登录状态，不将账号令牌写进插件配置或交付包。需要 ChatGPT 登录，API Key 登录不支持订阅额度。查询不启动模型对话、不使用重置券。

## 构建与测试

运行 `build.cmd`，需要 Visual Studio C++ Build Tools（x86/x64 MSVC、Windows SDK）。通过 vswhere 定位工具链，静态链接 C++ 运行库。构建自动运行解析、六项 ABI、倒计时和绘图边界测试。

实测结果：两个架构编译、加载及设置测试通过；x86 真实账号读取周剩余 `77.0%`、倒计时 `6d 14h`，5h 未返回时相关三项均显示 N/A。以上是测试快照，并非代码默认值。GDI 渲染验证了双行绘制在 18/28/40/60 像素高度下均不越界，并检查了实际渲染图。

独立验证真实数据：

```powershell
.\bin\x86\QuotaTests.exe "$PWD\bin\x86\CodexQuota.dll" --live
```

测试宿主验证了接口与渲染，没有替换或重启用户的 TrafficMonitor，没有声称已经在所有皮肤和桌面缩放比例下完成测试。安装仅需要 DLL，其余文件用于源码审查、重建和验证。

## 上游来源

- TrafficMonitor 插件规范：https://github.com/zhongyang219/TrafficMonitor/wiki/Plugin-Development-Guide
- 最新接口：https://github.com/zhongyang219/TrafficMonitor/blob/master/include/PluginInterface.h
- V1.86 布局：https://github.com/zhongyang219/TrafficMonitor/blob/V1.86/TrafficMonitor/TaskBarDlg.cpp
- Codex 账号接口：https://learn.chatgpt.com/docs/app-server

原创代码 MIT；TrafficMonitor 接口头文件保留上游 Anti-996 许可；nlohmann/json 3.11.3 保留 MIT 许可。详见随包许可文件。
