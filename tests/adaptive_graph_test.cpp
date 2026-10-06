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
    assert(std::isnan(up.Samples().back().value));
    assert(AdaptiveGraphHistory::Percent(2048, 4096) == 50); // fixed-scale mode
    assert(AdaptiveGraphHistory::Percent(8192, 4096) == 100);
    assert(AdaptiveGraphHistory::Percent(10, 0) == 0);
    for (unsigned i = 0; i < 3000; ++i) down.Add(4000 + i, i, true);
    assert(down.Samples().size() <= 1024);

    AdaptiveGraphHistory power;
    power.Add(1000, 8.0, true, 1.0);
    assert(power.Scale() == 10.0); // watts must not inherit the 1024 B/s floor
    power.Add(3000, 24.0, true, 1.0);
    assert(power.Scale() == 30.0);
    assert(power.Samples().front().value == 8.0);
    assert(AdaptiveGraphHistory::Percent(8.0, power.Scale()) == 26);
    for (unsigned t = 5000; t <= 123000; t += 2000) power.Add(t, 2.0, true, 1.0);
    assert(power.Scale() == 30.0); // keep the peak while it is still visible
    power.Add(125000, 2.0, true, 1.0);
    assert(power.Scale() < 30.0 && power.Scale() > 25.0);
    const auto decayStart = power.Scale();
    power.Add(140000, 2.0, true, 1.0);
    assert(std::abs(power.Scale() - decayStart / 2) < 1e-10);
    const auto count = power.Samples().size();
    power.Add(140000, std::numeric_limits<double>::quiet_NaN(), true, 1.0);
    assert(power.Samples().size() == count && std::isnan(power.Samples().back().value));
    assert(AdaptiveGraphHistory::Percent(power.Samples().back().value, power.Scale()) == 0);
    power.Add(300000, 0.0, true, 1.0);
    assert(power.Scale() == 1.0 && power.IsAdaptive());
    power.Add(100, 4.0, true, 1.0); // clock reset starts fresh history and scale
    assert(power.Samples().size() == 1 && power.Scale() == 5.0);
    power.Add(200, 10.0, false, 1.0);
    assert(!power.IsAdaptive());
}
