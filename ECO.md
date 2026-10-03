# TrafficMonitor Eco v0.1

本仓库基于 [zhongyang219/TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor)，保留上游 Git 历史与 Anti-996 许可。

- 上游基准提交：`930f17533d6098989ebad62f210aa97d75ef174b`。
- Eco 实现提交：`6419de3`，对应已在 Surface 实测的首版源码。
- 维护分支：`surface-eco-v1`。
- 实测设备：Surface Pro for Business 11th Edition with Intel；Core Ultra 7 268V；8 个逻辑处理器；Windows 11 Pro，build 26200。

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

用户配置、流量历史和本机运行状态不随源码提交；实际 Surface 便携包沿用了原 2 秒采样、任务栏左侧双行、上下载/CPU/RAM/PWR/BAT、微软雅黑 9、白色透明背景及曲线配置。

## Surface 实测（2026-10-04）

CPU 按进程 CPU 时间差 / 实际时长 / 8 计算，表示整机 CPU 容量口径。旧版两段合计约 8 分钟；Eco 两轮也约 8 分钟；同插件 Lite 对照约 5 分钟。

| 版本 | 平均 CPU | 私有内存 |
| --- | ---: | ---: |
| 原版 1.85.1，原 PowerMonPlugin | 0.574–0.594% | 22.6 MiB |
| 上游 Lite，同一原生插件 | 0.360% | 10.0 MiB |
| Eco 两轮 | 0.029–0.035% | 10.1–10.5 MiB |

第一轮 Eco 相对原版进程 CPU 降低约 94.1%，相对同插件 Lite 对照降低约 90.3%。各窗口按顺序在日常现场负载下测量，未严格固定其他应用负载。原版与 Lite 对照也包含上游版本差异，因此额外使用同插件 Lite 对照评估采集和刷新改动。

原版已运行多天，工作集经过系统裁剪；上述内存列为私有内存，不能据此声称物理工作集减少。现场处于充电状态，电池充电功率不是整机耗电；尚无量化续航结论。

通过：任务栏嵌入与原六项布局、数值更新、右键菜单、共享显存开关与恢复、正常退出和重新启动、插件并发读取。无电池环境显示 N/A。

首版没有熄屏暂停；Explorer 重启、旋转、多屏、合盖和拔电续航尚未测试。Win11 任务栏嵌入继续使用上游适配。

## 许可

TrafficMonitor 及本仓库衍生改动遵循 [LICENSE](./LICENSE) / [LICENSE_CN](./LICENSE_CN)（Anti-996）。
