#pragma once
#include <cmath>
#include <limits>
#include <string>

// Prefer a device/platform total to its overlapping rail/component sensors.
// Invalid/missing readings remain missing rather than becoming zero watts.
class HardwarePower
{
public:
    void Add(const std::wstring& name, double watts)
    {
        if (!std::isfinite(watts) || watts < 0) return;
        if (name.find(L"Platform") != std::wstring::npos) platform_ = watts;
        else if (name.find(L"Package") != std::wstring::npos) package_ = watts;
        else { components_ += watts; hasComponents_ = true; }
    }
    double Value() const
    {
        if (std::isfinite(platform_) && (!std::isfinite(package_) || platform_ >= package_)) return platform_;
        if (std::isfinite(package_)) return package_;
        return hasComponents_ ? components_ : std::numeric_limits<double>::quiet_NaN();
    }
private:
    double platform_ = std::numeric_limits<double>::quiet_NaN();
    double package_ = std::numeric_limits<double>::quiet_NaN();
    double components_ = 0;
    bool hasComponents_ = false;
};
