#pragma once

#include "PluginProcessor.h"
#include "Typography.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <array>

class ViFilterEditor final : public juce::AudioProcessorEditor,
                             private juce::Timer
{
public:
    explicit ViFilterEditor (ViFilterProcessor&);
    ~ViFilterEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    // Render a fresh analyzer frame on the message thread, including headless previews.
    // Avoid modal message loops: they may be disabled by JUCE build configuration.
    void refreshAnalyzerNow();

private:
    static constexpr float designWidth = 1440.0f;
    static constexpr float designHeight = 820.0f;
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;

    struct Knob
    {
        const char* id;
        const char* title;
        float x, y, radius;
    };
    static const std::array<Knob, 6> knobLayout;

    void timerCallback() override;
    void paintPanel (juce::Graphics&);
    void paintHeader (juce::Graphics&);
    void paintResponse (juce::Graphics&);
    void paintControls (juce::Graphics&);
    void paintKnob (juce::Graphics&, const Knob&);
    void paintButton (juce::Graphics&, juce::Rectangle<float>, const juce::String&,
                      bool selected = false, float fontSize = 13.0f);
    void paintSaveOverlay (juce::Graphics&);

    juce::Point<float> point (const juce::MouseEvent&) const noexcept;
    float scaleX() const noexcept { return getWidth() / designWidth; }
    float scaleY() const noexcept { return getHeight() / designHeight; }
    float cutoffToX (float cutoff) const noexcept;
    float xToCutoff (float x) const noexcept;
    float frequencyY (double frequency) const noexcept;
    float responseDb (double frequency) const noexcept;
    float currentValue (const char* id) const noexcept;
    void setParameter (const char* id, float value);
    juce::RangedAudioParameter* getParameter (const char* id) const;
    int knobAt (juce::Point<float>) const noexcept;
    void updateAnalyzer();
    void openPresetMenu();
    void applyPresetSelection (int);
    void showSaveDialog();
    void commitSave();
    void switchAB (bool selectA);
    void formatPresetName();
    juce::String formatKnobValue (const Knob&) const;

    ViFilterProcessor& processor;
    class DINLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        juce::Font getPopupMenuFont() override
        {
            return vi::typography::font (16.0f, vi::typography::Weight::regular);
        }
    };
    DINLookAndFeel darkLook;
    juce::TextEditor saveName;
    juce::String displayedPreset { "Custom" };
    juce::String feedback;
    int feedbackTicks = 0;
    bool saving = false;
    bool showSpectrum = true;
    bool linearScale = false;
    bool comparingA = true;
    juce::MemoryBlock snapshotA, snapshotB;
    juce::Array<juce::File> userFiles;
    int activeKnob = -1;
    bool draggingResponse = false;
    float dragStartY = 0.0f;
    float dragStartValue = 0.0f;
    juce::RangedAudioParameter* activeParameter = nullptr;

    // Single consumer on message thread. DSP writes into its lock-free FIFO.
    juce::dsp::FFT fft { fftOrder };
    std::array<float, fftSize> fftHistory {};
    std::array<float, fftSize * 2> fftWorkspace {};
    std::array<float, fftSize / 2 + 1> spectrum {};
    int historyWritePosition = 0;
    float measuredLevel = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ViFilterEditor)
};
