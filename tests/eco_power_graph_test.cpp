// Windows/MSVC integration test: exercise the actual plugin item implementation.
#include "../EcoTelemetry/EcoTelemetry.cpp"
#include <cassert>
#include <thread>

int main()
{
    Item power(L"Power", L"BatteryPowerMon", L"PWR:", L"+999.9 W", nullptr, true);
    Item energy(L"Energy", L"BatteryCapacityMon", L"BAT:", L"999.9 Wh");
    EcoGraphSample sample;
    auto read = [&] {
        assert(power.OnItemInfo(IPluginItem::IIT_ECO_GRAPH_SAMPLE, &sample, nullptr) == &sample);
        assert(sample.minimumScale == 1.0);
    };
    assert(power.IsDrawResourceUsageGraph() && !energy.IsDrawResourceUsageGraph());
    read();
    assert(std::isnan(sample.value));
    assert(power.GetResourceUsageGraphValue() == 0.0f);
    for (double watts : {0.0, 0.1, 12.34, -12.34, 999.9, -999.9}) {
        power.Set(Format(watts, true, L"%+.1f W"), watts);
        read();
        assert(sample.value == std::abs(watts));
        assert(std::wstring(power.GetItemValueText()) == Format(watts, true, L"%+.1f W"));
        const auto legacy = power.GetResourceUsageGraphValue();
        assert(legacy >= 0 && legacy <= 1);
    }
    power.Set(L"N/A");
    read();
    assert(std::isnan(sample.value)); // missing is not a valid zero-watt sample
    assert(!energy.OnItemInfo(IPluginItem::IIT_ECO_GRAPH_SAMPLE, &sample, nullptr));
    assert(!power.OnItemInfo(IPluginItem::IIT_ECO_GRAPH_SAMPLE, nullptr, nullptr));
    sample.size = 0;
    assert(!power.OnItemInfo(IPluginItem::IIT_ECO_GRAPH_SAMPLE, &sample, nullptr));
    sample.size = sizeof(sample);
    std::atomic<bool> done{false};
    std::thread writer([&] {
        for (int i = 0; i < 10000; ++i) power.Set(L"-12.3 W", -12.34);
        done.store(true);
    });
    do {
        read();
        assert(std::isnan(sample.value) || sample.value == 12.34);
        assert(power.GetItemValueText() != nullptr);
    } while (!done.load());
    writer.join();
}
