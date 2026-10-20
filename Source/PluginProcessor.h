#pragma once

#include "AnalogDSP.h"
#include "FilterDSP.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

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
    void renderSaturationPath (int quality, const juce::AudioBuffer<float>& source,
                               int start, int numSamples, int channels,
                               juce::AudioBuffer<float>& destination) noexcept;
    void resetSaturationPath (int quality) noexcept;

    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* outputGainParameter = nullptr;
    std::atomic<float>* cutoffParameter = nullptr;
    std::atomic<float>* resonanceParameter = nullptr;
    std::atomic<float>* modeParameter = nullptr;
    std::atomic<float>* driveParameter = nullptr;
    std::atomic<float>* characterParameter = nullptr;
    std::atomic<float>* oversamplingParameter = nullptr;

    std::array<vi::StateVariableFilter, 2> filters;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> logCutoff;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> resonance;
    std::array<juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear>, 3> modeWeights;
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>, 3> characterWeights;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> driveDb;

    // Both oversamplers and scratch buffers are allocated outside the audio callback.
    using Oversampler = juce::dsp::Oversampling<float>;
    Oversampler oversampling2 { 2, 1, Oversampler::filterHalfBandFIREquiripple, true, true };
    Oversampler oversampling4 { 2, 2, Oversampler::filterHalfBandFIREquiripple, true, true };
    std::array<std::array<vi::SampleDelay<2048>, 2>, 3> alignment;
    std::array<int, 3> alignmentSamples {};
    std::array<juce::AudioBuffer<float>, 2> pathBuffers;
    std::vector<float> driveSamples;
    std::vector<std::array<float, 3>> characterSamples;
    int maxChunkSize = 1;
    int activeQuality = 0;
    int fadeFromQuality = 0;
    int qualityFadeSamples = 1;
    int qualityFadeRemaining = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ViFilterProcessor)
};
