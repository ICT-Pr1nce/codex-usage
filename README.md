# Codex Quota 1.2.0

## 更新

- 悬浮提示仅保留 5h、week 两行查询结果，附重置倒计时；移除标题、接口、查询时刻和说明。
- 百分比去掉小数（直接截去小数部分），周标签统一为 `week:`，例如 `week: 76%`。
- 插件选项新增“显示内容”：剩余量 / 使用量。默认剩余量，保存立即生效。
- 实际使用量 **>80%** 时黄底黑字，**>=100%** 时红底白字。80% 不告警。
- 颜色始终依据实际使用量，与显示剩余量还是使用量无关。失效、缺失或已到重置时刻的数据不触发告警。
- 六个显示项均保留原 ID；四个单行项、两个双行块均支持底色告警。

## 安装

此用户当前 TrafficMonitor 是 x86（32 位），使用 `bin/x86/CodexQuota.dll`。
退出 TrafficMonitor，将 DLL 替换到其 plugins 文件夹，再重启。
64 位 TrafficMonitor 使用 `bin/x64/CodexQuota.dll`。安装只需 DLL。

“插件管理 → Codex Quota → 选项 → 显示内容”选择使用量或剩余量。
刷新间隔仍支持 5–1800 秒或 1–30 分钟；配置仍保存在 `%APPDATA%/TrafficMonitor/CodexQuota.ini`。
新增字段 `ShowUsed=0` 表示剩余量，`ShowUsed=1` 表示使用量。
CodexPath 留空自动检测，也可指定原生 codex.exe。

显示项：5h额度、周额度、5h刷新时间、周额度刷新时间、5h两行块、周额度两行块。
倒计时格式仍为 `xd xh`。TrafficMonitor 1.86 使用两个双行块时开启“水平排列”；支持 IsDoubleLineExclusive 的新版可直接独占双行。

## 数据与测试

继续使用已登录 Codex 的 account/rateLimits/read 读取真实账号数据；不根据本地用量估算，所有项共享一次查询。
源码包含在 src，运行 build.cmd 构建两个架构并运行回归测试。需要 MSVC C++ Build Tools 和 Windows SDK；DLL 静态链接运行库。

验证通过：x86/x64 编译与加载、六项稳定 ID、整数格式、倒计时、80/80.1/99.9/100 阈值、无效数据不告警、单/双行 GDI 颜色及边界、设置保存和上下限。
x86 真实账号测试显示周剩余 76%，设置切换后显示已用 24%，悬浮提示恰为两行；这些是测试快照，不是内置数据。
已查看正常、警告、用完的实际 GDI 渲染预览。未替换用户正在运行的 TrafficMonitor。

样式参考：https://github.com/Ganymede404/vscode-codex-usage/blob/main/src/statusBar.ts
参考插件使用 VS Code 主题告警背景；本插件按用户要求独立实现 >80% / >=100% 阈值及黄黑/红白配色。
TrafficMonitor 接口：https://github.com/zhongyang219/TrafficMonitor/blob/master/include/PluginInterface.h
Codex 接口：https://learn.chatgpt.com/docs/app-server

原创代码使用 MIT 许可，第三方头文件保留随包上游许可。
