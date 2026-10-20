#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <memory>

namespace vi::presets
{
constexpr int currentSchema = 1;
constexpr const char* parameterTreeName = "ViFilterParameters";
constexpr const char* presetExtension = ".vifilterpreset";

// Unnormalised values. IDs and defaults are stable across releases.
struct ParameterSpec
{
    const char* id;
    float defaultValue;
    float minimum;
    float maximum;
    bool discrete;
};

inline constexpr std::array<ParameterSpec, 9> specs {{
    { "filter_mode",      0.0f,    0.0f,     2.0f, true  },
    { "cutoff",        1000.0f,   20.0f, 20000.0f, false },
    { "resonance",        0.0f,    0.0f,     1.0f, false },
    { "drive",            0.0f,    0.0f,    24.0f, false },
    { "analog_character", 0.0f,    0.0f,     2.0f, true  },
    { "oversampling",     0.0f,    0.0f,     2.0f, true  },
    { "output_gain",      0.0f,  -24.0f,    24.0f, false },
    { "input_trim",       0.0f,  -24.0f,    24.0f, false },
    { "mix",            100.0f,    0.0f,   100.0f, false }
}};

struct FactoryPreset
{
    const char* name;
    std::array<float, specs.size()> values;
};

// Bank program zero is 'Custom'; these fixed-index factory programs start at one.
inline constexpr std::array<FactoryPreset, 8> factory {{
    { "Clean Start",     {{ 0, 16000, 0.00f, 0.0f, 0, 0,  0.0f, 0.0f, 100.0f }} },
    { "Soft Piano",      {{ 0,  5900, 0.08f, 2.5f, 0, 1, -0.5f, 0.0f, 100.0f }} },
    { "Velvet Lowpass",  {{ 0,  2100, 0.20f, 4.0f, 1, 2, -1.5f, 0.0f, 100.0f }} },
    { "Warm Tape",       {{ 0, 10500, 0.07f, 8.0f, 2, 2, -2.0f, 0.0f, 100.0f }} },
    { "Gentle Tube",     {{ 0,  7200, 0.12f, 6.0f, 1, 2, -1.5f, 0.0f, 100.0f }} },
    { "Focused Mid",     {{ 2,  1700, 0.25f, 3.0f, 0, 1, -1.0f, 0.0f, 100.0f }} },
    { "Low Cut Air",     {{ 1,   260, 0.06f, 1.5f, 0, 1,  0.0f, 0.0f, 100.0f }} },
    { "Vintage Radio",   {{ 2,  1400, 0.48f, 9.0f, 2, 2, -3.0f, 0.0f, 100.0f }} }
}};

std::unique_ptr<juce::XmlElement> makeSessionXml (const juce::ValueTree& state,
                                                  int program);
std::unique_ptr<juce::XmlElement> makePresetXml (const juce::ValueTree& state,
                                                 const juce::String& name);

// Accepts v0.1–v0.3 bare ViFilterParameters XML and v0.4 wrapped XML.
// Fully validates before changing either output argument.
juce::Result decodeXml (const juce::XmlElement& xml,
                         juce::ValueTree& decodedState, int& decodedProgram);

juce::File userDirectory();
juce::Result saveFile (const juce::ValueTree& state, const juce::String& name,
                       const juce::File& directory, juce::File* savedFile = nullptr);
juce::Result loadFile (const juce::File& file, juce::ValueTree& decodedState,
                       juce::String* presetName = nullptr);
juce::Array<juce::File> listFiles (const juce::File& directory);

} // namespace vi::presets
