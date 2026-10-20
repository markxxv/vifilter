#pragma once
#include "FilterDSP.h"
#include <algorithm>
#include <cmath>
#include <complex>

namespace vi::response
{
constexpr double pi = 3.14159265358979323846;

// Exact linear transfer functions of the TPT SVF via bilinear transform.
// Deliberately excludes nonlinear saturation, which has no fixed transfer curve.
inline double magnitude (double sampleRate, double frequency, double cutoff,
                         double resonance, int mode, double mix,
                         double inputTrimDb, double outputGainDb) noexcept
{
    const auto fs = std::max (sampleRate, 8000.0);
    const auto f = std::clamp (frequency, 1.0, fs * 0.499);
    const auto fc = std::clamp (cutoff, 20.0, fs * 0.45);
    const auto qPosition = std::clamp (resonance, 0.0, 1.0);
    const auto q = 0.5 + 11.5 * qPosition * qPosition;
    const auto k = 1.0 / q;
    const auto g = std::tan (pi * fc / fs);
    const auto phase = 2.0 * pi * f / fs;
    const std::complex<double> z { std::cos (phase), -std::sin (phase) };
    const auto plus = 1.0 + z;
    const auto minus = 1.0 - z;
    const auto denominator = minus * minus + k * g * minus * plus + g * g * plus * plus;
    const auto low = (g * g * plus * plus) / denominator;
    const auto band = (g * minus * plus) / denominator;
    const auto high = (minus * minus) / denominator;
    const auto wet = mode == 1 ? high : mode == 2 ? band : low;
    const auto blend = std::clamp (mix, 0.0, 1.0);
    const auto total = (1.0 - blend) + blend * wet;
    const auto gain = std::pow (10.0, (inputTrimDb + outputGainDb) / 20.0);
    return std::abs (total) * gain;
}
} // namespace vi::response
