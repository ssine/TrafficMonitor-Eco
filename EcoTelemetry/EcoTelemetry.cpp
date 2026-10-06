// Native battery and adapter-memory collector for the Surface Eco build.
// No hardware sensor library, WMI polling, subprocesses, or private timer.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <winioctl.h>
#include <batclass.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <utility>
#include "../include/PluginInterface.h"
#include "../include/EcoGraphSample.h"

namespace {
constexpr GUID batteryClass{0x72631e54, 0x78a4, 0x11d0,
    {0xbc, 0xf7, 0x00, 0xaa, 0x00, 0xb7, 0xb3, 0x2a}};

struct Battery {
    HANDLE handle = INVALID_HANDLE_VALUE;
    ULONG tag = 0;
    Battery(HANDLE h, ULONG t) : handle(h), tag(t) {}
    Battery(const Battery&) = delete;
    Battery& operator=(const Battery&) = delete;
    Battery(Battery&& other) noexcept : handle(std::exchange(other.handle, INVALID_HANDLE_VALUE)), tag(other.tag) {}
    ~Battery() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
};

struct Snapshot {
    double power = 0, energy = 0, dedicated = 0, shared = 0;
    bool powerValid = false, energyValid = false, dedicatedValid = false, sharedValid = false;
};

class Collector {
public:
    ~Collector() { if (query_) PdhCloseQuery(query_); }

    Snapshot Read(bool forceBattery = false, bool gpuRequired = true) {
        const auto now = GetTickCount64();
        if (forceBattery || now >= nextBattery_) {
            ReadBattery(now);
            nextBattery_ = now + 3000;
        }
        if (gpuRequired) ReadGpu(now);
        return snapshot_;
    }

    size_t BatteryCount() const { return batteries_.size(); }
    DWORD BatteryError() const { return batteryError_; }
    PDH_STATUS GpuError() const { return gpuError_; }

private:
    void DiscoverBatteries(ULONGLONG now) {
        batteries_.clear();
        nextDiscovery_ = now + 60000;
        const HDEVINFO devices = SetupDiGetClassDevsW(&batteryClass, nullptr, nullptr,
            DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        if (devices == INVALID_HANDLE_VALUE) { batteryError_ = GetLastError(); return; }
        SP_DEVICE_INTERFACE_DATA iface{};
        iface.cbSize = sizeof(iface);
        for (DWORD i = 0; SetupDiEnumDeviceInterfaces(devices, nullptr, &batteryClass, i, &iface); ++i) {
            DWORD bytes = 0;
            SetupDiGetDeviceInterfaceDetailW(devices, &iface, nullptr, 0, &bytes, nullptr);
            if (!bytes) continue;
            std::vector<BYTE> buffer(bytes);
            auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buffer.data());
            detail->cbSize = sizeof(*detail);
            if (!SetupDiGetDeviceInterfaceDetailW(devices, &iface, detail, bytes, nullptr, nullptr)) continue;
            HANDLE handle = CreateFileW(detail->DevicePath, GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (handle == INVALID_HANDLE_VALUE) { batteryError_ = GetLastError(); continue; }
            ULONG wait = 0, tag = 0;
            DWORD returned = 0;
            if (!DeviceIoControl(handle, IOCTL_BATTERY_QUERY_TAG, &wait, sizeof(wait),
                &tag, sizeof(tag), &returned, nullptr) || tag == 0) {
                batteryError_ = GetLastError(); CloseHandle(handle); continue;
            }
            BATTERY_QUERY_INFORMATION request{};
            request.BatteryTag = tag;
            request.InformationLevel = BatteryInformation;
            BATTERY_INFORMATION info{};
            if (!DeviceIoControl(handle, IOCTL_BATTERY_QUERY_INFORMATION, &request, sizeof(request),
                &info, sizeof(info), &returned, nullptr)) {
                batteryError_ = GetLastError(); CloseHandle(handle); continue;
            }
            if (!(info.Capabilities & BATTERY_SYSTEM_BATTERY) ||
                (info.Capabilities & BATTERY_CAPACITY_RELATIVE)) {
                CloseHandle(handle); continue;
            }
            batteries_.emplace_back(handle, tag);
        }
        SetupDiDestroyDeviceInfoList(devices);
    }

    void ReadBattery(ULONGLONG now) {
        if (now >= nextDiscovery_) DiscoverBatteries(now);
        snapshot_.power = snapshot_.energy = 0;
        snapshot_.powerValid = snapshot_.energyValid = !batteries_.empty();
        for (auto& battery : batteries_) {
            BATTERY_WAIT_STATUS request{};
            request.BatteryTag = battery.tag;
            request.Timeout = 0;
            request.LowCapacity = 0;
            request.HighCapacity = BATTERY_UNKNOWN_CAPACITY;
            BATTERY_STATUS status{};
            DWORD returned = 0;
            if (!DeviceIoControl(battery.handle, IOCTL_BATTERY_QUERY_STATUS, &request, sizeof(request),
                &status, sizeof(status), &returned, nullptr)) {
                batteryError_ = GetLastError();
                snapshot_.powerValid = snapshot_.energyValid = false;
                nextDiscovery_ = 0; // Renew handles/tags on removal or resume.
                continue;
            }
            batteryError_ = ERROR_SUCCESS;
            if (status.Capacity == BATTERY_UNKNOWN_CAPACITY) snapshot_.energyValid = false;
            else snapshot_.energy += status.Capacity / 1000.0;
            if (status.Rate == static_cast<LONG>(BATTERY_UNKNOWN_RATE)) snapshot_.powerValid = false;
            else snapshot_.power += status.Rate / 1000.0;
        }
    }

    bool OpenGpu(ULONGLONG now) {
        if (query_) return true;
        if (now < nextGpuRetry_) return false;
        nextGpuRetry_ = now + 30000;
        dedicated_ = shared_ = nullptr;
        gpuError_ = PdhOpenQueryW(nullptr, 0, &query_);
        if (gpuError_ != ERROR_SUCCESS) { query_ = nullptr; return false; }
        const auto d = PdhAddEnglishCounterW(query_, L"\\GPU Adapter Memory(*)\\Dedicated Usage", 0, &dedicated_);
        const auto s = PdhAddEnglishCounterW(query_, L"\\GPU Adapter Memory(*)\\Shared Usage", 0, &shared_);
        if (d != ERROR_SUCCESS && s != ERROR_SUCCESS) {
            gpuError_ = d; PdhCloseQuery(query_); query_ = nullptr; return false;
        }
        return true;
    }

    bool ReadArray(PDH_HCOUNTER counter, double& total) {
        if (!counter) return false;
        DWORD bytes = 0, count = 0;
        gpuError_ = PdhGetFormattedCounterArrayW(counter, PDH_FMT_LARGE, &bytes, &count, nullptr);
        if (gpuError_ != PDH_MORE_DATA) return false;
        buffer_.resize(bytes);
        auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer_.data());
        gpuError_ = PdhGetFormattedCounterArrayW(counter, PDH_FMT_LARGE, &bytes, &count, items);
        if (gpuError_ != ERROR_SUCCESS) return false;
        total = 0;
        bool valid = false;
        for (DWORD i = 0; i < count; ++i) {
            const auto& value = items[i].FmtValue;
            if (value.CStatus == PDH_CSTATUS_VALID_DATA || value.CStatus == PDH_CSTATUS_NEW_DATA) {
                total += static_cast<double>(value.largeValue);
                valid = true;
            }
        }
        return valid;
    }

