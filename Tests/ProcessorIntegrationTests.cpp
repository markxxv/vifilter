#include "PluginProcessor.h"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>

namespace
{
constexpr double pi = 3.14159265358979323846;

void require (bool good, const char* reason)
{
    if (! good)
    {
        std::cerr << "FAIL: " << reason << '\n';
        std::exit (EXIT_FAILURE);
    }
}

juce::RangedAudioParameter& parameter (ViFilterProcessor& processor, const juce::String& name)
{
    for (auto* p : processor.getParameters())
        if (p->getName (80) == name)
            return *static_cast<juce::RangedAudioParameter*> (p);

    std::cerr << "Missing parameter: " << name << '\n';
    std::exit (EXIT_FAILURE);
}

void set (ViFilterProcessor& processor, const juce::String& name, float value)
{
    auto& p = parameter (processor, name);
    p.beginChangeGesture();
    p.setValueNotifyingHost (p.convertTo0to1 (value));
    p.endChangeGesture();
}

double get (ViFilterProcessor& processor, const juce::String& name)
{
    auto& p = parameter (processor, name);
    return p.convertFrom0to1 (p.getValue());
}

void prepare (ViFilterProcessor& processor, double rate, int block)
{
    processor.setRateAndBufferSizeDetails (rate, block);
    processor.prepareToPlay (rate, block);
    require (processor.getLatencySamples() >= 0, "Negative reported latency");
    require (processor.getLatencySamples() < 2048, "Unexpectedly high latency");
}

void testProgramsAndPersistence()
{
    ViFilterProcessor processor;
    require (processor.getNumPrograms() == static_cast<int> (vi::presets::factory.size()) + 1,
             "Wrong factory program count");
    require (processor.getProgramName (0) == "Custom", "No Custom bank entry");
    require (processor.getProgramName (4) == "Warm Tape", "Factory order changed");

    processor.setCurrentProgram (4);
    require (processor.getCurrentProgram() == 4,
             "Applying factory program should not mark itself Custom");
    require (std::abs (get (processor, "Drive") - 8.0) < 0.051,
             "Factory drive was not applied");
    require (std::abs (get (processor, "Cutoff") - 10500.0) < 3.0,
             "Factory cutoff was not applied");

    juce::MemoryBlock saved;
    processor.getStateInformation (saved);
    require (saved.getSize() > 100, "No state saved");

    ViFilterProcessor restored;
    restored.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    require (restored.getCurrentProgram() == 4, "Factory selection lost after reload");
    require (std::abs (get (restored, "Drive") - 8.0) < 0.051,
             "Parameter value lost after reload");

    set (restored, "Drive", 13.0f);
    require (restored.getCurrentProgram() == 0,
             "Editing a factory program must switch to Custom");
    require (std::abs (get (restored, "Drive") - 13.0) < 0.051,
             "Editing a factory program changed the wrong value");

    juce::MemoryBlock custom;
    restored.getStateInformation (custom);
    ViFilterProcessor customRestored;
    customRestored.setStateInformation (custom.getData(), static_cast<int> (custom.getSize()));
    require (customRestored.getCurrentProgram() == 0,
             "Custom selection must survive a roundtrip");
    require (std::abs (get (customRestored, "Drive") - 13.0) < 0.051,
             "Custom drive must survive a roundtrip");

    const std::array<unsigned char, 8> broken {{ 0xde, 0xad, 0xbe, 0xef, 1, 2, 3, 4 }};
    customRestored.setStateInformation (broken.data(), static_cast<int> (broken.size()));
    require (std::abs (get (customRestored, "Drive") - 13.0) < 0.051,
             "Corrupt host state must preserve current parameters");

    processor.setCurrentProgram (-1);
    require (processor.getCurrentProgram() == 4, "Invalid negative program selected");
    processor.setCurrentProgram (999);
    require (processor.getCurrentProgram() == 4, "Invalid out-of-range program selected");
}

void testAudioAndAutomation (double rate, int channels)
{
    ViFilterProcessor processor;
    auto layout = processor.getBusesLayout();
    layout.inputBuses.getReference (0) = channels == 1
        ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
    layout.outputBuses.getReference (0) = layout.inputBuses[0];
    require (processor.setBusesLayout (layout), "Supported bus layout was rejected");

    // Deliberately choose a smaller preparation block than some process blocks.
    prepare (processor, rate, 64);
    const auto fixedLatency = processor.getLatencySamples();

    set (processor, "Drive", 15.0f);
    juce::MidiBuffer midi;
    const std::array<int, 6> blockSizes {{ 1, 7, 64, 97, 256, 503 }};

    int sampleCursor = 0;
    for (int step = 0; step < 180; ++step)
    {
        set (processor, "Filter Mode", static_cast<float> (step % 3));
        set (processor, "Analog Character", static_cast<float> (step % 3));
        set (processor, "Oversampling", static_cast<float> (step % 3));
        set (processor, "Resonance", static_cast<float> (step % 100) / 99.0f);
        set (processor, "Cutoff", 40.0f + static_cast<float> ((step * 317) % 19000));
        set (processor, "Drive", static_cast<float> ((step * 3) % 25));

        require (processor.getLatencySamples() == fixedLatency,
                 "Oversampling switch changed reported latency");

        const auto count = blockSizes[static_cast<size_t> (step) % blockSizes.size()];
        juce::AudioBuffer<float> audio (channels, count);
        for (int i = 0; i < count; ++i)
        {
            const auto t = static_cast<double> (sampleCursor + i) / rate;
            audio.setSample (0, i, static_cast<float> (
                0.35 * std::sin (2.0 * pi * 830.0 * t)));
            if (channels == 2)
                audio.setSample (1, i, 0.0f);
        }

        midi.clear();
        processor.processBlock (audio, midi);

        for (int channel = 0; channel < channels; ++channel)
            for (int i = 0; i < count; ++i)
            {
                const auto v = audio.getSample (channel, i);
                require (std::isfinite (v) && std::abs (v) < 200.0f,
                         "Realtime processing produced unstable output");
                if (channel == 1)
                    require (v == 0.0f, "Signal leaked between stereo channels");
            }
        sampleCursor += count;
    }

    // A new sample rate must reinitialize FIR/delays and adapt host latency.
    processor.releaseResources();
    prepare (processor, rate == 96000.0 ? 44100.0 : 96000.0, 512);
    juce::AudioBuffer<float> silence (channels, 512);
    silence.clear();
    processor.processBlock (silence, midi);
    for (int channel = 0; channel < channels; ++channel)
        for (int i = 0; i < 512; ++i)
            require (std::isfinite (silence.getSample (channel, i)),
                     "Rate transition produced nonfinite output");
}

double benchmark (int quality)
{
    constexpr double sampleRate = 48000.0;
    constexpr int block = 256;
    constexpr int iterations = 240;

    ViFilterProcessor processor;
    set (processor, "Oversampling", static_cast<float> (quality));
    set (processor, "Drive", 18.0f);
    set (processor, "Analog Character", 1.0f);
    prepare (processor, sampleRate, block);

    juce::AudioBuffer<float> audio (2, block);
    juce::MidiBuffer midi;
    double checksum = 0.0;
    const auto begin = std::chrono::steady_clock::now();
    for (int n = 0; n < iterations; ++n)
    {
        for (int i = 0; i < block; ++i)
        {
            const auto x = static_cast<float> (0.4 * std::sin (
                2.0 * pi * 1200.0 * (n * block + i) / sampleRate));
            audio.setSample (0, i, x);
            audio.setSample (1, i, -x);
        }
        processor.processBlock (audio, midi);
        checksum += audio.getSample (0, block - 1);
    }
    const auto end = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration<double> (end - begin).count();
    require (std::isfinite (checksum) && std::isfinite (elapsed) && elapsed > 0.0,
             "Benchmark failed to process signal");
    const auto audioSeconds = iterations * block / sampleRate;
    std::cout << "Oversampling " << (1 << quality) << "x: "
              << (elapsed * 1000.0) << " ms CPU for " << audioSeconds
              << " s audio; realtime factor " << audioSeconds / elapsed << "x\n";
    return elapsed;
}
} // namespace

int main()
{
    testProgramsAndPersistence();
    for (const auto rate : { 44100.0, 48000.0, 96000.0 })
        for (int channels : { 1, 2 })
            testAudioAndAutomation (rate, channels);

    const auto normal = benchmark (0);
    const auto highQuality = benchmark (2);
    std::cout << "4x/1x CPU cost: " << highQuality / normal << "x\n";
    std::cout << "Processor integration tests passed\n";
}
