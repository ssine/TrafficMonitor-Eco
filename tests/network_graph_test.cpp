#include "../TrafficMonitor/AdaptiveGraphHistory.h"
#include <cassert>
#include <limits>

namespace
{
    void Add(AdaptiveGraphHistory& history, std::uint64_t time, double value, bool adaptive = true)
    {
        history.Add(time, value, adaptive, 1024.0, AdaptiveGraphHistory::NetworkSmoothingMs, 10000);
    }
    bool Near(double a, double b) { return std::abs(a - b) < 1e-8; }
}

int main()
{
    // A one-second burst is subdued in the plot but retained for scaling and
    // diagnostics. The numeric readout uses the application's raw speed.
    AdaptiveGraphHistory burst, raw;
    Add(burst, 1000, 0);
    Add(burst, 2000, 10240);
    raw.Add(1000, 0, true);
    raw.Add(2000, 10240, true);
    assert(burst.Samples().back().value == 10240);
    const auto peak = burst.Samples().back().plottedValue;
    assert(peak > 2800 && peak < 3000);
    assert(burst.Scale() == raw.Scale() && burst.Scale() == 12800);
    Add(burst, 3000, 0);
    assert(burst.Samples().back().plottedValue > 2000 && burst.Samples().back().plottedValue < peak);
    assert(burst.Samples().back().value == 0);

    // Same elapsed time produces the same response at different sample rates.
    // A sustained step reaches ~63% in 3 s and ~90% in 7 s.
    AdaptiveGraphHistory fast, slow;
    Add(fast, 1000, 0); Add(slow, 1000, 0);
    for (unsigned time = 1250; time <= 4000; time += 250) Add(fast, time, 10000);
    Add(slow, 4000, 10000);
    assert(Near(fast.Samples().back().plottedValue, slow.Samples().back().plottedValue));
    assert(slow.Samples().back().plottedValue > 6300 && slow.Samples().back().plottedValue < 6400);
    Add(slow, 8000, 10000);
    assert(slow.Samples().back().plottedValue > 9000 && slow.Samples().back().plottedValue < 9100);

    // Duplicate timestamps replace without filter drift, including the first
    // sample. A changed value is recomputed from the previous distinct time.
    const auto count = burst.Samples().size();
    const auto before = burst.Samples().back().plottedValue;
    Add(burst, 3000, 0);
    assert(burst.Samples().size() == count && Near(before, burst.Samples().back().plottedValue));
    Add(burst, 3000, 10240);
    AdaptiveGraphHistory distinct;
    Add(distinct, 1000, 0); Add(distinct, 2000, 10240); Add(distinct, 3000, 10240);
    assert(Near(burst.Samples().back().plottedValue, distinct.Samples().back().plottedValue));
    AdaptiveGraphHistory first;
    Add(first, 1, 10); Add(first, 1, 20);
    assert(first.Samples().size() == 1 && first.Samples().back().plottedValue == 20);

    // Missing readings and long pauses restart the filter and leave a gap.
    const auto missing = std::numeric_limits<double>::quiet_NaN();
    Add(burst, 4000, missing);
    assert(std::isnan(burst.Samples().back().plottedValue));
    Add(burst, 5000, 5120);
    assert(burst.Samples().back().plottedValue == 5120);
    Add(burst, 16000, 2048);
    assert(burst.Samples().back().plottedValue == 2048);
    assert(std::isnan(AdaptiveGraphHistory::Interpolate(burst.Samples()[4], burst.Samples()[5], 8000, 10000)));
    Add(burst, 1, 10); // clock reset
    assert(burst.Samples().size() == 1 && burst.Samples().back().plottedValue == 10);

    // Linear interpolation stays between measured endpoints; no overshoot,
    // holding steps, or connections across an invalid endpoint.
    const AdaptiveGraphHistory::Sample left{1000, 0, 100}, right{3000, 0, 900}, invalid{3000, missing, missing};
    assert(AdaptiveGraphHistory::Interpolate(left, right, 2000, 10000) == 500);
    assert(AdaptiveGraphHistory::Interpolate(left, right, 1000, 10000) == 100);
    assert(AdaptiveGraphHistory::Interpolate(left, right, 3000, 10000) == 900);
    assert(std::isnan(AdaptiveGraphHistory::Interpolate(left, invalid, 2000, 10000)));
    assert(std::isnan(AdaptiveGraphHistory::Interpolate(left, right, 2000, 1000)));
    assert(std::isnan(AdaptiveGraphHistory::Interpolate(left, right, 4000, 10000)));
    assert(AdaptiveGraphHistory::PixelHeight(8, 1000, 200) == 2); // no whole-percent quantization
    assert(AdaptiveGraphHistory::PixelHeight(2000, 1000, 20) == 20);
    assert(AdaptiveGraphHistory::PixelHeight(-1, 1000, 20) == 0);
    assert(AdaptiveGraphHistory::PixelHeight(missing, 1000, 20) == 0);

    // Manual-scale network plots are also smoothed. Other histories remain raw.
    AdaptiveGraphHistory fixed, resource;
    Add(fixed, 1000, 0, false); Add(fixed, 2000, 10000, false);
    assert(!fixed.IsAdaptive() && fixed.Samples().back().plottedValue < 3000);
    resource.Add(1000, 0, false); resource.Add(2000, 100, false);
    assert(resource.Samples().back().plottedValue == resource.Samples().back().value);
}
