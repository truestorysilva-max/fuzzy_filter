#pragma once

// Everything that defines the mod grid: its sources, its destinations, and
// the "slots" (modulatable parameters) that destinations and the Assign
// columns point at.

#include <array>

namespace Mod
{
// ---- Sources (grid rows) ----
enum Source { lfo1, lfo2, lfo3, lfo4, env1, env2, follower1, follower2, numSources };

inline constexpr std::array<const char*, numSources> sourceIds { "lfo1", "lfo2", "lfo3", "lfo4", "env1", "env2", "fol1", "fol2" };
inline constexpr std::array<const char*, numSources> sourceNames { "LFO 1", "LFO 2", "LFO 3", "LFO 4", "Env 1", "Env 2", "Follower 1", "Follower 2" };

// ---- Slots: every parameter the grid can move ----
// The first 11 are the grid's fixed destination columns; the rest can be
// reached through the Assign 1 to 3 columns.
enum Slot
{
    inputGain, frequency, q, spaceL, spaceR, excite1, excite2, fuzz, fuzzBias, fuzzVolume, fuzzMix,
    lfo1Rate, lfo2Rate, lfo3Rate, lfo4Rate,
    env1Attack, env1Decay, env1Sustain, env1Release,
    env2Attack, env2Decay, env2Sustain, env2Release,
    env1Threshold, env2Threshold,
    combDamping, filterMix,
    levelLP, levelBP, levelHP, levelNP, levelComb,
    numSlots
};

inline constexpr int numFixedDestinations = 11;
inline constexpr int numAssigns = 3;
inline constexpr int numDestinations = numFixedDestinations + numAssigns; // 14 grid columns

// Parameter ID behind each slot (must match Parameters.h).
inline constexpr std::array<const char*, numSlots> slotParamIds {
    "inputGain", "frequency", "q", "spaceL", "spaceR", "excite1", "excite2", "fuzz", "fuzzBias", "fuzzVolume", "fuzzMix",
    "lfo1Rate", "lfo2Rate", "lfo3Rate", "lfo4Rate",
    "env1Attack", "env1Decay", "env1Sustain", "env1Release",
    "env2Attack", "env2Decay", "env2Sustain", "env2Release",
    "env1Threshold", "env2Threshold",
    "combDamping", "filterMix",
    "levelLP", "levelBP", "levelHP", "levelNP", "levelComb"
};

inline constexpr std::array<const char*, numSlots> slotNames {
    "Input Level", "Frequency", "Rez", "Space L", "Space R", "Excite 1", "Excite 2", "Fuzz", "Bias", "Volume", "Fuzz Mix",
    "LFO 1 Rate", "LFO 2 Rate", "LFO 3 Rate", "LFO 4 Rate",
    "Env 1 Attack", "Env 1 Decay", "Env 1 Sustain", "Env 1 Release",
    "Env 2 Attack", "Env 2 Decay", "Env 2 Sustain", "Env 2 Release",
    "Env 1 Threshold", "Env 2 Threshold",
    "Comb Damping", "Filter Mix",
    "Low Pass Level", "Band Pass Level", "High Pass Level", "Notch Pass Level", "Comb Level"
};

// ---- Destinations (grid columns) ----
inline constexpr std::array<const char*, numDestinations> destinationIds {
    "in", "freq", "q", "spl", "spr", "ex1", "ex2", "fuzz", "bias", "vol", "fmix", "a1", "a2", "a3"
};
inline constexpr std::array<const char*, numAssigns> assignNames { "Assign 1", "Assign 2", "Assign 3" };
inline constexpr std::array<const char*, numAssigns> assignTargetIds { "assign1Target", "assign2Target", "assign3Target" };

// Assign target choice index 0 is "None"; index i (i >= 1) is slot
// numFixedDestinations + i - 1.
inline constexpr int numAssignChoices = 1 + (numSlots - numFixedDestinations);
inline constexpr int assignChoiceToSlot (int choice) { return choice <= 0 ? -1 : numFixedDestinations + choice - 1; }
} // namespace Mod
