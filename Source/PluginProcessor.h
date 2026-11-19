#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Parameters.h"
#include "dsp/FuzzFace.h"
#include "dsp/InputStage.h"
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

private:
    static BusesProperties makeBuses();
    void runFuzz (juce::AudioBuffer<float>& buffer, int numSamples);

    // Cached parameter pointers (read on the audio thread).
    std::atomic<float>* pInputGain = nullptr;
    std::atomic<float>* pFuzzOn = nullptr;
    std::atomic<float>* pFuzzPosition = nullptr;
    std::atomic<float>* pFuzz = nullptr;
    std::atomic<float>* pFuzzVolume = nullptr;
    std::atomic<float>* pFuzzBias = nullptr;
    std::atomic<float>* pFuzzCharacter = nullptr;
    std::atomic<float>* pFuzzMix = nullptr;
    std::atomic<float>* pFrequency = nullptr;
    std::atomic<float>* pQ = nullptr;
    std::atomic<float>* pSpaceL = nullptr;
    std::atomic<float>* pSpaceR = nullptr;
    std::atomic<float>* pExcite1 = nullptr;
    std::atomic<float>* pExcite2 = nullptr;
    std::atomic<float>* pExcite1Source = nullptr;
    std::atomic<float>* pExcite2Source = nullptr;
    std::atomic<float>* pCombDamping = nullptr;
    std::atomic<float>* pLevels[kNumTypeOutputs] {};
    std::atomic<float>* pFilterMix = nullptr;
    std::atomic<float>* pOutputMode = nullptr;
    juce::AudioParameterBool* bypassParam = nullptr;

    // DSP
    ff::FuzzFace fuzz[2];
    ff::QuadPeakFilter filter;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    // Smoothed controls
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smFrequency;
    juce::SmoothedValue<float> smInputGain, smQ, smSpaceL, smSpaceR, smExcite1, smExcite2,
        smCombDamping, smFilterMix;
    juce::SmoothedValue<float> smLevels[kNumTypeOutputs];

    double preparedSampleRate = 44100.0;

    // Work buffers, sized in prepareToPlay so the audio thread never allocates.
    juce::AudioBuffer<float> work, sidechain, typeOut, dryForExcite;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FuzzyFilterProcessor)
};
