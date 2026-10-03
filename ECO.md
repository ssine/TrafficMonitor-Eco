# TrafficMonitor Eco v0.2

本仓库基于 [zhongyang219/TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor)，保留上游 Git 历史与 Anti-996 许可。

- 上游基准提交：`930f17533d6098989ebad62f210aa97d75ef174b`。
- Eco v0.1 基线提交：`6419de3`；v0.2 在此基础上增加网速自适应曲线和紧凑对齐。
- 维护分支：`surface-eco-v1`。
- 实测设备：Surface Pro for Business 11th Edition with Intel；Core Ultra 7 268V；8 个逻辑处理器；Windows 11 Pro，build 26200。

## 下载与显示设置

[下载 Eco v0.2 完整 x64 程序包](https://github.com/ssine/TrafficMonitor-Eco/releases/tag/eco-v0.2.0)。解压到可写目录，正常退出其他 TrafficMonitor 后运行 `TrafficMonitor.exe`。包内包含主程序、`plugins/EcoTelemetry.dll`、诊断工具、必要的运行库和默认配置。

默认每 2 秒采样，任务栏左侧双行显示上传/下载、CPU/RAM、PWR/BAT；字体为 Consolas 10，白色透明背景。网速和 PWR/BAT 固定一位小数，标签后留一格，数值和单位之间保留一个空格，例如 `PWR: +6.4 W`、`BAT: 49.4 Wh`。两行共享标签、数字和单位列，列宽按当前显示值调整，位数和正负号变化时重新计算宽度。

右键 → 选项 → 任务栏窗口设置：可使用原字体选择器，或点击 Consolas 快捷按钮（字号至少 10）；可勾选“数值和单位之间用空格分隔”切换单位间距。网速图默认开启自适应缩放，关闭后可指定手动上限。

上传、下载各自使用最近 120 秒的原始采样值计算量程，最低 1 KiB/s，上限为窗口内峰值的 1.25 倍；上涨立即跟随，旧峰值离开窗口后以 15 秒半衰期平滑回落。CPU/RAM 保持 0–100% 量程。所有曲线共用两分钟时间轴，历史样本按当前量程整体重画；绘制本身不添加样本。

## 主要改动

- 采集线程从 `Sleep(10)` 轮询改为事件等待；退出时处理同步窗口消息，避免等待线程退出时死锁。
- 任务栏从每 100ms 无条件重绘改为按采样和内容变化重绘；保留曲线和自绘插件刷新，几何位置兜底检查改为每秒一次。
- 不显示的 CPU 频率、硬盘及 GPU 利用率不再定期采集。
- PDH 数组复用缓冲区并过滤无效状态；GPU 利用率按物理引擎聚合，避免按进程实例重复统计。
- 新增原生 `EcoTelemetry.dll`：通过 Windows 电池 IOCTL 读取 W/Wh，通过适配器 PDH 读取专用/共享显存。电池采样间隔至少 3 秒，2 秒主周期下通常为 4 秒；显存按显示需求采集。插件没有独立线程或定时器。
- 沿用电池项 ID `BatteryPowerMon`、`BatteryCapacityMon`，兼容原显示标签。正 W 表示充电，负 W 表示放电，无有效数据时显示 N/A。

核心改动位于 `TrafficMonitor/TrafficMonitorDlg.cpp`、`TrafficMonitor/TrafficMonitorDlg.h` 和 `TrafficMonitor/PdhHardwareQuery/`；插件与构建脚本位于 `EcoTelemetry/`。

## 构建

在 Windows 上安装 Visual Studio 2022 或 Build Tools 2022，包含 MSVC v143、x64 MFC 和 Windows SDK。然后在仓库根目录运行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\EcoTelemetry\build.ps1
```

脚本生成 x64 `Release (lite)` 主程序、`plugins\EcoTelemetry.dll` 和一次性诊断工具 `TelemetryProbe.exe`，并复制必要的 release 运行库和许可证。默认输出目录为 `eco-release`。

脚本在临时源码副本中统一源文件编码，不修改工作区源码。可用 `-Output` 指定输出目录，`-ConfigPath` 复制已有用户配置，`-MfcRoot` 指定独立 MFC 库路径。

用户配置、流量历史和本机运行状态不随源码提交。构建脚本默认只生成程序和运行库；Release 额外提供经过整理的默认显示配置，不含 Surface 的网卡名称或流量历史。Surface 升级保留其已有配置和历史，并调整字体、单位间距及网速图设置。

自适应量程的独立验证可在支持 C++17 的环境运行：

```sh
g++ -std=c++17 -Wall -Wextra -pedantic tests/adaptive_graph_test.cpp -o /tmp/trafficmonitor-adaptive-test
/tmp/trafficmonitor-adaptive-test
```

覆盖独立上下行量程、增长/回落、原始历史保留、固定量程、重复时间、无效采样和容量边界。

## Surface v0.1 历史基线（2026-10-04）

CPU 按进程 CPU 时间差 / 实际时长 / 8 计算，表示整机 CPU 容量口径。旧版两段合计约 8 分钟；Eco 两轮也约 8 分钟；同插件 Lite 对照约 5 分钟。

| 版本 | 平均 CPU | 私有内存 |
| --- | ---: | ---: |
| 原版 1.85.1，原 PowerMonPlugin | 0.574–0.594% | 22.6 MiB |
| 上游 Lite，同一原生插件 | 0.360% | 10.0 MiB |
| Eco v0.1 两轮 | 0.029–0.035% | 10.1–10.5 MiB |

第一轮 Eco 相对原版进程 CPU 降低约 94.1%，相对同插件 Lite 对照降低约 90.3%。各窗口按顺序在日常现场负载下测量，未严格固定其他应用负载。原版与 Lite 对照也包含上游版本差异，因此额外使用同插件 Lite 对照评估采集和刷新改动。

原版已运行多天，工作集经过系统裁剪；上述内存列为私有内存，不能据此声称物理工作集减少。现场处于充电状态，电池充电功率不是整机耗电；尚无量化续航结论。

通过：任务栏嵌入与原六项布局、数值更新、右键菜单、共享显存开关与恢复、正常退出和重新启动、插件并发读取。无电池环境显示 N/A。

首版没有熄屏暂停；Explorer 重启、旋转、多屏、合盖和拔电续航尚未测试。Win11 任务栏嵌入继续使用上游适配。

## Surface v0.2 验证（2026-10-04）

本轮按同样的进程 CPU 时间口径采样 116.0 秒，共 24 个样本、8 个逻辑处理器。平均进程 CPU 为 **0.056%**（单核口径约 0.444%），私有内存约 **10.0 MiB**，工作集约 **35.5 MiB**；同期系统总 CPU 平均约 20.9%。采样期间做了六组 8 MiB 上传/下载往返，共约 96 MiB 实际传输，用于检查图表和 KB/s、MB/s 的一位小数显示。此轮较短且包含传输负载，与上方 v0.1 历史窗口的环境不同，不作为严格的版本性能差异或续航结论。

通过：Windows x64 Lite 与原生插件构建；自适应量程独立测试；字体切换、Consolas 快捷按钮与至少 10 号字号；自动量程开关及手动上限启用状态；实际上传/下载曲线；网速和 PWR/BAT 一位小数及单位空格；`+99.9 W`、`-99.9 W`、`+999.9 W`、`-999.9 W` 的完整显示；设置页按钮无重叠；正常退出和重启。

边界值来自独立测试目录中的合成插件，截图后已恢复正式程序。Surface 日常程序的 EXE/DLL 与 Release 程序包一致，桌面 Eco 快捷方式指向 v0.2；其原配置和流量历史保留。

## 许可

TrafficMonitor 及本仓库衍生改动遵循 [LICENSE](./LICENSE) / [LICENSE_CN](./LICENSE_CN)（Anti-996）。
