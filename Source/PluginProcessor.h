#pragma once

#include "FilterDSP.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

class ViFilterProcessor final : public juce::AudioProcessor
{
public:
    ViFilterProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& name) override;

    void getStateInformation (juce::MemoryBlock& destination) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* outputGainParameter = nullptr; // Owned by parameters.
    std::atomic<float>* cutoffParameter = nullptr;
    std::atomic<float>* resonanceParameter = nullptr;
    std::atomic<float>* modeParameter = nullptr;

    std::array<vi::StateVariableFilter, 2> filters;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> logCutoff;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> resonance;
    std::array<juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear>, 3> modeWeights;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ViFilterProcessor)
};
