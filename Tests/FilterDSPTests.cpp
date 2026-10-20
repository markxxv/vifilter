#include "FilterDSP.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
constexpr double pi = 3.14159265358979323846;

void check (bool condition, const char* message)
{
    if (! condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit (1);
    }
}

double measureRms (double frequency, double cutoff, int outputIndex)
{
    constexpr double fs = 48000.0;
    vi::StateVariableFilter filter;
    const auto coefficients = vi::makeCoefficients (fs, cutoff, 0.0);
    double sum = 0.0;
    constexpr int samples = 48000;

    for (int i = 0; i < samples; ++i)
    {
        const auto x = std::sin (2.0 * pi * frequency * i / fs);
        const auto y = filter.process (x, coefficients);
        const auto v = outputIndex == 0 ? y.low : outputIndex == 1 ? y.band : y.high;
        if (i >= samples / 2)
            sum += v * v;
    }
    return std::sqrt (sum / (samples / 2));
}

void testFrequencyResponse()
{
    check (measureRms (100, 800, 0) > 0.6, "LP should pass bass");
    check (measureRms (8000, 800, 0) < 0.03, "LP should attenuate highs");
    check (measureRms (100, 2000, 2) < 0.01, "HP should attenuate bass");
    check (measureRms (8000, 2000, 2) > 0.6, "HP should pass highs");
    check (measureRms (800, 800, 1) > measureRms (100, 800, 1) * 2.0,
           "BP should emphasize center vs lows");
    check (measureRms (800, 800, 1) > measureRms (8000, 800, 1) * 2.0,
           "BP should emphasize center vs highs");
}

void testDcAndChannels()
{
    const auto c = vi::makeCoefficients (48000.0, 1200.0, 0.0);
    vi::StateVariableFilter left, right;
    vi::FilterOutputs result;
    for (int i = 0; i < 48000; ++i)
    {
        result = left.process (1.0, c);
        auto silence = right.process (0.0, c);
        check (silence.low == 0.0 && silence.band == 0.0 && silence.high == 0.0,
               "one channel must not leak to another");
    }
    check (std::abs (result.low - 1.0) < 1.0e-7, "LP must pass DC");
    check (std::abs (result.high) < 1.0e-7, "HP must reject DC");
    left.reset();
    result = left.process (0.0, c);
    check (result.low == 0.0 && result.band == 0.0 && result.high == 0.0,
           "reset must clear filter history");
}

void testStress()
{
    for (const auto fs : { 8000.0, 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        vi::StateVariableFilter filter;
        for (int i = 0; i < 100000; ++i)
        {
            const auto fraction = static_cast<double> (i % 10000) / 9999.0;
            const auto c = vi::makeCoefficients (fs, 20.0 + fraction * 20000.0,
                                                   static_cast<double> (i % 1000) / 999.0);
            const auto x = std::sin (2.0 * pi * 1357.0 * i / fs);
            const auto y = filter.process (x, c);
            check (std::isfinite (y.low) && std::isfinite (y.band)
                    && std::isfinite (y.high), "DSP must stay finite on sweeps");
            check (std::abs (y.low) < 100.0 && std::abs (y.band) < 100.0
                    && std::abs (y.high) < 100.0, "DSP must stay bounded");
        }
    }
}
} // namespace

int main()
{
    testFrequencyResponse();
    testDcAndChannels();
    testStress();
    std::cout << "Vi Filter DSP tests passed\n";
}
