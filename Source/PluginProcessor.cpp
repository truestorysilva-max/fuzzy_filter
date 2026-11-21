#include "PluginProcessor.h"
#include "ui/PluginEditor.h"

namespace
{
[[maybe_unused]] const char* const kTypeOutputNames[FuzzyFilterProcessor::kNumTypeOutputs] = {
    "Low Pass", "Band Pass", "High Pass", "Notch Pass", "Comb"
};
const int kLevelSlots[FuzzyFilterProcessor::kNumTypeOutputs] = {
    Mod::levelLP, Mod::levelBP, Mod::levelHP, Mod::levelNP, Mod::levelComb
};

constexpr int kOversamplingFactorLog2 = 2; // 4x for the fuzz
constexpr float kSensitivityMaxDb = 24.0f;  // Sensitivity 100% = +24 dB into the follower
} // namespace

juce::AudioProcessor::BusesProperties FuzzyFilterProcessor::makeBuses()
{
    using juce::AudioChannelSet;
    auto buses = BusesProperties()
                     .withInput ("Input", AudioChannelSet::stereo(), true)
                    #if JUCE_IOS
                     // Off until the host turns it on: iOS hosts that don't
                     // offer sidechains then see a plain stereo effect.
                     .withInput ("Sidechain", AudioChannelSet::stereo(), false)
                    #else
                     .withInput ("Sidechain", AudioChannelSet::stereo(), true)
                    #endif
                     .withOutput ("Output", AudioChannelSet::stereo(), true);

   #if ! JUCE_IOS
    // One extra stereo output per filter type. They are off until the host
    // turns them on, so the plugin loads as a normal stereo effect. Left out
    // on iPad: few iOS hosts route extra AUv3 outputs, and some refuse to
    // load an effect that declares them.
    for (auto* name : kTypeOutputNames)
        buses = buses.withOutput (name, AudioChannelSet::stereo(), false);
   #endif

    return buses;
}

FuzzyFilterProcessor::FuzzyFilterProcessor()
    : AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "FuzzyFilter", createParameterLayout())
{
    auto get = [this] (const juce::String& id)
    {
        auto* p = apvts.getRawParameterValue (id);
        jassert (p != nullptr);
        return p;
    };

    pFuzzOn = get (ParamIDs::fuzzOn);
    pFuzzPosition = get (ParamIDs::fuzzPosition);
    pFuzzCharacter = get (ParamIDs::fuzzCharacter);
    pExcite1Source = get (ParamIDs::excite1Source);
    pExcite2Source = get (ParamIDs::excite2Source);
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (ParamIDs::bypass));

    for (int l = 0; l < 4; ++l)
    {
        pLfoWave[l] = get (ModParamIds::lfo (l + 1, "Wave"));
        pLfoSync[l] = get (ModParamIds::lfo (l + 1, "Sync"));
        pLfoDivision[l] = get (ModParamIds::lfo (l + 1, "Division"));
        pLfoPhase[l] = get (ModParamIds::lfo (l + 1, "Phase"));
        pLfoStereo[l] = get (ModParamIds::lfo (l + 1, "Stereo"));
        pLfoRetrigger[l] = get (ModParamIds::lfo (l + 1, "Retrigger"));
    }
    for (int e = 0; e < 2; ++e)
    {
        pEnvSensitivity[e] = get (ModParamIds::env (e + 1, "Sensitivity"));
        pFolAttack[e] = get (ModParamIds::env (e + 1, "FolAttack"));
        pFolHold[e] = get (ModParamIds::env (e + 1, "FolHold"));
        pFolRelease[e] = get (ModParamIds::env (e + 1, "FolRelease"));
        pEnvSource[e] = get (ModParamIds::env (e + 1, "Source"));
    }
    for (int a = 0; a < Mod::numAssigns; ++a)
        pAssignTarget[a] = get (Mod::assignTargetIds[(size_t) a]);
    for (int s = 0; s < Mod::numSources; ++s)
        for (int d = 0; d < Mod::numDestinations; ++d)
            pAmounts[s][d] = get (ModParamIds::amount (s, d));

    for (int s = 0; s < Mod::numSlots; ++s)
    {
        slots[(size_t) s].param = apvts.getParameter (Mod::slotParamIds[(size_t) s]);
        jassert (slots[(size_t) s].param != nullptr);
        slotLive[(size_t) s].store (-1.0f);
    }

    routings.reserve ((size_t) (Mod::numSources * Mod::numDestinations));
}

