#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "ModDefs.h"
#include "Parameters.h"
#include "PresetManager.h"
#include "dsp/FuzzFace.h"
#include "dsp/InputStage.h"
#include "dsp/Modulation.h"
#include "dsp/QuadPeakFilter.h"

class FuzzyFilterProcessor : public juce::AudioProcessor
{
public:
    FuzzyFilterProcessor();
    ~FuzzyFilterProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState apvts;

    // Separate outputs, in bus order after the main output.
    static constexpr int kNumTypeOutputs = 5; // LP, BP, HP, NP, Comb

    // Latest value of each mod source (left channel), for tests and the UI.
    float getSourceValue (int source) const { return sourceValues[(size_t) source].load(); }

    // For the UI: a slot's live normalised value (left channel) at the end of
    // the last block, or -1 when nothing modulates it.
    float getSlotLive (int slot) const { return slotLive[(size_t) slot].load(); }
    // Peak input level since the last call, per channel (after the input gain).
    float takeInputPeak (int ch) { return inputPeak[ch & 1].exchange (0.0f); }
    double getLfoPhase (int lfo) const { return lfoPhase[lfo & 3].load(); }

    PresetManager presets { apvts };

private:
    static BusesProperties makeBuses();

    // A parameter the mod grid can move. Its knob position is smoothed in
    // normalised (0..1) units; modulation adds to that, and the result is
    // converted back to the parameter's own units.
    struct ModSlot
    {
        juce::RangedAudioParameter* param = nullptr;
        juce::SmoothedValue<float> base;
        bool dynamic = false;   // modulated or moving this block: compute per sample
        float staticPlain = 0;  // otherwise this value is used for the whole block

        float plain (float normalised) const { return param->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, normalised)); }
    };

    struct Routing
    {
        int source, slot;
        float amount;
    };

    void prepareBuffers (int maxBlock);
    void updateRoutings();
    void runModulation (int numSamples);
    void runInputStage (int numSamples);
    void runFuzz (juce::AudioBuffer<float>& buffer, int numSamples);
    void runFilter (int numSamples);

    // Value of a slot for channel `ch` at sample `i`, in the parameter's units.
    float slotValue (int slot, int ch, int i) const
    {
        const auto& s = slots[(size_t) slot];
        return s.dynamic ? s.plain (modValues.getSample (slot * 2 + ch, i)) : s.staticPlain;
    }

    // Plain (unmodulated) parameters.
    std::atomic<float>* pFuzzOn = nullptr;
    std::atomic<float>* pFuzzPosition = nullptr;
    std::atomic<float>* pFuzzCharacter = nullptr;
    std::atomic<float>* pExcite1Source = nullptr;
    std::atomic<float>* pExcite2Source = nullptr;
    std::atomic<float>* pOutputMode = nullptr;
    juce::AudioParameterBool* bypassParam = nullptr;

    std::atomic<float>* pLfoWave[4] {};
    std::atomic<float>* pLfoSync[4] {};
    std::atomic<float>* pLfoDivision[4] {};
    std::atomic<float>* pLfoPhase[4] {};
    std::atomic<float>* pLfoStereo[4] {};
    std::atomic<float>* pLfoRetrigger[4] {};
    std::atomic<float>* pEnvSensitivity[2] {};
    std::atomic<float>* pEnvSource[2] {};
    std::atomic<float>* pAssignTarget[Mod::numAssigns] {};
    std::atomic<float>* pAmounts[Mod::numSources][Mod::numDestinations] {};

    std::array<ModSlot, Mod::numSlots> slots;
    std::vector<Routing> routings; // active grid crossings this block (reserved, never reallocates)
    bool spaceRHasOwnRouting = false;

    // DSP
    ff::FuzzFace fuzz[2];
    ff::QuadPeakFilter filter;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    ff::Lfo lfos[4];
    ff::EnvelopeFollower followers[2];
    ff::FollowerGate gates[2];
    ff::Adsr envelopes[2];
    float prevSourceL[Mod::numSources] {}, prevSourceR[Mod::numSources] {};
    std::array<std::atomic<float>, Mod::numSources> sourceValues {};
    std::array<std::atomic<float>, Mod::numSlots> slotLive {};
    std::atomic<float> inputPeak[2] {};
    std::atomic<double> lfoPhase[4] {};

    double preparedSampleRate = 44100.0;

    // Work buffers, sized in prepareToPlay so the audio thread never allocates.
    juce::AudioBuffer<float> work, sidechain, typeOut, dryForExcite;
    juce::AudioBuffer<float> modValues;   // [slot * 2 + channel][sample], normalised
    juce::AudioBuffer<float> gridExcite;  // grid signal into Excite 1/2 for "Mod grid only": [exc * 2 + ch]
    juce::AudioBuffer<float> fuzzParams;  // [ch * 4 + {fuzz, bias, volume, mix}][sample]

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FuzzyFilterProcessor)
};
