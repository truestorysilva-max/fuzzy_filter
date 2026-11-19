#include "Parameters.h"

namespace
{
using namespace juce;

std::unique_ptr<AudioParameterFloat> floatParam (const char* id, const char* name, NormalisableRange<float> range,
                                                 float defaultValue, const char* label = "")
{
    return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, defaultValue,
                                                  AudioParameterFloatAttributes().withLabel (label));
}

std::unique_ptr<AudioParameterFloat> percentParam (const char* id, const char* name, float defaultValue,
                                                   float minValue = 0.0f)
{
    auto toText = [] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + "%"; };
    auto fromText = [] (const String& t) { return t.getFloatValue() / 100.0f; };
    return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                  NormalisableRange<float> (minValue, 1.0f), defaultValue,
                                                  AudioParameterFloatAttributes()
                                                      .withStringFromValueFunction (toText)
                                                      .withValueFromStringFunction (fromText));
}

std::unique_ptr<AudioParameterFloat> levelParam (const char* id, const char* name, float defaultDb)
{
    auto toText = [] (float v, int) { return v <= kLevelOffDb ? String ("Off") : String (v, 1) + " dB"; };
    return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                  NormalisableRange<float> (kLevelOffDb, 6.0f, 0.1f, 2.5f),
                                                  defaultDb,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (toText));
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    // Input
    layout.add (floatParam (ParamIDs::inputGain, "Input Gain", { -24.0f, 24.0f, 0.1f }, 0.0f, "dB"));

    // Fuzz
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::fuzzOn, 1 }, "Fuzz On", true));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::fuzzPosition, 1 }, "Fuzz Position",
                                                        StringArray { "Pre filter", "Post filter" }, 0));
    layout.add (percentParam (ParamIDs::fuzz, "Fuzz", 0.7f));
    layout.add (floatParam (ParamIDs::fuzzVolume, "Fuzz Volume", { -36.0f, 6.0f, 0.1f }, 0.0f, "dB"));
    layout.add (percentParam (ParamIDs::fuzzBias, "Fuzz Bias", 0.5f));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::fuzzCharacter, 1 }, "Fuzz Character",
                                                        StringArray { "Germanium", "Silicon" }, 0));
    layout.add (percentParam (ParamIDs::fuzzMix, "Fuzz Mix", 1.0f));

    // Filter
    layout.add (floatParam (ParamIDs::frequency, "Frequency", { 20.0f, 20000.0f, 0.01f, 0.25f }, 1000.0f, "Hz"));
    layout.add (percentParam (ParamIDs::q, "Q", 0.3f, -1.0f));
    layout.add (percentParam (ParamIDs::spaceL, "Space L", 0.0f));
    layout.add (percentParam (ParamIDs::spaceR, "Space R", 0.0f));
    layout.add (percentParam (ParamIDs::excite1, "Excite 1", 0.0f));
    layout.add (percentParam (ParamIDs::excite2, "Excite 2", 0.0f));
    const StringArray sources { "Input audio", "Sidechain", "Mod grid only" };
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::excite1Source, 1 }, "Excite 1 Source", sources, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::excite2Source, 1 }, "Excite 2 Source", sources, 0));
    layout.add (percentParam (ParamIDs::combDamping, "Comb Damping", 0.3f));

    // Output mixer
    layout.add (levelParam (ParamIDs::levelLP, "Low Pass Level", 0.0f));
    layout.add (levelParam (ParamIDs::levelBP, "Band Pass Level", kLevelOffDb));
    layout.add (levelParam (ParamIDs::levelHP, "High Pass Level", kLevelOffDb));
    layout.add (levelParam (ParamIDs::levelNP, "Notch Pass Level", kLevelOffDb));
    layout.add (levelParam (ParamIDs::levelComb, "Comb Level", kLevelOffDb));
    layout.add (percentParam (ParamIDs::filterMix, "Filter Mix", 1.0f));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::outputMode, 1 }, "Output Mode",
                                                        StringArray { "Mixed", "Separate" }, 0));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::bypass, 1 }, "Bypass", false));

    return layout;
}
