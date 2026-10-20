#include "PluginProcessor.h"

namespace
{
constexpr auto outputGainId = "output_gain";
constexpr float minGainDb = -24.0f;
constexpr float maxGainDb = 24.0f;
constexpr double gainSmoothingSeconds = 0.02;
}

ViFilterProcessor::ViFilterProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "ViFilterParameters", createParameterLayout())
{
    outputGainParameter = parameters.getRawParameterValue (outputGainId);
    jassert (outputGainParameter != nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout
ViFilterProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { outputGainId, 1 },
        "Output Gain",
        juce::NormalisableRange<float> { minGainDb, maxGainDb, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return layout;
}

void ViFilterProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    outputGain.reset (sampleRate, gainSmoothingSeconds);
    const auto decibels = outputGainParameter->load (std::memory_order_relaxed);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (decibels));
}

void ViFilterProcessor::releaseResources()
{
}

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

    const auto numInputChannels = getTotalNumInputChannels();
    const auto numOutputChannels = getTotalNumOutputChannels();

    for (int channel = numInputChannels; channel < numOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    const auto decibels = outputGainParameter->load (std::memory_order_relaxed);
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (decibels));

    // A single smoothing step per sample keeps channels phase-coherent.
    auto** channels = buffer.getArrayOfWritePointers();
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto gain = outputGain.getNextValue();

        for (int channel = 0; channel < numInputChannels; ++channel)
            channels[channel][sample] *= gain;
    }
}

juce::AudioProcessorEditor* ViFilterProcessor::createEditor()
{
    // The visual design is intentionally deferred to the final project phase.
    return nullptr;
}

bool ViFilterProcessor::hasEditor() const
{
    return false;
}

const juce::String ViFilterProcessor::getName() const
{
    return JucePlugin_Name;
}

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