juce::AudioProcessorParameter* FuzzyFilterProcessor::getBypassParameter() const { return bypassParam; }

bool FuzzyFilterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    using juce::AudioChannelSet;
    const auto mono = AudioChannelSet::mono();
    const auto stereo = AudioChannelSet::stereo();

    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != stereo && mainOut != mono) return false;
    if (mainIn != stereo && mainIn != mono) return false;

    for (int i = 1; i < layouts.inputBuses.size(); ++i)
    {
        const auto& set = layouts.inputBuses.getReference (i);
        if (! set.isDisabled() && set != mono && set != stereo) return false;
    }
    for (int i = 1; i < layouts.outputBuses.size(); ++i)
    {
        const auto& set = layouts.outputBuses.getReference (i);
        if (! set.isDisabled() && set != stereo) return false;
    }
    return true;
}

void FuzzyFilterProcessor::prepareBuffers (int maxBlock)
{
    work.setSize (2, maxBlock);
    sidechain.setSize (2, maxBlock);
    dryForExcite.setSize (2, maxBlock);
    typeOut.setSize (kNumTypeOutputs * 2, maxBlock);
    modValues.setSize (Mod::numSlots * 2, maxBlock);
    gridExcite.setSize (4, maxBlock);
    fuzzParams.setSize (8, maxBlock);
}

void FuzzyFilterProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int maxBlock = juce::jmax (1, samplesPerBlock);
    preparedSampleRate = sampleRate;

    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        2, kOversamplingFactorLog2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampling->initProcessing ((size_t) maxBlock);

    const double osRate = sampleRate * (double) oversampling->getOversamplingFactor();
    for (auto& f : fuzz)
        f.prepare (osRate);
    filter.prepare (sampleRate);

    for (auto& l : lfos) l.prepare (sampleRate);
    for (auto& f : followers) f.prepare (sampleRate);
    for (auto& g : gates) g.reset();
    for (auto& e : envelopes) e.prepare (sampleRate);
    for (int s = 0; s < Mod::numSources; ++s)
        prevSourceL[s] = prevSourceR[s] = 0.0f;

    // The oversampler always runs (even with the fuzz off) so the latency the
    // host compensates for never changes.
    setLatencySamples (juce::roundToInt (oversampling->getLatencyInSamples()));

    for (auto& s : slots)
    {
        s.base.reset (sampleRate, 0.02);
        s.base.setCurrentAndTargetValue (s.param->getValue());
        s.staticPlain = s.plain (s.param->getValue());
    }

    prepareBuffers (maxBlock);
}

// Reads the grid once per block: which crossings are active, what the Assign
// columns point at, and which slots need per-sample values.
void FuzzyFilterProcessor::updateRoutings()
{
    routings.clear();
    spaceRHasOwnRouting = false;
    for (auto& s : slots)
        s.dynamic = false;

    int assignSlot[Mod::numAssigns];
    for (int a = 0; a < Mod::numAssigns; ++a)
        assignSlot[a] = Mod::assignChoiceToSlot ((int) pAssignTarget[a]->load());

    for (int src = 0; src < Mod::numSources; ++src)
        for (int dst = 0; dst < Mod::numDestinations; ++dst)
        {
            const float amount = pAmounts[src][dst]->load();
            if (std::abs (amount) < 1.0e-5f) continue;
            const int slot = dst < Mod::numFixedDestinations ? dst : assignSlot[dst - Mod::numFixedDestinations];
            if (slot < 0) continue;
            routings.push_back ({ src, slot, amount });
            slots[(size_t) slot].dynamic = true;
            if (slot == Mod::spaceR) spaceRHasOwnRouting = true;
        }

    // Space R follows Space L's modulation unless it has its own routing,
    // like the normalled Radiate inputs on the QPAS.
    if (slots[Mod::spaceL].dynamic && ! spaceRHasOwnRouting)
        slots[Mod::spaceR].dynamic = true;

    for (auto& s : slots)
    {
        s.base.setTargetValue (s.param->getValue());
        if (s.base.isSmoothing())
            s.dynamic = true;
        if (! s.dynamic)
            s.staticPlain = s.plain (s.base.getCurrentValue());
    }
}

