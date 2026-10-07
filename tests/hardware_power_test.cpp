#include "../OpenHardwareMonitorApi/HardwarePower.h"
#include <cassert>

int main()
{
    HardwarePower power;
    assert(std::isnan(power.Value()));
    power.Add(L"CPU Cores", 12);
    power.Add(L"CPU Graphics", 3);
    assert(power.Value() == 15);
    power.Add(L"CPU Package", 20);
    assert(power.Value() == 20); // package and components must not double-count
    power.Add(L"Platform", 25);
    assert(power.Value() == 25);
    power.Add(L"Platform", 10);
    assert(power.Value() == 20);
    HardwarePower missing;
    missing.Add(L"GPU Package", -1);
    missing.Add(L"GPU Package", std::numeric_limits<double>::quiet_NaN());
    assert(std::isnan(missing.Value()));
    missing.Add(L"GPU Package", 0);
    assert(missing.Value() == 0); // valid idle is distinct from missing
}
