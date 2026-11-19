// Runs the real plugin processor with no host: checks the bus layouts,
// separate outputs, sidechain, bypass and saving/loading state.

#include "PluginProcessor.h"

#include <cstdio>
#include <random>

namespace
{
int failures = 0;

void check (bool ok, const char* what, double value = 0.0)
{
    std::printf ("[%s] %s (%.4g)\n", ok ? "PASS" : "FAIL", what, value);
    if (! ok) ++failures;
}

void setParam (FuzzyFilterProcessor& p, const juce::String& id, float plainValue)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (plainValue));
}

// Fills main input with noise and the sidechain with a click every 250 ms,
// runs `blocks` blocks, and returns the RMS of every output bus.
std::vector<double> run (FuzzyFilterProcessor& p, int blocks, bool& finite)
{
    constexpr int blockSize = 512;
    const int numChannels = juce::jmax (p.getTotalNumInputChannels(), p.getTotalNumOutputChannels());
    juce::AudioBuffer<float> buffer (numChannels, blockSize);
    juce::MidiBuffer midi;
    std::mt19937 rng (7);
    std::uniform_real_distribution<float> noise (-0.3f, 0.3f);

    std::vector<double> sums ((size_t) p.getBusCount (false), 0.0);
    finite = true;
    int sample = 0;
    for (int b = 0; b < blocks; ++b)
    {
        buffer.clear();
        auto mainIn = p.getBusBuffer (buffer, true, 0);
        for (int ch = 0; ch < mainIn.getNumChannels(); ++ch)
            for (int i = 0; i < blockSize; ++i)
                mainIn.setSample (ch, i, noise (rng));

        if (p.getBus (true, 1)->isEnabled())
        {
            auto sc = p.getBusBuffer (buffer, true, 1);
            for (int i = 0; i < blockSize; ++i)
                for (int ch = 0; ch < sc.getNumChannels(); ++ch)
                    sc.setSample (ch, i, ((sample + i) % 12000) < 48 ? 0.9f : 0.0f);
        }
        sample += blockSize;

        p.processBlock (buffer, midi);

        for (int bus = 0; bus < p.getBusCount (false); ++bus)
        {
            if (! p.getBus (false, bus)->isEnabled()) continue;
            auto out = p.getBusBuffer (buffer, false, bus);
            for (int ch = 0; ch < out.getNumChannels(); ++ch)
                for (int i = 0; i < blockSize; ++i)
                {
                    const float v = out.getSample (ch, i);
                    finite = finite && std::isfinite (v);
                    sums[(size_t) bus] += (double) v * v;
                }
        }
    }
    for (auto& s : sums)
        s = std::sqrt (s / (double) (blocks * blockSize * 2));
    return sums;
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;

    // Default layout: stereo in, sidechain on, separate outputs off.
    {
        FuzzyFilterProcessor p;
        p.prepareToPlay (48000.0, 512);
        bool finite = false;
        const auto rms = run (p, 200, finite);
        check (finite, "default layout: output is finite");
        check (rms[0] > 0.01, "default layout: main output carries audio", rms[0]);
        check (p.getLatencySamples() >= 0, "latency reported", p.getLatencySamples());
    }

    // Every bus on, Separate mode, every filter type up, Excite on the sidechain.
    {
        FuzzyFilterProcessor p;
        auto layout = p.getBusesLayout();
        for (int i = 1; i < layout.outputBuses.size(); ++i)
            layout.outputBuses.getReference (i) = juce::AudioChannelSet::stereo();
        check (p.setBusesLayout (layout), "all separate outputs can be turned on");

        setParam (p, ParamIDs::outputMode, (float) Choices::separate);
        for (auto* id : { ParamIDs::levelBP, ParamIDs::levelHP, ParamIDs::levelNP, ParamIDs::levelComb })
            setParam (p, id, 0.0f);
        setParam (p, ParamIDs::excite1Source, (float) Choices::sidechain);
        setParam (p, ParamIDs::excite1, 1.0f);
        setParam (p, ParamIDs::q, 0.9f);
        setParam (p, ParamIDs::spaceL, 0.7f);

        p.prepareToPlay (48000.0, 512);
        bool finite = false;
        const auto rms = run (p, 200, finite);
        check (finite, "separate mode: every output is finite");
        bool allCarry = true;
        for (size_t bus = 1; bus < rms.size(); ++bus)
            allCarry = allCarry && rms[bus] > 0.001;
        check (allCarry, "separate mode: all 5 filter type outputs carry audio", rms.back());

        setParam (p, ParamIDs::outputMode, (float) Choices::mixed);
        const auto mixedRms = run (p, 50, finite);
        check (mixedRms[1] == 0.0, "mixed mode: separate outputs are silent", mixedRms[1]);
    }

    // Mono in, no sidechain, fuzz after the filter at full gain.
    {
        FuzzyFilterProcessor p;
        auto layout = p.getBusesLayout();
        layout.inputBuses.getReference (0) = juce::AudioChannelSet::mono();
        layout.inputBuses.getReference (1) = juce::AudioChannelSet::disabled();
        check (p.setBusesLayout (layout), "mono input without sidechain is accepted");
        setParam (p, ParamIDs::fuzzPosition, (float) Choices::postFilter);
        setParam (p, ParamIDs::fuzz, 1.0f);
        setParam (p, ParamIDs::inputGain, 24.0f);
        p.prepareToPlay (44100.0, 256); // smaller than the 512-sample blocks run() sends
        bool finite = false;
        const auto rms = run (p, 200, finite);
        check (finite && rms[0] > 0.01, "mono in, fuzz post at full drive, oversized blocks: finite and audible", rms[0]);
    }

    // Bypass passes the input straight through.
    {
        FuzzyFilterProcessor p;
        setParam (p, ParamIDs::bypass, 1.0f);
        p.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buffer (juce::jmax (p.getTotalNumInputChannels(), p.getTotalNumOutputChannels()), 512);
        buffer.clear();
        for (int i = 0; i < 512; ++i) { buffer.setSample (0, i, 0.25f); buffer.setSample (1, i, -0.25f); }
        juce::MidiBuffer midi;
        p.processBlock (buffer, midi);
        check (buffer.getSample (0, 100) == 0.25f && buffer.getSample (1, 100) == -0.25f, "bypass: input passes unchanged");
    }

    // Saving and loading state restores parameters.
    {
        FuzzyFilterProcessor a;
        setParam (a, ParamIDs::frequency, 333.0f);
        setParam (a, ParamIDs::fuzzCharacter, (float) Choices::silicon);
        juce::MemoryBlock state;
        a.getStateInformation (state);

        FuzzyFilterProcessor b;
        b.setStateInformation (state.getData(), (int) state.getSize());
        const float f = b.apvts.getRawParameterValue (ParamIDs::frequency)->load();
        const float c = b.apvts.getRawParameterValue (ParamIDs::fuzzCharacter)->load();
        check (std::abs (f - 333.0f) < 1.0f && (int) c == Choices::silicon, "state: save and load round trip", f);
    }


    // Modulation: LFO 1 on Frequency changes the sound.
    {
        FuzzyFilterProcessor plain, modded;
        for (auto* p : { &plain, &modded })
        {
            setParam (*p, ParamIDs::fuzzOn, 0.0f);
            setParam (*p, ParamIDs::q, 0.7f);
        }
        setParam (modded, ModParamIds::lfo (1, "Rate"), 3.0f);
        setParam (modded, ModParamIds::amount (Mod::lfo1, Mod::frequency), 0.5f);
        plain.prepareToPlay (48000.0, 512);
        modded.prepareToPlay (48000.0, 512);
        bool f1 = false, f2 = false;
        const auto a = run (plain, 100, f1);
        const auto b = run (modded, 100, f2);
        check (f1 && f2, "mod: output stays finite with LFO 1 on Frequency");
        check (std::abs (a[0] - b[0]) > 0.002, "mod: LFO 1 on Frequency changes the output level", b[0] - a[0]);
    }

    // Envelope follower on the sidechain: the clicks open Env 1.
    {
        FuzzyFilterProcessor p;
        setParam (p, ModParamIds::env (1, "Source"), (float) Choices::followerSidechain);
        setParam (p, ModParamIds::env (1, "Threshold"), -20.0f);
        setParam (p, ModParamIds::env (1, "Attack"), 0.001f);
        setParam (p, ModParamIds::env (1, "Decay"), 1.0f);
        setParam (p, ModParamIds::env (1, "Sustain"), 0.0f);
        p.prepareToPlay (48000.0, 512);
        bool finite = false;
        float peak = 0.0f;
        for (int b = 0; b < 30; ++b)
        {
            run (p, 1, finite);
            peak = std::max (peak, p.getSourceValue (Mod::env1));
        }
        check (peak > 0.3f, "mod: sidechain clicks trigger Env 1", peak);
    }

    // Stress: every grid crossing at +100%, audio-rate LFOs, assigns on LFO rates.
    {
        FuzzyFilterProcessor p;
        for (int src = 0; src < Mod::numSources; ++src)
            for (int dst = 0; dst < Mod::numDestinations; ++dst)
                setParam (p, ModParamIds::amount (src, dst), 1.0f);
        for (int l = 1; l <= 4; ++l)
        {
            setParam (p, ModParamIds::lfo (l, "Rate"), 1500.0f);
            setParam (p, ModParamIds::lfo (l, "Wave"), (float) (l + 1));
            setParam (p, ModParamIds::lfo (l, "Stereo"), 90.0f);
        }
        setParam (p, Mod::assignTargetIds[0], 1.0f); // LFO 1 Rate
        setParam (p, Mod::assignTargetIds[1], 5.0f); // Env 1 Attack
        setParam (p, Mod::assignTargetIds[2], 21.0f); // Comb Level (last choice)
        setParam (p, ParamIDs::excite1Source, (float) Choices::gridOnly);
        p.prepareToPlay (48000.0, 512);
        bool finite = false;
        const auto rms = run (p, 200, finite);
        check (finite && rms[0] < 2.5, "mod stress: every crossing at 100% stays finite and bounded", rms[0]);
    }

    // Parameter count: 25 sound + 28 LFO + 14 envelope + 3 assign + 112 grid.
    {
        FuzzyFilterProcessor p;
        const int count = p.getParameters().size();
        check (count == 25 + 28 + 14 + 3 + 112, "parameter count", count);
    }

    std::printf ("\n%s: %d failed\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures);
    return failures == 0 ? 0 : 1;
}
