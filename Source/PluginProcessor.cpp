#include "PluginProcessor.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr auto outputGainId = "output_gain";
constexpr auto cutoffId = "cutoff";
constexpr auto resonanceId = "resonance";
constexpr auto modeId = "filter_mode";
constexpr double smoothingSeconds = 0.02;

int currentMode (const std::atomic<float>* parameter) noexcept
{
    return std::clamp (static_cast<int> (parameter->load (std::memory_order_relaxed)), 0, 2);
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

    jassert (outputGainParameter != nullptr && cutoffParameter != nullptr
             && resonanceParameter != nullptr && modeParameter != nullptr);
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
        juce::ParameterID { outputGainId, 1 }, "Output Gain",
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.1f }, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return layout;
}

void ViFilterProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    for (auto& filter : filters)
        filter.reset();

    logCutoff.reset (sampleRate, smoothingSeconds);
    resonance.reset (sampleRate, smoothingSeconds);
    outputGain.reset (sampleRate, smoothingSeconds);
    for (auto& weight : modeWeights)
        weight.reset (sampleRate, smoothingSeconds);

    logCutoff.setCurrentAndTargetValue (std::log (cutoffParameter->load (std::memory_order_relaxed)));
    resonance.setCurrentAndTargetValue (resonanceParameter->load (std::memory_order_relaxed));
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (
        outputGainParameter->load (std::memory_order_relaxed)));

    const auto selected = currentMode (modeParameter);
    for (int i = 0; i < static_cast<int> (modeWeights.size()); ++i)
        modeWeights[static_cast<size_t> (i)].setCurrentAndTargetValue (i == selected ? 1.0 : 0.0);
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

    const auto selected = currentMode (modeParameter);
    for (int i = 0; i < static_cast<int> (modeWeights.size()); ++i)
        modeWeights[static_cast<size_t> (i)].setTargetValue (i == selected ? 1.0 : 0.0);

    // One coefficient set and one set of mode gains per sample, shared by
    // both channels. The processing callback makes no heap allocations.
    auto coefficients = vi::makeCoefficients (getSampleRate(),
                                               std::exp (logCutoff.getCurrentValue()),
                                               resonance.getCurrentValue());
    auto samples = buffer.getArrayOfWritePointers();
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto isMoving = logCutoff.isSmoothing() || resonance.isSmoothing();
        const auto cutoffLogNow = logCutoff.getNextValue();
        const auto resonanceNow = resonance.getNextValue();
        if (isMoving)
            coefficients = vi::makeCoefficients (getSampleRate(),
                                                   std::exp (cutoffLogNow), resonanceNow);

        const auto lowWeight = modeWeights[0].getNextValue();
        const auto highWeight = modeWeights[1].getNextValue();
        const auto bandWeight = modeWeights[2].getNextValue();
        const auto gain = outputGain.getNextValue();

        for (int channel = 0; channel < inputChannels; ++channel)
        {
            const auto sampleIn = static_cast<double> (samples[channel][sample]);
            const auto response = filters[static_cast<size_t> (channel)].process (sampleIn, coefficients);
            const auto sampleOut = lowWeight * response.low
                                 + highWeight * response.high
                                 + bandWeight * response.band;
            samples[channel][sample] = static_cast<float> (sampleOut * gain);
        }
    }
}

juce::AudioProcessorEditor* ViFilterProcessor::createEditor()
{
    // Host's generic parameter view is used until final visual design phase.
    return nullptr;
}

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
