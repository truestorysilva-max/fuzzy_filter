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
inline constexpr auto outputMode    = "outputMode";

inline constexpr auto bypass        = "bypass";
} // namespace ParamIDs

namespace Choices
{
enum FuzzPosition { preFilter = 0, postFilter = 1 };
enum FuzzCharacter { germanium = 0, silicon = 1 };
enum ExciteSource { inputAudio = 0, sidechain = 1, gridOnly = 2 };
enum OutputMode { mixed = 0, separate = 1 };
} // namespace Choices

inline constexpr float kLevelOffDb = -60.0f; // filter type level at or below this = off

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
