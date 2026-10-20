#include "PresetEngine.h"

#include <algorithm>
#include <cmath>

namespace vi::presets
{
namespace
{
constexpr const char* sessionTag = "ViFilterSession";
constexpr const char* presetTag = "ViFilterPreset";

std::unique_ptr<juce::XmlElement> wrap (const juce::ValueTree& state,
                                        const char* rootTag, int program,
                                        const juce::String& name)
{
    auto xml = std::make_unique<juce::XmlElement> (rootTag);
    xml->setAttribute ("schema", currentSchema);
    xml->setAttribute ("program", program);
    if (name.isNotEmpty())
        xml->setAttribute ("name", name);

    if (auto child = state.createXml())
        xml->addChildElement (child.release());
    return xml;
}
} // namespace

std::unique_ptr<juce::XmlElement> makeSessionXml (const juce::ValueTree& state,
                                                  int program)
{
    return wrap (state, sessionTag,
                 std::clamp (program, 0, static_cast<int> (factory.size())), {});
}

std::unique_ptr<juce::XmlElement> makePresetXml (const juce::ValueTree& state,
                                                 const juce::String& name)
{
    return wrap (state, presetTag, 0, name);
}

juce::Result decodeXml (const juce::XmlElement& xml,
                         juce::ValueTree& decodedState, int& decodedProgram)
{
    const auto legacy = xml.hasTagName (parameterTreeName);
    if (! legacy && ! xml.hasTagName (sessionTag) && ! xml.hasTagName (presetTag))
        return juce::Result::fail ("Unknown Vi Filter state document");

    const juce::XmlElement* parameterXml = &xml;
    int program = 0;

    if (! legacy)
    {
        if (! xml.hasAttribute ("schema")
            || xml.getIntAttribute ("schema", -1) != currentSchema)
            return juce::Result::fail ("Unsupported preset schema");

        parameterXml = xml.getChildByName (parameterTreeName);
        if (parameterXml == nullptr)
            return juce::Result::fail ("Parameter data missing");

        // User files are always loaded as Custom, even if they originated in a factory bank.
        if (xml.hasTagName (sessionTag))
            program = std::clamp (xml.getIntAttribute ("program", 0),
                                  0, static_cast<int> (factory.size()));
    }

    const auto source = juce::ValueTree::fromXml (*parameterXml);
    if (! source.isValid() || ! source.hasType (parameterTreeName))
        return juce::Result::fail ("Invalid parameter tree");

    juce::ValueTree state (parameterTreeName);
    for (const auto& spec : specs)
    {
        double value = spec.defaultValue;
        bool seen = false;

        for (const auto& child : source)
        {
            if (! child.hasType ("PARAM")
                || child.getProperty ("id").toString() != spec.id)
                continue;

            if (seen)
                return juce::Result::fail ("Duplicate parameter: " + juce::String (spec.id));
            seen = true;

            const auto raw = child.getProperty ("value").toString().trim();
            if (raw.isEmpty() || ! raw.containsOnly ("+-0123456789.eE"))
                return juce::Result::fail ("Invalid value for: " + juce::String (spec.id));

            value = raw.getDoubleValue();
            if (! std::isfinite (value))
                return juce::Result::fail ("Non-finite value for: " + juce::String (spec.id));
        }

        value = std::clamp (value, static_cast<double> (spec.minimum),
                           static_cast<double> (spec.maximum));
        if (spec.discrete)
            value = std::round (value);

        juce::ValueTree parameter ("PARAM");
        parameter.setProperty ("id", spec.id, nullptr);
        parameter.setProperty ("value", value, nullptr);
        state.addChild (parameter, -1, nullptr);
    }

    decodedState = state;
    decodedProgram = program;
    return juce::Result::ok();
}

juce::File userDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Vi Audio")
        .getChildFile ("Vi Filter")
        .getChildFile ("Presets");
}

juce::Result saveFile (const juce::ValueTree& state, const juce::String& name,
                       const juce::File& directory, juce::File* savedFile)
{
    const auto trimmed = name.trim();
    if (trimmed.isEmpty() || trimmed.length() > 64
        || trimmed.containsAnyOf ("/\\:") || trimmed == "." || trimmed == "..")
        return juce::Result::fail ("Invalid preset name");

    const auto safeName = juce::File::createLegalFileName (trimmed);
    if (safeName.isEmpty())
        return juce::Result::fail ("Invalid preset filename");

    auto xml = makePresetXml (state, trimmed);
    juce::ValueTree checked;
    int program = 0;
    const auto valid = decodeXml (*xml, checked, program);
    if (valid.failed())
        return valid;

    const auto created = directory.createDirectory();
    if (created.failed())
        return created;

    const auto target = directory.getChildFile (safeName + presetExtension);
    if (target.exists())
        return juce::Result::fail ("A preset with this name already exists");

    juce::TemporaryFile temporary (target);
    if (! temporary.getFile().replaceWithText (xml->toString()))
        return juce::Result::fail ("Could not write temporary preset file");
    if (! temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail ("Could not commit preset file");

    if (savedFile != nullptr)
        *savedFile = target;
    return juce::Result::ok();
}

juce::Result loadFile (const juce::File& file, juce::ValueTree& decodedState,
                       juce::String* presetName)
{
    if (! file.existsAsFile() || ! file.hasFileExtension (presetExtension))
        return juce::Result::fail ("Not a Vi Filter preset file");
    if (file.getSize() > 1024 * 1024)
        return juce::Result::fail ("Preset exceeds 1 MiB");

    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (presetTag))
        return juce::Result::fail ("Invalid user preset document");

    juce::ValueTree decoded;
    int program = 0;
    const auto result = decodeXml (*xml, decoded, program);
    if (result.failed())
        return result;

    decodedState = decoded;
    if (presetName != nullptr)
        *presetName = xml->getStringAttribute ("name", file.getFileNameWithoutExtension());
    return juce::Result::ok();
}

juce::Array<juce::File> listFiles (const juce::File& directory)
{
    juce::Array<juce::File> result;
    if (directory.isDirectory())
        directory.findChildFiles (result, juce::File::findFiles, false,
                                  juce::String ("*") + presetExtension);
    return result;
}

} // namespace vi::presets
