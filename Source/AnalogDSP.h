#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace vi
{
// Digital waveshapers. Every character is exactly transparent at 0 dB.
inline double saturate (double input, double driveDb, int character) noexcept
{
    const auto amount = std::clamp (driveDb / 24.0, 0.0, 1.0);
    if (amount <= 0.0)
        return input;
    const auto gain = std::pow (10.0, std::clamp (driveDb, 0.0, 24.0) / 20.0);
    double shaped;
    switch (character)
    {
        case 1:
        {
            constexpr double bias = 0.35;
            const auto zero = std::tanh (bias);
            const auto scale = std::tanh (gain + bias) - zero;
            shaped = (std::tanh (gain * input + bias) - zero) / scale;
            break;
        }
        case 2:
            shaped = std::atan (gain * input) / std::atan (gain);
            break;
        default:
            shaped = std::tanh (gain * input) / std::tanh (gain);
            break;
    }
    return input + amount * (shaped - input);
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
