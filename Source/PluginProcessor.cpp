#include "PluginProcessor.h"

namespace
{
const char* const kTypeOutputNames[FuzzyFilterProcessor::kNumTypeOutputs] = {
    "Low Pass", "Band Pass", "High Pass", "Notch Pass", "Comb"
};
const char* const kLevelIds[FuzzyFilterProcessor::kNumTypeOutputs] = {
    ParamIDs::levelLP, ParamIDs::levelBP, ParamIDs::levelHP, ParamIDs::levelNP, ParamIDs::levelComb
};

constexpr int kOversamplingFactorLog2 = 2; // 4x for the fuzz
} // namespace

juce::AudioProcessor::BusesProperties FuzzyFilterProcessor::makeBuses()
{
    using juce::AudioChannelSet;
    auto buses = BusesProperties()
                     .withInput ("Input", AudioChannelSet::stereo(), true)
                     .withInput ("Sidechain", AudioChannelSet::stereo(), true)
                     .withOutput ("Output", AudioChannelSet::stereo(), true);

    // One extra stereo output per filter type. They are off until the host
    // turns them on, so the plugin loads as a normal stereo effect.
    for (auto* name : kTypeOutputNames)
        buses = buses.withOutput (name, AudioChannelSet::stereo(), false);

    return buses;
}

FuzzyFilterProcessor::FuzzyFilterProcessor()
    : AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "FuzzyFilter", createParameterLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };

    pInputGain = get (ParamIDs::inputGain);
    pFuzzOn = get (ParamIDs::fuzzOn);
    pFuzzPosition = get (ParamIDs::fuzzPosition);
    pFuzz = get (ParamIDs::fuzz);
    pFuzzVolume = get (ParamIDs::fuzzVolume);
    pFuzzBias = get (ParamIDs::fuzzBias);
    pFuzzCharacter = get (ParamIDs::fuzzCharacter);
    pFuzzMix = get (ParamIDs::fuzzMix);
    pFrequency = get (ParamIDs::frequency);
    pQ = get (ParamIDs::q);
    pSpaceL = get (ParamIDs::spaceL);
    pSpaceR = get (ParamIDs::spaceR);
    pExcite1 = get (ParamIDs::excite1);
    pExcite2 = get (ParamIDs::excite2);
    pExcite1Source = get (ParamIDs::excite1Source);
    pExcite2Source = get (ParamIDs::excite2Source);
    pCombDamping = get (ParamIDs::combDamping);
    for (int t = 0; t < kNumTypeOutputs; ++t)
        pLevels[t] = get (kLevelIds[t]);
    pFilterMix = get (ParamIDs::filterMix);
    pOutputMode = get (ParamIDs::outputMode);
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (ParamIDs::bypass));
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

    // The oversampler always runs (even with the fuzz off) so the latency the
    // host compensates for never changes.
    setLatencySamples (juce::roundToInt (oversampling->getLatencyInSamples()));

    const double rampSeconds = 0.02;
    auto init = [&] (auto& sv, float value) { sv.reset (sampleRate, rampSeconds); sv.setCurrentAndTargetValue (value); };
    init (smFrequency, pFrequency->load());
    init (smInputGain, pInputGain->load());
    init (smQ, pQ->load());
    init (smSpaceL, pSpaceL->load());
    init (smSpaceR, pSpaceR->load());
    init (smExcite1, pExcite1->load());
    init (smExcite2, pExcite2->load());
    init (smCombDamping, pCombDamping->load());
    init (smFilterMix, pFilterMix->load());
    for (int t = 0; t < kNumTypeOutputs; ++t)
        init (smLevels[t], ff::dbToGain (pLevels[t]->load(), kLevelOffDb));

    work.setSize (2, maxBlock);
    sidechain.setSize (2, maxBlock);
    dryForExcite.setSize (2, maxBlock);
    typeOut.setSize (kNumTypeOutputs * 2, maxBlock);
}

