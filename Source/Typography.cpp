#include "Typography.h"
#include <BinaryData.h>

namespace vi::typography
{
namespace
{
juce::Typeface::Ptr loadFace (const char* resource)
{
    int bytes = 0;
    const auto* data = BinaryData::getNamedResource (resource, bytes);
    if (data == nullptr || bytes <= 0)
        return nullptr;
    return juce::Typeface::createSystemTypefaceFor (data, static_cast<size_t> (bytes));
}

const juce::Typeface::Ptr& faceFor (Weight weight)
{
    // Reuse typeface instances. Creating a Typeface for every paint() is costly
    // and can provoke OS font registration/allocations.
    static const auto light = loadFace ("DINish_Light_otf");
    static const auto regular = loadFace ("DINish_Regular_otf");
    static const auto medium = loadFace ("DINish_Medium_otf");

    switch (weight)
    {
        case Weight::light:   return light;
        case Weight::medium:  return medium;
        case Weight::regular: return regular;
    }
    return regular;
}
} // namespace

juce::Typeface::Ptr typeface (Weight weight)
{
    return faceFor (weight);
}

juce::Font font (float height, Weight weight)
{
    if (auto face = typeface (weight))
        return juce::Font (juce::FontOptions (face).withHeight (height));
    return juce::Font (juce::FontOptions (height));
}

bool bundledFacesAvailable() noexcept
{
    return faceFor (Weight::light) != nullptr
        && faceFor (Weight::regular) != nullptr
        && faceFor (Weight::medium) != nullptr;
}
} // namespace vi::typography
