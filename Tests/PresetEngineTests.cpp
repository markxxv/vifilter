#include "PresetEngine.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
void check (bool valid, const char* reason)
{
    if (! valid)
    {
        std::cerr << "FAIL: " << reason << '\n';
        std::exit (EXIT_FAILURE);
    }
}

juce::ValueTree sampleState()
{
    juce::ValueTree state (vi::presets::parameterTreeName);
    for (size_t i = 0; i < vi::presets::specs.size(); ++i)
    {
        juce::ValueTree child ("PARAM");
        child.setProperty ("id", vi::presets::specs[i].id, nullptr);
        child.setProperty ("value", vi::presets::factory[3].values[i], nullptr);
        state.addChild (child, -1, nullptr);
    }
    return state;
}

float valueOf (const juce::ValueTree& state, const char* key)
{
    const auto child = state.getChildWithProperty ("id", key);
    check (child.isValid(), "Missing parameter after decode");
    return static_cast<float> (child.getProperty ("value"));
}

void testFactoryBank()
{
    check (vi::presets::factory.size() >= 6, "Expected useful factory bank");
    for (const auto& preset : vi::presets::factory)
    {
        check (juce::String (preset.name).isNotEmpty(), "Factory name must not be empty");
        for (size_t i = 0; i < vi::presets::specs.size(); ++i)
        {
            const auto& spec = vi::presets::specs[i];
            const auto v = preset.values[i];
            check (std::isfinite (v) && v >= spec.minimum && v <= spec.maximum,
                   "Factory value out of range");
            if (spec.discrete)
                check (std::round (v) == v, "Discrete factory value is not an integer");
        }
    }
}

void testVersionedRoundtrip()
{
    auto state = sampleState();
    auto xml = vi::presets::makeSessionXml (state, 4);
    check (xml != nullptr && xml->hasTagName ("ViFilterSession"),
           "Session must have versioned wrapper");

    // Parse serialized XML, not just the in-memory ValueTree.
    auto parsed = juce::XmlDocument::parse (xml->toString());
    check (parsed != nullptr, "Serialized session XML failed to parse");

    juce::ValueTree decoded;
    int program = -1;
    check (vi::presets::decodeXml (*parsed, decoded, program).wasOk(),
           "Versioned project state must roundtrip");
    check (program == 4, "Factory program selection must persist");
    check (decoded.getNumChildren() == static_cast<int> (vi::presets::specs.size()),
           "Full parameter count must persist");
    check (std::abs (valueOf (decoded, "drive") - 8.0f) < 1.0e-5f,
           "Drive must survive session serialization");
    check (valueOf (decoded, "analog_character") == 2.0f,
           "Choice values must survive session serialization");
}

void testLegacyAndMigration()
{
    juce::ValueTree legacy ("ViFilterParameters");
    juce::ValueTree gain ("PARAM");
    gain.setProperty ("id", "output_gain", nullptr);
    gain.setProperty ("value", -7.5, nullptr);
    legacy.addChild (gain, -1, nullptr);
    juce::ValueTree cutoff ("PARAM");
    cutoff.setProperty ("id", "cutoff", nullptr);
    cutoff.setProperty ("value", 3700.0, nullptr);
    legacy.addChild (cutoff, -1, nullptr);

    // v0.1 and v0.2 had fewer parameters, v0.3 was unwrapped too.
    auto xml = legacy.createXml();
    juce::ValueTree migrated;
    int program = 7;
    check (vi::presets::decodeXml (*xml, migrated, program).wasOk(),
           "Older projects must still load");
    check (program == 0, "Legacy projects have no selected factory program");
    check (valueOf (migrated, "output_gain") == -7.5f,
           "Legacy gain must remain unchanged");
    check (valueOf (migrated, "cutoff") == 3700.0f,
           "Legacy cutoff must remain unchanged");
    check (valueOf (migrated, "drive") == 0.0f,
           "Missing drive must default to silence-neutral value");
    check (valueOf (migrated, "oversampling") == 0.0f,
           "Missing oversampling must default to 1x");
}

