#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>

// Raw samples share a two-minute time axis. Repainting never adds samples,
// and changing the scale reinterprets the entire visible history consistently.
class AdaptiveGraphHistory
{
public:
    struct Sample { std::uint64_t time; double value; };
    static constexpr std::uint64_t WindowMs = 120000;

    void Add(std::uint64_t now, double value, bool adaptive)
    {
        if (!std::isfinite(value) || value < 0) value = 0;
        if (!samples_.empty() && now < samples_.back().time) samples_.clear();
        const auto elapsed = samples_.empty() ? 0 : now - samples_.back().time;
        if (!samples_.empty() && now == samples_.back().time)
            samples_.back().value = value;
        else
            samples_.push_back({now, value});
        while (!samples_.empty() && now - samples_.front().time > WindowMs)
            samples_.pop_front();
        while (samples_.size() > 1024) samples_.pop_front();

        if (adaptive)
        {
            double peak = 0;
            for (const auto& sample : samples_) peak = (std::max)(peak, sample.value);
            const double target = (std::max)(1024.0, peak * 1.25);
            // Grow immediately; fall with a 15-second half-life once old peaks
            // leave the window. A small floor keeps idle noise from filling it.
            if (target >= scale_) scale_ = target;
            else scale_ = (std::max)(target, scale_ * std::exp2(-static_cast<double>(elapsed) / 15000.0));
        }
    }

    const std::deque<Sample>& Samples() const { return samples_; }
    double Scale() const { return scale_; }
    static int Percent(double value, double scale)
    {
        if (!(scale > 0) || !std::isfinite(value)) return 0;
        return static_cast<int>((std::max)(0.0, (std::min)(100.0, value * 100.0 / scale)));
    }

private:
    std::deque<Sample> samples_;
    double scale_ = 1024.0;
};
