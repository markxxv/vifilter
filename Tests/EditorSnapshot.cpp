#include "PluginEditor.h"

#include <juce_gui_extra/juce_gui_extra.h>
#include <iostream>
#include <cmath>
#include <memory>

int main()
{
    juce::ScopedJuceInitialiser_GUI initialise;
    ViFilterProcessor processor;
    processor.setRateAndBufferSizeDetails (48000.0, 512);
    processor.prepareToPlay (48000.0, 512);

    if (! vi::typography::bundledFacesAvailable())
    {
        std::cerr << "Bundled DINish Light, Regular, or Medium font failed to load\n";
        return 1;
    }

    auto editor = std::unique_ptr<juce::AudioProcessorEditor> (processor.createEditor());
    // Compact default is deliberate: Logic adds a substantial host toolbar.
    if (editor == nullptr || editor->getWidth() > 1000 || editor->getHeight() > 560
        || editor->getWidth() < 880 || editor->getHeight() < 493)
    {
        std::cerr << "Vi Filter must open in a compact, laptop-friendly size\n";
        return 1;
    }

    // Feed a deterministic multi-tone test signal through the *real processor*
    // so the offscreen preview shows a non-empty, real FFT analyzer trace.
    // Production editor content never generates a synthetic spectrum.
    const auto set = [&processor] (const char* id, float raw)
    {
        if (auto* p = processor.parameterState().getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (raw));
    };
    set ("cutoff", 1240.0f);
    set ("resonance", 0.62f);
    set ("drive", 4.3f);
    set ("oversampling", 1.0f);

    juce::AudioBuffer<float> audio (2, 512);
    juce::MidiBuffer midi;
    constexpr auto tau = 6.28318530717958647692;
    for (int block = 0; block < 80; ++block)
    {
        for (int i = 0; i < 512; ++i)
        {
            const auto t = static_cast<double> (block * 512 + i) / 48000.0;
            const auto signal = 0.13 * std::sin (tau * 95 * t)
                              + 0.10 * std::sin (tau * 210 * t)
                              + 0.12 * std::sin (tau * 410 * t)
                              + 0.10 * std::sin (tau * 750 * t)
                              + 0.05 * std::sin (tau * 1750 * t)
                              + 0.03 * std::sin (tau * 3300 * t);
            audio.setSample (0, i, static_cast<float> (signal));
            audio.setSample (1, i, static_cast<float> (signal));
        }
        midi.clear();
        processor.processBlock (audio, midi);
    }
    // Compute one real FFT frame synchronously on the message thread.
    // JUCE_MODAL_LOOPS_PERMITTED may be disabled on some macOS/Xcode builds.
    static_cast<ViFilterEditor*> (editor.get())->refreshAnalyzerNow();

    // Exercise the minimum laptop size and the new 1000 x 560 design grid.
    for (const auto dimensions : { std::pair<int, int> { 880, 493 },
                                   std::pair<int, int> { 1000, 560 } })
    {
        editor->setSize (dimensions.first, dimensions.second);
        juce::Image image (juce::Image::ARGB, dimensions.first, dimensions.second, true);
        juce::Graphics graphics (image);
        editor->paintEntireComponent (graphics, true);

        const auto target = juce::File::getCurrentWorkingDirectory().getChildFile (
            dimensions.first == 1000 ? "ViFilter_UI_Preview.png" : "ViFilter_UI_Compact.png");
        juce::PNGImageFormat png;
        auto stream = target.createOutputStream();
        if (stream == nullptr || ! png.writeImageToStream (image, *stream))
        {
            std::cerr << "Could not render offscreen JUCE editor\n";
            return 1;
        }
        stream->flush();
    }
    std::cout << "Editor rendered at 880x493 and 1000x560\n";
    return 0;
}
