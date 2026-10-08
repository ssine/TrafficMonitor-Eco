#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>

// Raw samples share a two-minute time axis. Repainting never adds samples,
// and changing the scale reinterprets the entire visible history consistently.
class AdaptiveGraphHistory
{
public:
    struct Sample { std::uint64_t time; double value; double plottedValue; };
    static constexpr std::uint64_t WindowMs = 120000;
    static constexpr double NetworkSmoothingMs = 3000.0;

    void Add(std::uint64_t now, double value, bool adaptive, double minimumScale = 1024.0,
        double smoothingMs = 0.0, std::uint64_t maximumGapMs = WindowMs)
    {
        // Preserve missing readings so the renderer leaves a gap.
        if (!std::isfinite(value)) value = std::numeric_limits<double>::quiet_NaN();
        else if (value < 0) value = 0;
        if (!std::isfinite(minimumScale) || minimumScale <= 0) minimumScale = 1024.0;
        if (!samples_.empty() && now < samples_.back().time) samples_.clear();
        if (samples_.empty()) scale_ = minimumScale;
        adaptive_ = adaptive;
        const auto elapsed = samples_.empty() ? 0 : now - samples_.back().time;
        const bool replace = !samples_.empty() && now == samples_.back().time;
        // Replacing a timestamp starts from the previous distinct sample, so
        // duplicate updates cannot repeatedly advance the display filter.
        const auto previous = samples_.size() - (replace ? 1 : 0);
        double plottedValue = value;
        if (previous > 0 && std::isfinite(value) && std::isfinite(smoothingMs) && smoothingMs > 0)
        {
            const auto& last = samples_[previous - 1];
            const auto dt = now - last.time;
            if (std::isfinite(last.plottedValue) && dt <= maximumGapMs)
            {
                const double alpha = -std::expm1(-static_cast<double>(dt) / smoothingMs);
                plottedValue = last.plottedValue + alpha * (value - last.plottedValue);
            }
        }
        if (replace)
            samples_.back() = {now, value, plottedValue};
        else
            samples_.push_back({now, value, plottedValue});
        while (!samples_.empty() && now - samples_.front().time > WindowMs)
            samples_.pop_front();
        while (samples_.size() > 1024) samples_.pop_front();

        if (adaptive)
        {
            double peak = 0;
            for (const auto& sample : samples_) peak = (std::max)(peak, sample.value);
            const double target = (std::max)(minimumScale, peak * 1.25);
            // Grow immediately; fall with a 15-second half-life once old peaks
            // leave the window. A small floor keeps idle noise from filling it.
            if (target >= scale_) scale_ = target;
            else scale_ = (std::max)(target, scale_ * std::exp2(-static_cast<double>(elapsed) / 15000.0));
        }
    }

    const std::deque<Sample>& Samples() const { return samples_; }
    double Scale() const { return scale_; }
    bool IsAdaptive() const { return adaptive_; }
    // Only the network history renderer uses interpolation. Missing samples
    // and long pauses leave gaps instead of inventing a connecting slope.
    static double Interpolate(const Sample& left, const Sample& right,
        std::uint64_t time, std::uint64_t maximumGapMs)
    {
        if (time == left.time) return left.plottedValue;
        if (time == right.time) return right.plottedValue;
        if (right.time <= left.time || time < left.time || time > right.time ||
            right.time - left.time > maximumGapMs ||
            !std::isfinite(left.plottedValue) || !std::isfinite(right.plottedValue))
            return std::numeric_limits<double>::quiet_NaN();
        const double fraction = static_cast<double>(time - left.time) / (right.time - left.time);
        return left.plottedValue + fraction * (right.plottedValue - left.plottedValue);
    }
    static int PixelHeight(double value, double scale, int pixels)
    {
        if (!(scale > 0) || !std::isfinite(value) || pixels <= 0) return 0;
        const double fraction = (std::max)(0.0, (std::min)(1.0, value / scale));
        return static_cast<int>(std::lround(fraction * pixels));
    }
    static int Percent(double value, double scale)
    {
        if (!(scale > 0) || !std::isfinite(value)) return 0;
        return static_cast<int>((std::max)(0.0, (std::min)(100.0, value * 100.0 / scale)));
    }

private:
    std::deque<Sample> samples_;
    double scale_ = 1024.0;
    bool adaptive_ = false;
};
