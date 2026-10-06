#pragma once
#include <cstdint>
#include <limits>

// Optional raw graph data carried by IPluginItem::OnItemInfo. Older hosts keep
// using the standard, normalized GetResourceUsageGraphValue() interface.
struct EcoGraphSample
{
    std::uint32_t size = sizeof(EcoGraphSample);
    double value = std::numeric_limits<double>::quiet_NaN(); // magnitude; NaN = gap
    double minimumScale = 1.0;
};