// Runs the LFOs, followers and envelopes for the block and writes every
// dynamic slot's normalised value, per channel and per sample, into modValues.
void FuzzyFilterProcessor::runModulation (int n)
{
    // Host tempo, for LFO tempo sync.
    double bpm = 120.0, ppq = 0.0;
    bool playing = false, hasPpq = false;
    if (auto* head = getPlayHead())
        if (auto pos = head->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) { ppq = *p; hasPpq = true; }
            playing = pos->getIsPlaying();
        }

    ff::Lfo::Wave waves[4];
    bool synced[4];
    float syncedHz[4], phaseOffset[4], stereoOffset[4];
    int retrigger[4];
    for (int l = 0; l < 4; ++l)
    {
        waves[l] = (ff::Lfo::Wave) juce::jlimit (0, (int) ff::Lfo::numWaves - 1, (int) pLfoWave[l]->load());
        synced[l] = (int) pLfoSync[l]->load() == Choices::tempo;
        const double beats = Choices::divisionBeats[juce::jlimit (0, Choices::numDivisions - 1, (int) pLfoDivision[l]->load())];
        syncedHz[l] = (float) (bpm / 60.0 / beats);
        phaseOffset[l] = pLfoPhase[l]->load() / 360.0f;
        stereoOffset[l] = pLfoStereo[l]->load() / 360.0f;
        retrigger[l] = (int) pLfoRetrigger[l]->load();
        // While the host plays, a synced LFO locks its phase to the song
        // position, shifted by where the last restart put it. (Without that
        // shift, the next block would undo every restart.)
        if (retrigger[l] == Choices::retriggerOff)
            syncRestartOffset[l] = 0.0;
        syncBeats[l] = beats;
        if (synced[l] && playing && hasPpq)
            lfos[l].setPhase (ppq / beats - syncRestartOffset[l]);
    }

    // Follower inputs.
    const float postGain = juce::Decibels::decibelsToGain (slots[Mod::inputGain].plain (slots[Mod::inputGain].base.getCurrentValue()));
    const float* detL[2];
    const float* detR[2];
    float detGain[2], sensGain[2];
    for (int e = 0; e < 2; ++e)
    {
        const int src = (int) pEnvSource[e]->load();
        const auto& buf = src == Choices::followerSidechain ? sidechain : work;
        detL[e] = buf.getReadPointer (0);
        detR[e] = buf.getReadPointer (1);
        detGain[e] = src == Choices::postInputGain ? postGain : 1.0f;
        sensGain[e] = juce::Decibels::decibelsToGain (pEnvSensitivity[e]->load() * kSensitivityMaxDb);
        followers[e].setTimes (pFolAttack[e]->load(), pFolHold[e]->load(), pFolRelease[e]->load());
    }

    float baseVals[Mod::numSlots];
    for (int i = 0; i < n; ++i)
    {
        // 1. Knob positions plus modulation (from the previous sample's sources).
        for (int s = 0; s < Mod::numSlots; ++s)
        {
            auto& slot = slots[(size_t) s];
            if (! slot.dynamic) continue;
            const float b = slot.base.getNextValue();
            baseVals[s] = b;
            modValues.setSample (s * 2, i, b);
            modValues.setSample (s * 2 + 1, i, b);
        }
        for (const auto& r : routings)
        {
            float* l = modValues.getWritePointer (r.slot * 2);
            float* rr = modValues.getWritePointer (r.slot * 2 + 1);
            l[i] += r.amount * prevSourceL[r.source];
            rr[i] += r.amount * prevSourceR[r.source];
        }
        if (slots[Mod::spaceL].dynamic && ! spaceRHasOwnRouting)
            for (int ch = 0; ch < 2; ++ch)
                modValues.getWritePointer (Mod::spaceR * 2 + ch)[i]
                    += modValues.getSample (Mod::spaceL * 2 + ch, i) - baseVals[Mod::spaceL];
        for (int e = 0; e < 2; ++e)
        {
            const int s = Mod::excite1 + e;
            for (int ch = 0; ch < 2; ++ch)
                gridExcite.setSample (e * 2 + ch, i,
                                      slots[(size_t) s].dynamic ? modValues.getSample (s * 2 + ch, i) - baseVals[s] : 0.0f);
        }

        // 2. Followers and envelopes (mono).
        bool opened[2] = { false, false };
        float envOut[2], folOut[2];
        for (int e = 0; e < 2; ++e)
        {
            const float x = std::max (std::abs (detL[e][i]), std::abs (detR[e][i])) * detGain[e];
            const float level = followers[e].process (x) * sensGain[e];
            const int base = e == 0 ? Mod::env1Attack : Mod::env2Attack;
            const float thresholdDb = slotValue (e == 0 ? Mod::env1Threshold : Mod::env2Threshold, 0, i);
            opened[e] = gates[e].update (level, thresholdDb);
            envelopes[e].gate (gates[e].isOpen());
            envOut[e] = envelopes[e].tick (slotValue (base, 0, i), slotValue (base + 1, 0, i),
                                           slotValue (base + 2, 0, i), slotValue (base + 3, 0, i));
            folOut[e] = juce::jlimit (0.0f, 1.0f, level);
        }

        // 3. LFOs.
        for (int l = 0; l < 4; ++l)
        {
            if ((retrigger[l] == Choices::retriggerEnv1 && opened[0]) || (retrigger[l] == Choices::retriggerEnv2 && opened[1]))
            {
                lfos[l].setPhase (0.0);
                lfoRestarts[l].fetch_add (1);
                if (synced[l] && playing && hasPpq)
                {
                    const double ppqNow = ppq + (double) i * bpm / 60.0 / preparedSampleRate;
                    const double cycles = ppqNow / syncBeats[l];
                    syncRestartOffset[l] = cycles - std::floor (cycles);
                }
            }
            const float hz = synced[l] ? syncedHz[l] : slotValue (Mod::lfo1Rate + l, 0, i);
            // The shape can be moved by the grid through an Assign column;
            // the modulated value steps through the shapes in menu order.
            const auto wave = slots[(size_t) (Mod::lfo1Wave + l)].dynamic
                                ? (ff::Lfo::Wave) juce::jlimit (0, (int) ff::Lfo::numWaves - 1, juce::roundToInt (slotValue (Mod::lfo1Wave + l, 0, i)))
                                : waves[l];
            lfos[l].tick (hz, wave, phaseOffset[l], stereoOffset[l], prevSourceL[Mod::lfo1 + l], prevSourceR[Mod::lfo1 + l]);
        }
        for (int e = 0; e < 2; ++e)
        {
            prevSourceL[Mod::env1 + e] = prevSourceR[Mod::env1 + e] = envOut[e];
            prevSourceL[Mod::follower1 + e] = prevSourceR[Mod::follower1 + e] = folOut[e];
        }
    }

    for (int s = 0; s < Mod::numSources; ++s)
        sourceValues[(size_t) s].store (prevSourceL[s]);
    for (int s = 0; s < Mod::numSlots; ++s)
        slotLive[(size_t) s].store (slots[(size_t) s].dynamic ? juce::jlimit (0.0f, 1.0f, modValues.getSample (s * 2, n - 1)) : -1.0f);
    for (int l = 0; l < 4; ++l)
        lfoPhase[l].store (lfos[l].getPhase());
}

