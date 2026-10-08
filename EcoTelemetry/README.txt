TrafficMonitor Eco v0.4.2 (Windows 11 x64)

基于 TrafficMonitor 的便携程序。先正常退出其他 TrafficMonitor，再解压到可写目录运行 TrafficMonitor.exe。
配置和流量历史保存在程序目录，升级前请备份；旧 PowerMonPlugin 不需一起复制。

本次提供两个包：
Desktop Full：含 CPU/GPU 温度，默认每 1 秒采集；八个指标为上传/下载、CPU/GPU 使用率、RAM/PWR、CPU/GPU 温度。
桌面 RAM 默认显示已用容量，例如 70.4 G，沿用 Windows 风格的 1024 进制容量换算，单位简写为 G；图表仍表示容量占用比例。
桌面默认启用“数值右对齐”，上下行共享单位列，G 与 W、上下载单位等按列对齐。
Surface Lite：默认每 2 秒采集；六个指标为上传/下载、CPU/RAM、PWR/BAT；无需硬件传感器驱动。
两种包均使用 Consolas 10，可在选项中换字体；网速和 PWR/BAT 保留一位小数、数值与单位之间一格。
两行共享数字和单位列，宽度按当前显示值调整。上传/下载曲线分别自适应最近两分钟的真实峰值。
网速历史图使用 3 秒时间常数的轻度平滑及采样点间插值，持续提速约 7 秒跟上 90%；网速数字、流量统计仍使用真实读数。
CPU/GPU/RAM 图为固定 0–100%，温度图为固定 0–100℃（并非过热阈值）；PWR 图用动态量程。

PWR 有两种来源，选择“显示项目”时请按设备选择：
“电池功率”：系统电池端读数，正号为充电，负号为放电；充电时不代表整机耗电。
“桌面硬件功率”：CPU/platform + 独立 GPU 的传感器总量，是估计值，并非插座实测；没有正负号。
桌面功率与主程序温度监控共用一次采样，插件不启动第二套硬件采集。集成 GPU 避免重复计入 CPU 总量。
PWR 柱形图显示两分钟历史，量程最低 1 W，峰值上方保留 25% 余量，旧峰值离开后以 15 秒半衰期回落。
N/A 表示系统没有有效读数，并在图上留空。BAT 为剩余电量 Wh；显存按所有适配器合计，用 GiB 换算。

Desktop Full 需要 .NET Framework 4.7.2 或更高（Windows 11 自带更新版本），以及 PawnIO 2.0 或更高用于 CPU 传感器。
首次在没有 PawnIO 的机器使用时，手动以管理员身份运行 prerequisites/PawnIO_setup.exe 完成安装，之后以管理员权限运行主程序。
已装满足要求的 PawnIO 无需升级。本次桌面实测使用已装的 PawnIO 2.1，没有更改它或 Windows 安全策略。
包内主程序和 OpenHardwareMonitorApi.dll 必须一起更新；不要混用旧版 DLL。
温度版使用 LibreHardwareMonitor 0.9.6 和随包依赖，不携带旧 TrafficMonitor.sys 或旧 PowerMonPlugin。

TelemetryProbe.exe：一次性电池/显存 JSON 诊断，不常驻。
HardwareProbe.exe（Full 包）：一次性检查 CPU/GPU 温度和桌面功率；需要管理员权限，不常驻。
任务栏嵌入沿用上游 Win11 适配；本版未修改 Explorer、任务栏系统设置或电源计划。

源码和完整程序包：https://github.com/ssine/TrafficMonitor-Eco/releases/tag/eco-v0.4.2
许可：TrafficMonitor 与衍生改动采用 LICENSE / LICENSE_CN（Anti-996）；第三方许可和来源见 third-party/。
MSVC/MFC release 运行库来自 Visual Studio Build Tools 的再发行目录。