void FuzzyFilterProcessor::runFuzz (juce::AudioBuffer<float>& buffer, int numSamples)
{
    juce::dsp::AudioBlock<float> block (buffer);
    auto sub = block.getSubBlock (0, (size_t) numSamples);
    auto up = oversampling->processSamplesUp (sub);

    const bool fuzzOn = pFuzzOn->load() > 0.5f;
    if (fuzzOn)
    {
        ff::FuzzFace::Params fp;
        fp.fuzz = pFuzz->load();
        fp.bias = pFuzzBias->load();
        fp.silicon = (int) pFuzzCharacter->load() == Choices::silicon;
        fp.volume = juce::Decibels::decibelsToGain (pFuzzVolume->load());
        fp.mix = pFuzzMix->load();

        for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
        {
            auto& fz = fuzz[ch];
            fz.setParams (fp);
            auto* d = up.getChannelPointer (ch);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
                d[i] = fz.process (d[i]);
        }
    }

    oversampling->processSamplesDown (sub);
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

    const bool separate = (int) pOutputMode->load() == Choices::separate;

    auto writeOutputs = [&] (bool typesAvailable)
    {
        auto mainOut = getBusBuffer (buffer, false, 0);
        for (int ch = 0; ch < mainOut.getNumChannels(); ++ch)
            mainOut.copyFrom (ch, 0, work, juce::jmin (ch, 1), 0, n);

        for (int t = 0; t < kNumTypeOutputs; ++t)
        {
            const int busIndex = 1 + t;
            if (busIndex >= getBusCount (false)) break;
            auto out = getBusBuffer (buffer, false, busIndex);
            for (int ch = 0; ch < out.getNumChannels(); ++ch)
            {
                if (separate && typesAvailable)
                    out.copyFrom (ch, 0, typeOut, t * 2 + juce::jmin (ch, 1), 0, n);
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

    // 2. Input gain and saturation.
    smInputGain.setTargetValue (pInputGain->load());
    {
        auto* l = work.getWritePointer (0);
        auto* r = work.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            const float db = smInputGain.getNextValue();
            const float g = juce::Decibels::decibelsToGain (db);
            const float sat = ff::InputStage::saturationAmount (db);
            l[i] = ff::InputStage::process (l[i], g, sat);
            r[i] = ff::InputStage::process (r[i], g, sat);
        }
    }

    // The input audio that can drive Excite, taken after the input stage.
    for (int ch = 0; ch < 2; ++ch)
        dryForExcite.copyFrom (ch, 0, work, ch, 0, n);

    // 3. Fuzz before the filter.
    const bool fuzzPost = (int) pFuzzPosition->load() == Choices::postFilter;
    if (! fuzzPost)
        runFuzz (work, n);

    // 4. Filter and output mixer.
    smFrequency.setTargetValue (pFrequency->load());
    smQ.setTargetValue (pQ->load());
    smSpaceL.setTargetValue (pSpaceL->load());
    smSpaceR.setTargetValue (pSpaceR->load());
    smExcite1.setTargetValue (pExcite1->load());
    smExcite2.setTargetValue (pExcite2->load());
    smCombDamping.setTargetValue (pCombDamping->load());
    smFilterMix.setTargetValue (pFilterMix->load());
    for (int t = 0; t < kNumTypeOutputs; ++t)
        smLevels[t].setTargetValue (ff::dbToGain (pLevels[t]->load(), kLevelOffDb));

    auto sourcePtr = [this] (float choice, int ch) -> const float*
    {
        switch ((int) choice)
        {
            case Choices::inputAudio: return dryForExcite.getReadPointer (ch);
            case Choices::sidechain:  return sidechain.getReadPointer (ch);
            default:                  return nullptr; // mod grid only (step 3)
        }
    };
    const float* e1L = sourcePtr (pExcite1Source->load(), 0);
    const float* e1R = sourcePtr (pExcite1Source->load(), 1);
    const float* e2L = sourcePtr (pExcite2Source->load(), 0);
    const float* e2R = sourcePtr (pExcite2Source->load(), 1);

    float* wl = work.getWritePointer (0);
    float* wr = work.getWritePointer (1);
    float* t[kNumTypeOutputs * 2];
    for (int c = 0; c < kNumTypeOutputs * 2; ++c)
        t[c] = typeOut.getWritePointer (c);

    ff::QuadPeakFilter::Params fp;
    ff::QuadPeakFilter::Outputs oL, oR;
    for (int i = 0; i < n; ++i)
    {
        fp.frequency = smFrequency.getNextValue();
        fp.q = smQ.getNextValue();
        fp.spaceL = smSpaceL.getNextValue();
        fp.spaceR = smSpaceR.getNextValue();
        fp.excite1 = smExcite1.getNextValue();
        fp.excite2 = smExcite2.getNextValue();
        fp.combDamping = smCombDamping.getNextValue();

        filter.process (wl[i], wr[i],
                        e1L ? e1L[i] : 0.0f, e1R ? e1R[i] : 0.0f,
                        e2L ? e2L[i] : 0.0f, e2R ? e2R[i] : 0.0f,
                        fp, oL, oR);

        const float typesL[kNumTypeOutputs] = { oL.lp, oL.bp, oL.hp, oL.np, oL.comb };
        const float typesR[kNumTypeOutputs] = { oR.lp, oR.bp, oR.hp, oR.np, oR.comb };

        float mixL = 0.0f, mixR = 0.0f;
        for (int k = 0; k < kNumTypeOutputs; ++k)
        {
            const float level = smLevels[k].getNextValue();
            mixL += level * typesL[k];
            mixR += level * typesR[k];
            t[k * 2][i] = typesL[k];
            t[k * 2 + 1][i] = typesR[k];
        }

        const float m = smFilterMix.getNextValue();
        wl[i] = (1.0f - m) * wl[i] + m * mixL;
        wr[i] = (1.0f - m) * wr[i] + m * mixR;
    }

    // 5. Fuzz after the filter.
    if (fuzzPost)
        runFuzz (work, n);

    writeOutputs (true);
}

juce::AudioProcessorEditor* FuzzyFilterProcessor::createEditor()
{
    // Plain knobs for now; the designed UI comes in step 4.
    auto* editor = new juce::GenericAudioProcessorEditor (*this);
    editor->setSize (520, 720);
    return editor;
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
