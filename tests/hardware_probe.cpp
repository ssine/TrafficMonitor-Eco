// Elevated Windows integration probe; uses the same monitor DLL as the host.
#include "../include/OpenHardwareMonitor/OpenHardwareMonitorApi.h"
#include <windows.h>
#include <cmath>
#include <cstdio>

int main()
{
    auto monitor = OpenHardwareMonitorApi::CreateInstance();
    if (!monitor) return 1;
    monitor->SetCpuEnable(true);
    monitor->SetGpuEnable(true);
    bool valid = false;
    for (int i = 0; i < 3; ++i)
    {
        monitor->GetHardwareInfo();
        const auto power = monitor->DesktopPower();
        const auto cpu = monitor->CpuTemperature(), gpu = monitor->GpuTemperature();
        valid = std::isfinite(power) && power >= 0 && cpu > 0 && gpu > 0;
        std::printf("{\"cpuTemperature\":%.1f,\"gpuTemperature\":%.1f,\"gpuUsage\":%.1f,\"desktopPower\":%.1f,\"valid\":%s}\n",
            cpu, gpu, monitor->GpuUsage(), std::isfinite(power) ? power : -1, valid ? "true" : "false");
        if (i < 2) Sleep(1000);
    }
    return valid ? 0 : 2;
}
