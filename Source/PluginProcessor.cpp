#include "PluginProcessor.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr auto outputGainId = "output_gain";
constexpr auto cutoffId = "cutoff";
constexpr auto resonanceId = "resonance";
constexpr auto modeId = "filter_mode";
constexpr auto driveId = "drive";
constexpr auto characterId = "analog_character";
constexpr auto oversamplingId = "oversampling";
constexpr double smoothingSeconds = 0.02;
constexpr double qualityFadeSeconds = 0.04;

int choice (const std::atomic<float>* parameter, int last) noexcept
{
    return std::clamp (static_cast<int> (parameter->load (std::memory_order_relaxed)), 0, last);
}
} // namespace

ViFilterProcessor::ViFilterProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "ViFilterParameters", createParameterLayout())
{
    outputGainParameter = parameters.getRawParameterValue (outputGainId);
    cutoffParameter = parameters.getRawParameterValue (cutoffId);
    resonanceParameter = parameters.getRawParameterValue (resonanceId);
    modeParameter = parameters.getRawParameterValue (modeId);
    driveParameter = parameters.getRawParameterValue (driveId);
    characterParameter = parameters.getRawParameterValue (characterId);
    oversamplingParameter = parameters.getRawParameterValue (oversamplingId);

    jassert (outputGainParameter != nullptr && cutoffParameter != nullptr
             && resonanceParameter != nullptr && modeParameter != nullptr
             && driveParameter != nullptr && characterParameter != nullptr
             && oversamplingParameter != nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout
ViFilterProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { modeId, 1 }, "Filter Mode",
        juce::StringArray { "Low-pass", "High-pass", "Band-pass" }, 0));

    juce::NormalisableRange<float> cutoffRange { 20.0f, 20000.0f };
    cutoffRange.setSkewForCentre (1000.0f);
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { cutoffId, 1 }, "Cutoff", cutoffRange, 1000.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { resonanceId, 1 }, "Resonance",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { driveId, 1 }, "Drive",
        juce::NormalisableRange<float> { 0.0f, 24.0f, 0.1f }, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { characterId, 1 }, "Analog Character",
        juce::StringArray { "Soft", "Tube", "Tape" }, 0));

    // Quality is switchable during playback, but not intended for host automation.
    layout.add (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { oversamplingId, 1 }, "Oversampling",
        juce::StringArray { "1x", "2x", "4x" }, 0,
        juce::AudioParameterChoiceAttributes().withAutomatable (false)));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { outputGainId, 1 }, "Output Gain",
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.1f }, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return layout;
}

void ViFilterProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxChunkSize = std::max (1, samplesPerBlock);
    for (auto& filter : filters)
        filter.reset();

    for (auto& scratch : pathBuffers)
        scratch.setSize (2, maxChunkSize, false, false, true);
    driveSamples.resize (static_cast<size_t> (maxChunkSize));
    characterSamples.resize (static_cast<size_t> (maxChunkSize));

    oversampling2.initProcessing (static_cast<size_t> (maxChunkSize));
    oversampling4.initProcessing (static_cast<size_t> (maxChunkSize));

    const auto latency2 = static_cast<int> (std::lround (oversampling2.getLatencyInSamples()));
    const auto latency4 = static_cast<int> (std::lround (oversampling4.getLatencyInSamples()));
    const auto maximumLatency = std::max (latency2, latency4);
    jassert (maximumLatency < 2048);
    alignmentSamples = { maximumLatency, maximumLatency - latency2,
                         maximumLatency - latency4 };
    setLatencySamples (maximumLatency);

    for (int quality = 0; quality < 3; ++quality)
        resetSaturationPath (quality);

    logCutoff.reset (sampleRate, smoothingSeconds);
    resonance.reset (sampleRate, smoothingSeconds);
    outputGain.reset (sampleRate, smoothingSeconds);
    driveDb.reset (sampleRate, smoothingSeconds);
    for (auto& weight : modeWeights)
        weight.reset (sampleRate, smoothingSeconds);
    for (auto& weight : characterWeights)
        weight.reset (sampleRate, smoothingSeconds);

    logCutoff.setCurrentAndTargetValue (std::log (cutoffParameter->load (std::memory_order_relaxed)));
    resonance.setCurrentAndTargetValue (resonanceParameter->load (std::memory_order_relaxed));
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (
        outputGainParameter->load (std::memory_order_relaxed)));
    driveDb.setCurrentAndTargetValue (driveParameter->load (std::memory_order_relaxed));

    const auto mode = choice (modeParameter, 2);
    const auto character = choice (characterParameter, 2);
    for (int i = 0; i < 3; ++i)
    {
        modeWeights[static_cast<size_t> (i)].setCurrentAndTargetValue (i == mode ? 1.0 : 0.0);
        characterWeights[static_cast<size_t> (i)].setCurrentAndTargetValue (i == character ? 1.0f : 0.0f);
    }
    activeQuality = choice (oversamplingParameter, 2);
    fadeFromQuality = activeQuality;
    qualityFadeSamples = std::max (1, static_cast<int> (std::lround (sampleRate * qualityFadeSeconds)));
    qualityFadeRemaining = 0;
}

