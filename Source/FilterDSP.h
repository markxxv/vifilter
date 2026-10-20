#pragma once

#include <algorithm>
#include <cmath>

namespace vi
{
// Topology-preserving (TPT) state-variable filter; computes all three modes.
struct FilterCoefficients
{
    double a1 = 1.0;
    double a2 = 0.0;
    double a3 = 0.0;
    double k = 1.0;
};

inline FilterCoefficients makeCoefficients (double sampleRate,
                                            double cutoffHz,
                                            double resonance) noexcept
{
    const auto fs = std::max (sampleRate, 8000.0);
    const auto cutoff = std::clamp (cutoffHz, 20.0, fs * 0.45);
    const auto r = std::clamp (resonance, 0.0, 1.0);
    const auto q = 0.5 + 11.5 * r * r;
    constexpr auto pi = 3.14159265358979323846;
    const auto g = std::tan (pi * cutoff / fs);
    const auto k = 1.0 / q;
    const auto a1 = 1.0 / (1.0 + g * (g + k));
    const auto a2 = g * a1;
    return { a1, a2, g * a2, k };
}

struct FilterOutputs
{
    double low = 0.0;
    double band = 0.0;
    double high = 0.0;
};

class StateVariableFilter
{
public:
    void reset() noexcept { s1 = 0.0; s2 = 0.0; }

    FilterOutputs process (double input, const FilterCoefficients& c) noexcept
    {
        const auto v3 = input - s2;
        const auto band = c.a1 * s1 + c.a2 * v3;
        const auto low = s2 + c.a2 * s1 + c.a3 * v3;
        s1 = 2.0 * band - s1;
        s2 = 2.0 * low - s2;
        return { low, band, input - c.k * band - low };
    }

private:
    double s1 = 0.0;
    double s2 = 0.0;
};
} // namespace vi