void FuzzyFilterProcessor::runInputStage (int n)
{
    for (int ch = 0; ch < 2; ++ch)
    {
        float* d = work.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            const float db = slotValue (Mod::inputGain, ch, i);
            d[i] = ff::InputStage::process (d[i], juce::Decibels::decibelsToGain (db), ff::InputStage::saturationAmount (db));
        }
        const float peak = work.getMagnitude (ch, 0, n);
        if (peak > inputPeak[ch].load())
            inputPeak[ch].store (peak);
    }
}

void FuzzyFilterProcessor::runFuzz (juce::AudioBuffer<float>& buffer, int n)
{
    juce::dsp::AudioBlock<float> block (buffer);
    auto sub = block.getSubBlock (0, (size_t) n);
    auto up = oversampling->processSamplesUp (sub);

    if (pFuzzOn->load() > 0.5f)
    {
        // Per-sample settings at the base rate (modulation can move them).
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
            {
                fuzzParams.setSample (ch * 4 + 0, i, slotValue (Mod::fuzz, ch, i));
                fuzzParams.setSample (ch * 4 + 1, i, slotValue (Mod::fuzzBias, ch, i));
                fuzzParams.setSample (ch * 4 + 2, i, juce::Decibels::decibelsToGain (slotValue (Mod::fuzzVolume, ch, i)));
                fuzzParams.setSample (ch * 4 + 3, i, slotValue (Mod::fuzzMix, ch, i));
            }

        const bool silicon = (int) pFuzzCharacter->load() == Choices::silicon;
        const size_t factor = oversampling->getOversamplingFactor();
        for (size_t ch = 0; ch < up.getNumChannels() && ch < 2; ++ch)
        {
            auto& fz = fuzz[ch];
            auto* d = up.getChannelPointer (ch);
            const float* pf = fuzzParams.getReadPointer ((int) ch * 4 + 0);
            const float* pb = fuzzParams.getReadPointer ((int) ch * 4 + 1);
            const float* pv = fuzzParams.getReadPointer ((int) ch * 4 + 2);
            const float* pm = fuzzParams.getReadPointer ((int) ch * 4 + 3);
            for (size_t j = 0; j < up.getNumSamples(); ++j)
            {
                const size_t i = j / factor;
                fz.setParams ({ pf[i], pb[i], silicon, pv[i], pm[i] });
                d[j] = fz.process (d[j]);
            }
        }
    }

    oversampling->processSamplesDown (sub);
}

