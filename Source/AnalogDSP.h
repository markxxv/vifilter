#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace vi
{
// Prepare transfer coefficients once per distinct Drive value instead of doing
// expensive pow/atan/tanh normalizations for each oversampled channel sample.
struct SaturationParameters
{
    double amount = 0.0;
    double gain = 1.0;
    double softNorm = 1.0;
    double tubeNorm = 1.0;
    double tapeNorm = 1.0;
};

inline SaturationParameters makeSaturationParameters (double driveDb) noexcept
{
    const auto amount = std::clamp (driveDb / 24.0, 0.0, 1.0);
    if (amount <= 0.0)
        return {};

    constexpr double bias = 0.35;
    const auto gain = std::pow (10.0, std::clamp (driveDb, 0.0, 24.0) / 20.0);
    const auto softNorm = 1.0 / std::tanh (gain);
    const auto tubeNorm = 1.0 / (std::tanh (gain + bias) - std::tanh (bias));
    const auto tapeNorm = 1.0 / std::atan (gain);
    return { amount, gain, softNorm, tubeNorm, tapeNorm };
}

inline double saturateWithParameters (double input,
                                     const SaturationParameters& p,
                                     int character) noexcept
{
    if (p.amount <= 0.0)
        return input;

    double shaped;
    switch (character)
    {
        case 1:
        {
            constexpr double bias = 0.35;
            shaped = (std::tanh (p.gain * input + bias) - std::tanh (bias)) * p.tubeNorm;
            break;
        }
        case 2:
            shaped = std::atan (p.gain * input) * p.tapeNorm;
            break;
        default:
            shaped = std::tanh (p.gain * input) * p.softNorm;
            break;
    }
    return input + p.amount * (shaped - input);
}

// Retain the stateless entry point for independent tests and external DSP users.
inline double saturate (double input, double driveDb, int character) noexcept
{
    return saturateWithParameters (input, makeSaturationParameters (driveDb), character);
}

template <size_t capacity = 2048>
class SampleDelay
{
public:
    void reset() noexcept
    {
        values.fill (0.0f);
        position = 0;
    }

    float process (float input, int delaySamples) noexcept
    {
        const auto delay = static_cast<size_t> (std::clamp (delaySamples, 0,
                                                              static_cast<int> (capacity - 1)));
        values[position] = input;
        const auto read = (position + capacity - delay) % capacity;
        const auto output = values[read];
        position = (position + 1) % capacity;
        return output;
    }

private:
    std::array<float, capacity> values {};
    size_t position = 0;
};
} // namespace vi
