#include "Parameters.h"
#include "ModDefs.h"

juce::String ModParamIds::amount (int source, int destination)
{
    return juce::String ("mod_") + Mod::sourceIds[(size_t) source] + "_" + Mod::destinationIds[(size_t) destination];
}

namespace
{
using namespace juce;

std::unique_ptr<AudioParameterFloat> floatParam (const String& id, const String& name, NormalisableRange<float> range,
                                                 float defaultValue, const char* label = "")
{
    return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, defaultValue,
                                                  AudioParameterFloatAttributes().withLabel (label));
}

std::unique_ptr<AudioParameterFloat> percentParam (const String& id, const String& name, float defaultValue,
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

// Times from `lo` to `hi` seconds on a log scale.
NormalisableRange<float> logRange (float lo, float hi)
{
    return NormalisableRange<float> (lo, hi,
        [] (float a, float b, float x) { return a * std::pow (b / a, x); },
        [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); });
}

String secondsText (float s)
{
    return s < 1.0f ? String (s * 1000.0f, s < 0.01f ? 2 : 0) + " ms" : String (s, 2) + " s";
}

std::unique_ptr<AudioParameterFloat> timeParam (const String& id, const String& name, float lo, float hi, float def)
{
    return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, logRange (lo, hi), def,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (
                                                      [] (float v, int) { return secondsText (v); }));
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
    layout.add (percentParam (ParamIDs::q, "Rez", 0.3f, -1.0f));
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

    // LFOs
    StringArray waves { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Sample and Hold", "Smooth Random" };
    StringArray divisions;
    for (auto* d : Choices::divisionNames)
        divisions.add (d);
    for (int n = 1; n <= 4; ++n)
    {
        const String name = "LFO " + String (n) + " ";
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ModParamIds::lfo (n, "Wave"), 1 }, name + "Wave", waves, 0));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ModParamIds::lfo (n, "Rate"), 1 }, name + "Rate", logRange (0.01f, 2000.0f), 1.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                { return v < 1.0f ? String (v, 3) + " Hz" : v < 100.0f ? String (v, 2) + " Hz" : String (v, 0) + " Hz"; })));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ModParamIds::lfo (n, "Sync"), 1 }, name + "Sync",
                                                            StringArray { "Free", "Tempo" }, 0));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ModParamIds::lfo (n, "Division"), 1 }, name + "Division", divisions, 6));
        layout.add (floatParam (ModParamIds::lfo (n, "Phase"), name + "Phase", { 0.0f, 360.0f, 1.0f }, 0.0f, "deg"));
        layout.add (floatParam (ModParamIds::lfo (n, "Stereo"), name + "Stereo", { 0.0f, 180.0f, 1.0f }, 0.0f, "deg"));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ModParamIds::lfo (n, "Retrigger"), 1 }, name + "Retrigger",
                                                            StringArray { "Off", "Env 1", "Env 2" }, 0));
    }

    // Envelopes, each triggered by its own envelope follower
    for (int n = 1; n <= 2; ++n)
    {
        const String name = "Env " + String (n) + " ";
        layout.add (floatParam (ModParamIds::env (n, "Threshold"), name + "Threshold", { -60.0f, 0.0f, 0.1f }, -30.0f, "dB"));
        layout.add (percentParam (ModParamIds::env (n, "Sensitivity"), name + "Sensitivity", 0.5f));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ModParamIds::env (n, "Source"), 1 }, name + "Source",
                                                            StringArray { "Pre input gain", "Post input gain", "Sidechain" }, 1));
        layout.add (timeParam (ModParamIds::env (n, "Attack"), name + "Attack", 0.0001f, 10.0f, 0.005f));
        layout.add (timeParam (ModParamIds::env (n, "Decay"), name + "Decay", 0.001f, 10.0f, 0.2f));
        layout.add (percentParam (ModParamIds::env (n, "Sustain"), name + "Sustain", 0.7f));
        layout.add (timeParam (ModParamIds::env (n, "Release"), name + "Release", 0.001f, 20.0f, 0.3f));
    }

    // Mod grid: what the three Assign columns point at...
    StringArray assignChoices { "None" };
    for (int s = Mod::numFixedDestinations; s < Mod::numSlots; ++s)
        assignChoices.add (Mod::slotNames[(size_t) s]);
    for (int a = 0; a < Mod::numAssigns; ++a)
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { Mod::assignTargetIds[(size_t) a], 1 },
                                                            String (Mod::assignNames[(size_t) a]) + " Target", assignChoices, 0));

    // ...and one amount per crossing, -100% to +100%.
    for (int src = 0; src < Mod::numSources; ++src)
        for (int dst = 0; dst < Mod::numDestinations; ++dst)
        {
            const String dstName = dst < Mod::numFixedDestinations ? String (Mod::slotNames[(size_t) dst])
                                                                   : String (Mod::assignNames[(size_t) (dst - Mod::numFixedDestinations)]);
            layout.add (percentParam (ModParamIds::amount (src, dst),
                                      String (Mod::sourceNames[(size_t) src]) + " > " + dstName, 0.0f, -1.0f));
        }

    return layout;
}