void FuzzyFilterProcessor::runFilter (int n)
{
    struct ExciteInput
    {
        const float* l = nullptr;
        const float* r = nullptr;
        bool fromGrid = false;
    };
    auto exciteInput = [this] (float choice, int index) -> ExciteInput
    {
        switch ((int) choice)
        {
            case Choices::inputAudio: return { dryForExcite.getReadPointer (0), dryForExcite.getReadPointer (1), false };
            case Choices::sidechain:  return { sidechain.getReadPointer (0), sidechain.getReadPointer (1), false };
            default:                  return { gridExcite.getReadPointer (index * 2), gridExcite.getReadPointer (index * 2 + 1), true };
        }
    };
    const ExciteInput ex[2] = { exciteInput (pExcite1Source->load(), 0), exciteInput (pExcite2Source->load(), 1) };

    float staticLevel[kNumTypeOutputs];
    for (int t = 0; t < kNumTypeOutputs; ++t)
        staticLevel[t] = ff::dbToGain (slots[(size_t) kLevelSlots[t]].staticPlain, kLevelOffDb);

    float* w[2] = { work.getWritePointer (0), work.getWritePointer (1) };
    float* t[kNumTypeOutputs * 2];
    for (int c = 0; c < kNumTypeOutputs * 2; ++c)
        t[c] = typeOut.getWritePointer (c);

    ff::QuadPeakFilter::Params p[2];
    ff::QuadPeakFilter::Outputs o[2];
    for (int i = 0; i < n; ++i)
    {
        float exSig[2][2]; // [excite][channel]
        for (int ch = 0; ch < 2; ++ch)
        {
            auto& pc = p[ch];
            pc.frequency = slotValue (Mod::frequency, ch, i);
            pc.q = slotValue (Mod::q, ch, i);
            pc.spaceL = slotValue (Mod::spaceL, ch, i);
            pc.spaceR = slotValue (Mod::spaceR, ch, i);
            pc.combDamping = slotValue (Mod::combDamping, ch, i);

            for (int e = 0; e < 2; ++e)
            {
                const int slot = Mod::excite1 + e;
                float amount = slotValue (slot, ch, i);
                if (ex[e].fromGrid)
                {
                    // "Mod grid only": the grid's signal is what excites the
                    // filter, and the Excite knob sets how much.
                    const float gridSignal = (ch == 0 ? ex[e].l : ex[e].r)[i];
                    amount = slots[(size_t) slot].dynamic ? juce::jlimit (0.0f, 1.0f, amount - gridSignal) : amount;
                    exSig[e][ch] = gridSignal;
                }
                else
                {
                    exSig[e][ch] = (ch == 0 ? ex[e].l : ex[e].r)[i];
                }
                (e == 0 ? pc.excite1 : pc.excite2) = amount;
            }
        }

        filter.process (w[0][i], w[1][i], exSig[0][0], exSig[0][1], exSig[1][0], exSig[1][1], p[0], p[1], o[0], o[1]);

        for (int ch = 0; ch < 2; ++ch)
        {
            const float types[kNumTypeOutputs] = { o[ch].lp, o[ch].bp, o[ch].hp, o[ch].np, o[ch].comb };
            float mix = 0.0f;
            for (int k = 0; k < kNumTypeOutputs; ++k)
            {
                const auto& s = slots[(size_t) kLevelSlots[k]];
                const float level = s.dynamic ? ff::dbToGain (slotValue (kLevelSlots[k], ch, i), kLevelOffDb) : staticLevel[k];
                mix += level * types[k];
                t[k * 2 + ch][i] = types[k];
            }
            const float m = slotValue (Mod::filterMix, ch, i);
            w[ch][i] = (1.0f - m) * w[ch][i] + m * mix;
        }
    }
}

void FuzzyFilterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0) return;

    if (n > work.getNumSamples())
        prepareToPlay (preparedSampleRate, n); // host sent a bigger block than promised

    // 1. Copy the main input (mono is copied to both sides) and the sidechain.
    auto mainIn = getBusBuffer (buffer, true, 0);
    const int inCh = mainIn.getNumChannels();
    for (int ch = 0; ch < 2; ++ch)
    {
        if (inCh > 0) work.copyFrom (ch, 0, mainIn, juce::jmin (ch, inCh - 1), 0, n);
        else          work.clear (ch, 0, n);
    }

    sidechain.clear (0, n);
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        auto sc = getBusBuffer (buffer, true, 1);
        const int scCh = sc.getNumChannels();
        for (int ch = 0; ch < 2 && scCh > 0; ++ch)
            sidechain.copyFrom (ch, 0, sc, juce::jmin (ch, scCh - 1), 0, n);
    }

    auto writeOutputs = [&] (bool typesAvailable)
    {
        auto mainOut = getBusBuffer (buffer, false, 0);
        for (int ch = 0; ch < mainOut.getNumChannels(); ++ch)
            mainOut.copyFrom (ch, 0, work, juce::jmin (ch, 1), 0, n);

        for (int ty = 0; ty < kNumTypeOutputs; ++ty)
        {
            const int busIndex = 1 + ty;
            if (busIndex >= getBusCount (false)) break;
            auto out = getBusBuffer (buffer, false, busIndex);
            for (int ch = 0; ch < out.getNumChannels(); ++ch)
            {
                if (typesAvailable)
                    out.copyFrom (ch, 0, typeOut, ty * 2 + juce::jmin (ch, 1), 0, n);
                else
                    out.clear (ch, 0, n);
            }
        }
    };

    if (bypassParam != nullptr && bypassParam->get())
    {
        writeOutputs (false);
        return;
    }

    // 2. Modulation for the whole block (followers listen to the raw input
    //    or the sidechain, so this runs before the input stage).
    updateRoutings();
    runModulation (n);

    // 3. Input gain and saturation.
    runInputStage (n);

    // The input audio that can drive Excite, taken after the input stage.
    for (int ch = 0; ch < 2; ++ch)
        dryForExcite.copyFrom (ch, 0, work, ch, 0, n);

    // 4. Fuzz, filter, fuzz: the fuzz runs in one of the two places.
    const bool fuzzPost = (int) pFuzzPosition->load() == Choices::postFilter;
    if (! fuzzPost)
        runFuzz (work, n);

    runFilter (n);

    if (fuzzPost)
        runFuzz (work, n);

    writeOutputs (true);
}

juce::AudioProcessorEditor* FuzzyFilterProcessor::createEditor()
{
    return new FuzzyFilterEditor (*this);
}

void FuzzyFilterProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void FuzzyFilterProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FuzzyFilterProcessor();
}
