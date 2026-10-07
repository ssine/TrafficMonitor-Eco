#pragma once
#include <cstdint>
#include <limits>

// Optional host -> item snapshot; shares the host's existing sensor update.
struct EcoDesktopPowerSample
{
    std::uint32_t size = sizeof(EcoDesktopPowerSample);
    double value = std::numeric_limits<double>::quiet_NaN();
};