void testInvalidFilesAndAtomicDecoding()
{
    auto state = sampleState();
    auto bad = vi::presets::makeSessionXml (state, 1);
    bad->setAttribute ("schema", 99);

    juce::ValueTree decoded = state.createCopy();
    int program = 8;
    check (vi::presets::decodeXml (*bad, decoded, program).failed(),
           "Future schema must be rejected");
    check (program == 8 && valueOf (decoded, "cutoff") == 10500.0f,
           "Failed decode must leave old state intact");

    auto broken = vi::presets::makeSessionXml (state, 0);
    auto* tree = broken->getChildByName ("ViFilterParameters");
    check (tree != nullptr, "Test data missing parameter tree");
    auto* param = tree->getChildByAttribute ("id", "cutoff");
    check (param != nullptr, "Test data missing cutoff");
    param->setAttribute ("value", "NaN");
    check (vi::presets::decodeXml (*broken, decoded, program).failed(),
           "Non-finite parameter must be rejected");

    for (const auto& invalid : { "1e", ".", "+", "2.3.4", "1e9999" })
    {
        param->setAttribute ("value", invalid);
        check (vi::presets::decodeXml (*broken, decoded, program).failed(),
               "Malformed numeric literals must be rejected");
    }
    check (valueOf (decoded, "cutoff") == 10500.0f,
           "Malformed file must not modify decoded output");

    auto clamp = vi::presets::makeSessionXml (state, 0);
    auto* value = clamp->getChildByName ("ViFilterParameters")
                       ->getChildByAttribute ("id", "resonance");
    value->setAttribute ("value", 23.0);
    check (vi::presets::decodeXml (*clamp, decoded, program).wasOk(),
           "Out-of-range finite parameters should migrate safely");
    check (valueOf (decoded, "resonance") == 1.0f,
           "Out-of-range parameter must clamp to legal maximum");
}

void testUserFileRoundtrip()
{
    const auto directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
        .getNonexistentChildFile ("vi-filter-presets-test", {}, false);
    check (directory.createDirectory().wasOk(), "Cannot create temporary directory");
    const auto state = sampleState();

    juce::File saved;
    check (vi::presets::saveFile (state, "My Warm Piano", directory, &saved).wasOk(),
           "User preset must save");
    check (saved.existsAsFile(), "Saved user file missing");
    check (vi::presets::saveFile (state, "My Warm Piano", directory).failed(),
           "Existing user preset must not be silently overwritten");
    check (vi::presets::saveFile (state, "../escape", directory).failed(),
           "Path traversal in preset name must be rejected");

    juce::ValueTree restored;
    juce::String name;
    check (vi::presets::loadFile (saved, restored, &name).wasOk(),
           "User preset must load");
    check (name == "My Warm Piano", "User preset name should roundtrip");
    check (valueOf (restored, "output_gain") == -2.0f,
           "User values must roundtrip");
    check (vi::presets::listFiles (directory).size() == 1,
           "User browser must enumerate saved presets");

    const auto corrupt = directory.getChildFile ("broken.vifilterpreset");
    check (corrupt.replaceWithText ("not XML"), "Corrupt fixture write failed");
    const auto before = restored.createCopy();
    check (vi::presets::loadFile (corrupt, restored).failed(),
           "Corrupt user preset must not load");
    check (restored.isEquivalentTo (before),
           "Corrupt preset must not modify existing state");
    check (directory.deleteRecursively(), "Temporary preset directory cleanup failed");
}
} // namespace

int main()
{
    testFactoryBank();
    testVersionedRoundtrip();
    testLegacyAndMigration();
    testInvalidFilesAndAtomicDecoding();
    testUserFileRoundtrip();
    std::cout << "Vi Filter preset tests passed\n";
}
