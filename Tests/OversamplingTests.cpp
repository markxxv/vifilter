#include "AnalogDSP.h"

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr int blockSize = 128;
constexpr int totalSamples = 8192;
constexpr double sampleRate = 48000.0;

std::vector<float> render (int factorExponent)
{
    juce::dsp::Oversampling<float> oversampler (
        1, static_cast<size_t> (factorExponent),
        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
        true, true);
    oversampler.initProcessing (blockSize);

    juce::AudioBuffer<float> buffer (1, blockSize);
    std::vector<float> rendered;
    rendered.reserve (totalSamples);
    for (int blockIndex = 0; blockIndex < totalSamples / blockSize; ++blockIndex)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            const auto sample = blockIndex * blockSize + i;
            buffer.setSample (0, i, static_cast<float> (
                0.5 * std::sin (2.0 * pi * 9000.0 * sample / sampleRate)));
        }
        auto base = juce::dsp::AudioBlock<float> (buffer);
        if (factorExponent > 0)
        {
            auto high = oversampler.processSamplesUp (juce::dsp::AudioBlock<const float> (base));
            for (size_t i = 0; i < high.getNumSamples(); ++i)
                high.setSample (0, i, static_cast<float> (
                    vi::saturate (high.getSample (0, i), 24.0, 0)));
            oversampler.processSamplesDown (base);
        }
        else
        {
            for (int i = 0; i < blockSize; ++i)
                buffer.setSample (0, i, static_cast<float> (
                    vi::saturate (buffer.getSample (0, i), 24.0, 0)));
        }
        for (int i = 0; i < blockSize; ++i)
            rendered.push_back (buffer.getSample (0, i));
    }
    return rendered;
}

double frequencyAmplitude (const std::vector<float>& samples, double frequency)
{
    double cosine = 0.0;
    double sine = 0.0;
    constexpr int window = 4096;
    for (int i = 0; i < window; ++i)
    {
        const auto x = samples[static_cast<size_t> (totalSamples - window + i)];
        const auto phase = 2.0 * pi * frequency * i / sampleRate;
        cosine += x * std::cos (phase);
        sine += x * std::sin (phase);
    }
    return 2.0 * std::hypot (cosine, sine) / window;
}
} // namespace

int main()
{
    const auto normal = render (0);
    const auto twice = render (1);
    const auto fourTimes = render (2);
    const auto alias1 = frequencyAmplitude (normal, 21000.0);
    const auto alias2 = frequencyAmplitude (twice, 21000.0);
    const auto alias4 = frequencyAmplitude (fourTimes, 21000.0);
    std::cout << "21kHz alias of 9kHz input: 1x=" << alias1
              << " 2x=" << alias2 << " 4x=" << alias4 << '\n';
    if (! std::isfinite (alias4) || alias1 < 0.001
        || alias2 > alias1 * 0.6 || alias4 > alias1 * 0.25)
    {
        std::cerr << "FAIL: oversampling does not suppress folded third harmonic\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