void ViFilterProcessor::resetSaturationPath (int quality) noexcept
{
    for (auto& delay : alignment[static_cast<size_t> (quality)])
        delay.reset();
    if (quality == 1)
        oversampling2.reset();
    if (quality == 2)
        oversampling4.reset();
}

void ViFilterProcessor::releaseResources() {}

bool ViFilterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return input == output
        && (output == juce::AudioChannelSet::mono()
            || output == juce::AudioChannelSet::stereo());
}

void ViFilterProcessor::renderSaturationPath (int quality,
                                               const juce::AudioBuffer<float>& source,
                                               int start, int numSamples, int channels,
                                               juce::AudioBuffer<float>& destination) noexcept
{
    for (int channel = 0; channel < channels; ++channel)
        destination.copyFrom (channel, 0, source, channel, start, numSamples);

    auto base = juce::dsp::AudioBlock<float> (destination)
                    .getSubsetChannelBlock (0, static_cast<size_t> (channels))
                    .getSubBlock (0, static_cast<size_t> (numSamples));
    const auto factor = 1 << quality;
    auto* oversampler = quality == 1 ? &oversampling2
                         : quality == 2 ? &oversampling4 : nullptr;
    auto samples = oversampler != nullptr
        ? oversampler->processSamplesUp (juce::dsp::AudioBlock<const float> (base))
        : base;

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* values = samples.getChannelPointer (static_cast<size_t> (channel));
        for (int i = 0; i < static_cast<int> (samples.getNumSamples()); ++i)
        {
            const auto index = static_cast<size_t> (i / factor);
            const auto x = static_cast<double> (values[i]);
            const auto& mix = characterSamples[index];
            const auto drive = static_cast<double> (driveSamples[index]);
            if (drive <= 0.0)
                continue;

            double colored = 0.0;
            for (int character = 0; character < 3; ++character)
                if (mix[static_cast<size_t> (character)] > 0.0f)
                    colored += mix[static_cast<size_t> (character)]
                               * vi::saturate (x, drive, character);
            values[i] = static_cast<float> (colored);
        }
    }

    if (oversampler != nullptr)
        oversampler->processSamplesDown (base);

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* values = destination.getWritePointer (channel);
        auto& delay = alignment[static_cast<size_t> (quality)][static_cast<size_t> (channel)];
        for (int i = 0; i < numSamples; ++i)
            values[i] = delay.process (values[i], alignmentSamples[static_cast<size_t> (quality)]);
    }
}

void ViFilterProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    const auto inputChannels = getTotalNumInputChannels();
    const auto outputChannels = getTotalNumOutputChannels();
    for (int channel = inputChannels; channel < outputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    logCutoff.setTargetValue (std::log (cutoffParameter->load (std::memory_order_relaxed)));
    resonance.setTargetValue (resonanceParameter->load (std::memory_order_relaxed));
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (
        outputGainParameter->load (std::memory_order_relaxed)));
    driveDb.setTargetValue (driveParameter->load (std::memory_order_relaxed));

    const auto mode = choice (modeParameter, 2);
    const auto character = choice (characterParameter, 2);
    for (int i = 0; i < 3; ++i)
    {
        modeWeights[static_cast<size_t> (i)].setTargetValue (i == mode ? 1.0 : 0.0);
        characterWeights[static_cast<size_t> (i)].setTargetValue (i == character ? 1.0f : 0.0f);
    }

    const auto requestedQuality = choice (oversamplingParameter, 2);
    if (requestedQuality != activeQuality)
    {
        fadeFromQuality = activeQuality;
        activeQuality = requestedQuality;
        resetSaturationPath (activeQuality);
        qualityFadeRemaining = qualityFadeSamples;
    }

    auto coefficients = vi::makeCoefficients (getSampleRate(),
                                             std::exp (logCutoff.getCurrentValue()),
                                             resonance.getCurrentValue());
    auto channels = buffer.getArrayOfWritePointers();
    for (int start = 0; start < buffer.getNumSamples(); start += maxChunkSize)
    {
        const auto count = std::min (maxChunkSize, buffer.getNumSamples() - start);
        for (int i = 0; i < count; ++i)
        {
            const auto moving = logCutoff.isSmoothing() || resonance.isSmoothing();
            const auto cutoffLog = logCutoff.getNextValue();
            const auto res = resonance.getNextValue();
            if (moving)
                coefficients = vi::makeCoefficients (getSampleRate(), std::exp (cutoffLog), res);

            const auto low = modeWeights[0].getNextValue();
            const auto high = modeWeights[1].getNextValue();
            const auto band = modeWeights[2].getNextValue();
            driveSamples[static_cast<size_t> (i)] = driveDb.getNextValue();
            auto& characterNow = characterSamples[static_cast<size_t> (i)];
            for (int c = 0; c < 3; ++c)
                characterNow[static_cast<size_t> (c)] = characterWeights[static_cast<size_t> (c)].getNextValue();

            for (int channel = 0; channel < inputChannels; ++channel)
            {
                const auto x = static_cast<double> (channels[channel][start + i]);
                const auto filtered = filters[static_cast<size_t> (channel)].process (x, coefficients);
                channels[channel][start + i] = static_cast<float> (
                    low * filtered.low + high * filtered.high + band * filtered.band);
            }
        }

        const auto fading = qualityFadeRemaining > 0;
        renderSaturationPath (fading ? fadeFromQuality : activeQuality,
                              buffer, start, count, inputChannels, pathBuffers[0]);
        if (fading)
            renderSaturationPath (activeQuality, buffer, start, count, inputChannels, pathBuffers[1]);

        for (int i = 0; i < count; ++i)
        {
            const auto blend = qualityFadeRemaining > 0
                ? 1.0f - static_cast<float> (qualityFadeRemaining) / static_cast<float> (qualityFadeSamples)
                : 1.0f;
            if (qualityFadeRemaining > 0)
                --qualityFadeRemaining;
            const auto gain = outputGain.getNextValue();
            for (int channel = 0; channel < inputChannels; ++channel)
            {
                const auto oldSignal = pathBuffers[0].getSample (channel, i);
                const auto newSignal = fading ? pathBuffers[1].getSample (channel, i) : oldSignal;
                channels[channel][start + i] = ((1.0f - blend) * oldSignal + blend * newSignal) * gain;
            }
        }
    }
}

juce::AudioProcessorEditor* ViFilterProcessor::createEditor() { return nullptr; }
bool ViFilterProcessor::hasEditor() const { return false; }
const juce::String ViFilterProcessor::getName() const { return JucePlugin_Name; }
bool ViFilterProcessor::acceptsMidi() const { return false; }
bool ViFilterProcessor::producesMidi() const { return false; }
bool ViFilterProcessor::isMidiEffect() const { return false; }
double ViFilterProcessor::getTailLengthSeconds() const { return 0.0; }
int ViFilterProcessor::getNumPrograms() { return 1; }
int ViFilterProcessor::getCurrentProgram() { return 0; }
void ViFilterProcessor::setCurrentProgram (int index) { juce::ignoreUnused (index); }
const juce::String ViFilterProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}
void ViFilterProcessor::changeProgramName (int index, const juce::String& name)
{
    juce::ignoreUnused (index, name);
}

void ViFilterProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    if (const auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destination);
}

void ViFilterProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        const auto state = juce::ValueTree::fromXml (*xml);
        if (state.isValid() && state.hasType (parameters.state.getType()))
            parameters.replaceState (state);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ViFilterProcessor();
}
