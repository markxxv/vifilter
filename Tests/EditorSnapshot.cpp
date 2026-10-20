#include "PluginEditor.h"

#include <juce_gui_extra/juce_gui_extra.h>
#include <iostream>
#include <memory>

int main()
{
    juce::ScopedJuceInitialiser_GUI initialise;
    ViFilterProcessor processor;
    processor.setRateAndBufferSizeDetails (48000.0, 512);
    processor.prepareToPlay (48000.0, 512);

    auto editor = std::unique_ptr<juce::AudioProcessorEditor> (processor.createEditor());
    if (editor == nullptr || editor->getWidth() < 1000 || editor->getHeight() < 600)
    {
        std::cerr << "Missing or undersized Vi Filter editor\n";
        return 1;
    }

    // Exercise both the intended desktop size and the minimum supported size.
    for (const auto dimensions : { std::pair<int, int> { 1100, 650 },
                                   std::pair<int, int> { 1440, 820 } })
    {
        editor->setSize (dimensions.first, dimensions.second);
        juce::Image image (juce::Image::ARGB, dimensions.first, dimensions.second, true);
        juce::Graphics graphics (image);
        editor->paintEntireComponent (graphics, true);

        const auto target = juce::File::getCurrentWorkingDirectory().getChildFile (
            dimensions.first == 1440 ? "ViFilter_UI_Preview.png" : "ViFilter_UI_Compact.png");
        juce::PNGImageFormat png;
        auto stream = target.createOutputStream();
        if (stream == nullptr || ! png.writeImageToStream (image, *stream))
        {
            std::cerr << "Could not render offscreen JUCE editor\n";
            return 1;
        }
        stream->flush();
    }
    std::cout << "Editor rendered at 1100x650 and 1440x820\n";
    return 0;
}
