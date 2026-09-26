# Codex Quota 1.2.2

TrafficMonitor 原生插件，通过本地已登录的 Codex app-server 读取真实账号额度，不使用手填总额或本地用量估算。

## 安装

退出 TrafficMonitor，将对应架构的 `bin/x86/CodexQuota.dll` 或 `bin/x64/CodexQuota.dll` 复制到 TrafficMonitor 的 plugins 文件夹，再启动。
本机 TrafficMonitor 1.86 是 32 位，使用 x86 DLL。插件运行不需要 Visual Studio 或额外 C++ 运行库。
需要可运行且已使用 ChatGPT 账号登录的原生 codex.exe；支持自动检测或在插件选项中指定路径。

## 显示和设置

提供六个可独立选择的显示项：5h额度、周额度、5h刷新时间、周额度刷新时间、5h两行块、周额度两行块。
四个单行项和两个双行块可按需选择。TrafficMonitor 1.86 显示两个块时使用“水平排列”。
额度显示整数，格式如 `week: 76%`；倒计时格式为 `6d 13h`。悬浮提示只保留两个窗口的查询结果。
设置支持显示剩余量或使用量。告警始终根据服务器实际使用量判断：

| 实际使用量 | 样式 |
| --- | --- |
| ≤80% | 普通样式 |
| >80% 且 ≤90% | 黄底白字 |
| >90% | 红底白字 |

阈值使用未取整的数值，因此实际 90.1% 会显示整数 90% 并呈红底。失效、缺失或已到重置时刻的数据不触发告警。
查询间隔可选 5–1800 秒或 1–30 分钟，默认 60 秒，请求不重叠。
设置保存于 `%APPDATA%/TrafficMonitor/CodexQuota.ini`；所有显示项共享一次查询。

## 诊断日志

插件菜单可打开 `%LOCALAPPDATA%/TrafficMonitor/CodexQuota.log`。
日志记录查询阶段、耗时和错误类别，不记录令牌、账号标识、原始响应或代理地址。
超过约 1 MiB 轮换为 `.log.1`，最多保留当前文件和一份备份。

## 构建

运行 `build.cmd`。需要 MSVC x86/x64 C++ Build Tools 和 Windows SDK；可用 VSROOT 指定安装位置，默认通过 vswhere 检测；本项目也会优先使用 `.tools/msvc/setup_x86.bat` 和 `setup_x64.bat`，或使用 PORTABLE_MSVC 指定的便携工具链。
脚本构建两种架构并运行额度解析、插件加载/卸载、六项接口、倒计时、阈值和 GDI 像素测试。
源码位于项目根目录 src；历史版本文件夹和 zip 保留为旧交付存档，不进入 Git。
Git 的 v1.0.0–v1.2.1 标签对应按已有源码快照恢复的四批提交；提交时间为本次归档时间。

## 许可

原创代码使用 MIT。TrafficMonitor 接口头文件及 nlohmann/json 保留随包上游许可。
