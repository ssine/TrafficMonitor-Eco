TrafficMonitor Eco v0.1 (Windows 11 x64)

基于 TrafficMonitor 930f17533d6098989ebad62f210aa97d75ef174b 的 Lite 构建。
适用设备：Windows 11 / Intel Core Ultra 7 268V（Surface Pro Intel）。

用法：先退出其他 TrafficMonitor，再解压到可写目录，运行 TrafficMonitor.exe。
这是便携版本，配置和流量历史保存在本目录。保留原 TrafficMonitor 安装后，可正常退出本版，再启动原版切换回去。

Surface 专用配置沿用用户 2026-10-04 的现有设置：
每 2 秒采集；任务栏左侧双行；上下载、CPU、RAM、PWR、BAT；微软雅黑 9；白色文字与透明背景；保留曲线及提示。
Native 插件使用原电池项 ID，因此原标签可直接沿用。没有启用 GPU 项；需要时右键任务栏显示区域，在显示项目中选择“显卡利用率”、“专用显存”或“共享显存”。集成显卡主要看共享显存，专用为 0 是有效结果。

PWR：电池端功率，正号为充电，负号为放电。充电时不代表整机消耗功率。
BAT：剩余电量，单位 Wh。N/A 表示系统没有提供有效数据。
显存：所有适配器的占用合计，以 GiB 换算。专用和共享分开显示。

变化：
采集线程等待事件，移除 Sleep(10) 轮询；任务栏位置兜底每秒检查一次；按采样和内容变化重绘，曲线保留；不显示的 CPU 频率、硬盘及 GPU 利用率不再定期采集；GPU 利用率按物理引擎聚合；PDH 数组缓冲复用；电池使用系统 IOCTL（最短 3 秒，当前 2 秒主采样下通常 4 秒）；显存仅在其显示值被读取时请求采集。

本版包含低开销原生 EcoTelemetry.dll，未携带 LibreHardwareMonitor、旧 PowerMonPlugin 或温度传感器驱动。原来的温度监控不在本版范围内。
任务栏嵌入继续使用上游的 Win11 适配。此版没有加入熄屏暂停，也没有修改系统任务栏、Explorer 或电源计划。
TelemetryProbe.exe 是一次性诊断工具；运行后输出原始电池/显存 JSON，不常驻。

许可：TrafficMonitor 及衍生改动遵循随包 LICENSE / LICENSE_CN（Anti-996）。MSVC/MFC release 运行库来自微软 Visual Studio Build Tools 的再发行目录。