    void ReadGpu(ULONGLONG now) {
        snapshot_.dedicatedValid = snapshot_.sharedValid = false;
        if (!OpenGpu(now)) return;
        gpuError_ = PdhCollectQueryData(query_);
        if (gpuError_ != ERROR_SUCCESS) {
            PdhCloseQuery(query_); query_ = nullptr; return;
        }
        snapshot_.dedicatedValid = ReadArray(dedicated_, snapshot_.dedicated);
        snapshot_.sharedValid = ReadArray(shared_, snapshot_.shared);
    }

    Snapshot snapshot_;
    std::vector<Battery> batteries_;
    ULONGLONG nextBattery_ = 0, nextDiscovery_ = 0, nextGpuRetry_ = 0;
    DWORD batteryError_ = ERROR_SUCCESS;
    PDH_STATUS gpuError_ = ERROR_SUCCESS;
    PDH_HQUERY query_ = nullptr;
    PDH_HCOUNTER dedicated_ = nullptr, shared_ = nullptr;
    std::vector<BYTE> buffer_;
};

class Item final : public IPluginItem {
public:
    Item(const wchar_t* name, const wchar_t* id, const wchar_t* label, const wchar_t* sample, std::atomic<bool>* demand = nullptr, bool powerGraph = false)
        : name_(name), id_(id), label_(label), sample_(sample), demand_(demand), powerGraph_(powerGraph) {}
    const wchar_t* GetItemName() const override { return name_; }
    const wchar_t* GetItemId() const override { return id_; }
    const wchar_t* GetItemLableText() const override { return label_; }
    const wchar_t* GetItemValueSampleText() const override { return sample_; }
    const wchar_t* GetItemValueText() const override {
        // Request GPU sampling only while its values are being displayed.
        // A pending flag also works with long host sampling intervals.
        if (demand_) demand_->store(true, std::memory_order_relaxed);
        // Host reads on the UI thread while sampling runs on a worker.
        thread_local std::wstring result;
        AcquireSRWLockShared(&lock_); result = value_; ReleaseSRWLockShared(&lock_);
        return result.c_str();
    }
    int IsDrawResourceUsageGraph() const override { return powerGraph_ ? 1 : 0; }
    float GetResourceUsageGraphValue() const override {
        // Compatible 0..1 value for upstream/older hosts (fixed 100 W scale).
        const auto value = GraphValue();
        return std::isfinite(value) ? static_cast<float>((std::min)(1.0, value / 100.0)) : 0.0f;
    }
    void* OnItemInfo(ItemInfoType type, void* para1, void*) override {
        if (type != IIT_ECO_GRAPH_SAMPLE || !powerGraph_ || !para1) return nullptr;
        auto& sample = *static_cast<EcoGraphSample*>(para1);
        if (sample.size != sizeof(EcoGraphSample)) return nullptr;
        sample.value = GraphValue();
        sample.minimumScale = 1.0; // Watts; network graphs have their own floor.
        return para1;
    }
    void Set(const std::wstring& value, double power = std::numeric_limits<double>::quiet_NaN()) {
        AcquireSRWLockExclusive(&lock_);
        value_ = value;
        graphValue_ = std::isfinite(power) ? std::abs(power) : std::numeric_limits<double>::quiet_NaN();
        ReleaseSRWLockExclusive(&lock_);
    }
private:
    double GraphValue() const {
        AcquireSRWLockShared(&lock_); const auto value = graphValue_; ReleaseSRWLockShared(&lock_);
        return value;
    }
    const wchar_t *name_, *id_, *label_, *sample_;
    std::atomic<bool>* demand_;
    bool powerGraph_;
    mutable SRWLOCK lock_ = SRWLOCK_INIT;
    std::wstring value_ = L"N/A";
    double graphValue_ = std::numeric_limits<double>::quiet_NaN();
};

std::wstring Format(double value, bool valid, const wchar_t* format) {
    if (!valid || !std::isfinite(value)) return L"N/A";
    wchar_t buffer[64]; swprintf_s(buffer, format, value);
    std::wstring text = buffer;
    // Fixed decimals keep the numeric columns steady as values change.
    return text;
}

class Plugin final : public ITMPlugin {
public:
    IPluginItem* GetItem(int index) override { return index >= 0 && index < 4 ? &items_[index] : nullptr; }
    void DataRequired() override {
        const auto s = collector_.Read(false, gpuRequested_.exchange(false, std::memory_order_relaxed));
        items_[0].Set(Format(s.power, s.powerValid, L"%+.1f W"),
            s.powerValid ? s.power : std::numeric_limits<double>::quiet_NaN());
        items_[1].Set(Format(s.energy, s.energyValid, L"%.1f Wh"));
        items_[2].Set(Format(s.dedicated / (1024 * 1024 * 1024.0), s.dedicatedValid, L"%.2f G"));
        items_[3].Set(Format(s.shared / (1024 * 1024 * 1024.0), s.sharedValid, L"%.2f G"));
    }
    const wchar_t* GetInfo(PluginInfoIndex index) override {
        switch (index) {
        case TMI_NAME: return L"Surface Eco Telemetry";
        case TMI_DESCRIPTION: return L"Native battery W/Wh and adapter GPU memory. Battery refresh: 3s. No sensor library.";
        case TMI_AUTHOR: return L"Sine / Codex";
        case TMI_COPYRIGHT: return L"2026 Sine";
        case TMI_VERSION: return L"0.3.0";
        default: return L"";
        }
    }
    const wchar_t* GetTooltipInfo() override {
        return L"Battery: negative W = discharging; positive W = charging.\n"
               L"PWR graph: magnitude, adaptive 2-minute scale (minimum 1 W).\n"
               L"Wh is remaining battery energy. N/A means unavailable.\n"
               L"GPU memory: adapter totals (all adapters), GiB; dedicated and shared separately.";
    }
private:
    Collector collector_;
    std::atomic<bool> gpuRequested_{ false };
    Item items_[4]{
        {L"电池功率", L"BatteryPowerMon", L"PWR:", L"+999.9 W", nullptr, true},
        {L"剩余电量", L"BatteryCapacityMon", L"BAT:", L"999.9 Wh"},
        {L"专用显存", L"eco_gpu_dedicated", L"VRAM:", L"9.99 G", &gpuRequested_},
        {L"共享显存", L"eco_gpu_shared", L"SHR:", L"9.99 G", &gpuRequested_}
    };
};
}

extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance() {
    static Plugin plugin;
    return &plugin;
}

#ifdef ECO_PROBE
int wmain() {
    Collector collector;
    collector.Read(true);
    Sleep(1100); // Probe only; the DLL has no timer or sleep loop.
    const auto s = collector.Read(true);
    printf("{\"battery_count\":%zu,\"battery_error\":%lu,\"gpu_error\":%ld,"
           "\"power_valid\":%s,\"power_w\":%.3f,\"energy_valid\":%s,\"energy_wh\":%.3f,"
           "\"dedicated_valid\":%s,\"dedicated_bytes\":%.0f,\"shared_valid\":%s,\"shared_bytes\":%.0f}\n",
        collector.BatteryCount(), collector.BatteryError(), collector.GpuError(),
        s.powerValid ? "true" : "false", s.power, s.energyValid ? "true" : "false", s.energy,
        s.dedicatedValid ? "true" : "false", s.dedicated, s.sharedValid ? "true" : "false", s.shared);
    return s.dedicatedValid || s.sharedValid ? 0 : 1;
}
#endif
