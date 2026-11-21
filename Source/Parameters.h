#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Parameter IDs. These are saved in sessions and presets, so never rename one
// once people are using the plugin; add new IDs instead.
namespace ParamIDs
{
inline constexpr auto inputGain     = "inputGain";

inline constexpr auto fuzzOn        = "fuzzOn";
inline constexpr auto fuzzPosition  = "fuzzPosition";
inline constexpr auto fuzz          = "fuzz";
inline constexpr auto fuzzVolume    = "fuzzVolume";
inline constexpr auto fuzzBias      = "fuzzBias";
inline constexpr auto fuzzCharacter = "fuzzCharacter";
inline constexpr auto fuzzMix       = "fuzzMix";

inline constexpr auto frequency     = "frequency";
inline constexpr auto q             = "q";
inline constexpr auto spaceL        = "spaceL";
inline constexpr auto spaceR        = "spaceR";
inline constexpr auto excite1       = "excite1";
inline constexpr auto excite2       = "excite2";
inline constexpr auto excite1Source = "excite1Source";
inline constexpr auto excite2Source = "excite2Source";
inline constexpr auto combDamping   = "combDamping";

inline constexpr auto levelLP       = "levelLP";
inline constexpr auto levelBP       = "levelBP";
inline constexpr auto levelHP       = "levelHP";
inline constexpr auto levelNP       = "levelNP";
inline constexpr auto levelComb     = "levelComb";
inline constexpr auto filterMix     = "filterMix";

inline constexpr auto bypass        = "bypass";
} // namespace ParamIDs

// LFO n (1-4) and envelope n (1-2) parameters: "lfo1Rate", "env2Attack", ...
namespace ModParamIds
{
inline juce::String lfo (int n, const char* suffix) { return "lfo" + juce::String (n) + suffix; }
inline juce::String env (int n, const char* suffix) { return "env" + juce::String (n) + suffix; }
// Mod grid amount: "mod_lfo1_freq" = LFO 1 into Frequency.
juce::String amount (int source, int destination);
} // namespace ModParamIds

namespace Choices
{
enum FuzzPosition { preFilter = 0, postFilter = 1 };
enum FuzzCharacter { germanium = 0, silicon = 1 };
enum ExciteSource { inputAudio = 0, sidechain = 1, gridOnly = 2 };
enum LfoSync { free = 0, tempo = 1 };
enum LfoRetrigger { retriggerOff = 0, retriggerEnv1 = 1, retriggerEnv2 = 2 };
enum FollowerSource { preInputGain = 0, postInputGain = 1, followerSidechain = 2, midiNotes = 3 };

// Tempo sync divisions and their length in quarter notes (4/4 assumed).
inline constexpr int numDivisions = 16;
inline constexpr const char* divisionNames[numDivisions] = {
    "8 bars", "4 bars", "2 bars", "1 bar", "1/2", "1/4 D", "1/4", "1/4 T",
    "1/8 D", "1/8", "1/8 T", "1/16 D", "1/16", "1/16 T", "1/32", "1/64"
};
inline constexpr double divisionBeats[numDivisions] = {
    32.0, 16.0, 8.0, 4.0, 2.0, 1.5, 1.0, 2.0 / 3.0,
    0.75, 0.5, 1.0 / 3.0, 0.375, 0.25, 1.0 / 6.0, 0.125, 0.0625
};
} // namespace Choices

inline constexpr float kLevelOffDb = -60.0f; // filter type level at or below this = off

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
