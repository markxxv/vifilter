#pragma once

#include <juce_graphics/juce_graphics.h>

namespace vi::typography
{
enum class Weight
{
    light,
    regular,
    medium
};

// All weights are compiled into the AU/Standalone via JUCE BinaryData.
// No installed system fonts or runtime file paths are required.
juce::Typeface::Ptr typeface (Weight weight);
juce::Font font (float height, Weight weight = Weight::regular);
bool bundledFacesAvailable() noexcept;
} // namespace vi::typography
