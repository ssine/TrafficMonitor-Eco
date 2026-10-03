#include "../TrafficMonitor/AdaptiveGraphHistory.h"
#include <cassert>
#include <limits>

int main()
{
    AdaptiveGraphHistory up, down;
    up.Add(1000, 2048, true);
    assert(up.Scale() == 2560);
    assert(AdaptiveGraphHistory::Percent(2048, up.Scale()) == 80);
    up.Add(3000, 1024 * 1024, true);
    assert(up.Scale() == 1310720);
    assert(up.Samples().front().value == 2048); // raw history survives rescaling
    assert(AdaptiveGraphHistory::Percent(2048, up.Scale()) == 0);
    down.Add(3000, 4096, true);
    assert(down.Scale() == 5120); // directions scale independently

    up.Add(5000, 0, true);
    assert(up.Scale() == 1310720); // visible old peak still defines the scale
    up.Add(123001, 0, true);
    assert(up.Scale() >= 1024 && up.Scale() < 1310720);
    const auto scale = up.Scale();
    up.Add(125001, 0, true);
    assert(up.Scale() >= 1024 && up.Scale() < scale);
    up.Add(125001, 10, true);
    assert(up.Samples().size() == 2); // duplicate time replaces, never advances
    assert(up.Samples().back().value == 10);
    up.Add(200000, std::numeric_limits<double>::quiet_NaN(), true);
    assert(up.Samples().back().value == 0);
    assert(AdaptiveGraphHistory::Percent(2048, 4096) == 50); // fixed-scale mode
    assert(AdaptiveGraphHistory::Percent(8192, 4096) == 100);
    assert(AdaptiveGraphHistory::Percent(10, 0) == 0);
    for (unsigned i = 0; i < 3000; ++i) down.Add(4000 + i, i, true);
    assert(down.Samples().size() <= 1024);
}
