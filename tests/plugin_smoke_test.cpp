// Runs the real plugin processor with no host: checks the bus layouts,
// separate outputs, sidechain, bypass and saving/loading state.

#include "PluginProcessor.h"
#include "ui/PluginEditor.h"

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
std::vector<double> run (FuzzyFilterProcessor& p, int blocks, bool& finite, int startSample = 0)
{
    constexpr int blockSize = 512;
    const int numChannels = juce::jmax (p.getTotalNumInputChannels(), p.getTotalNumOutputChannels());
    juce::AudioBuffer<float> buffer (numChannels, blockSize);
    juce::MidiBuffer midi;
    std::mt19937 rng (7);
    std::uniform_real_distribution<float> noise (-0.3f, 0.3f);

    std::vector<double> sums ((size_t) p.getBusCount (false), 0.0);
    finite = true;
    int sample = startSample;
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

    // Every bus on, every filter type up, Excite on the sidechain.
    {
        FuzzyFilterProcessor p;
        auto layout = p.getBusesLayout();
        for (int i = 1; i < layout.outputBuses.size(); ++i)
            layout.outputBuses.getReference (i) = juce::AudioChannelSet::stereo();
        check (p.setBusesLayout (layout), "all separate outputs can be turned on");

        for (auto* id : { ParamIDs::levelBP, ParamIDs::levelHP, ParamIDs::levelNP, ParamIDs::levelComb })
            setParam (p, id, 0.0f);
        setParam (p, ParamIDs::excite1Source, (float) Choices::sidechain);
        setParam (p, ParamIDs::excite1, 1.0f);
        setParam (p, ParamIDs::q, 0.9f);
        setParam (p, ParamIDs::spaceL, 0.7f);

        p.prepareToPlay (48000.0, 512);
        bool finite = false;
        const auto rms = run (p, 200, finite);
        check (finite, "separate outputs: every output is finite");
        bool allCarry = true;
        for (size_t bus = 1; bus < rms.size(); ++bus)
            allCarry = allCarry && rms[bus] > 0.001;
        check (allCarry, "separate outputs: all 5 filter type outputs carry audio", rms.back());
        check (rms[0] > 0.001, "separate outputs: main output still carries the mix", rms[0]);
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
        setParam (p, Mod::assignTargetIds[2], 21.0f); // Comb Level
        setParam (p, ParamIDs::excite1Source, (float) Choices::gridOnly);
        p.prepareToPlay (48000.0, 512);
        bool finite = false;
        const auto rms = run (p, 200, finite);
        check (finite && rms[0] < 2.5, "mod stress: every crossing at 100% stays finite and bounded", rms[0]);
    }

    // LFO shape as an Assign target: LFO 2 switches LFO 1's shape.
    {
        auto lfo1Trace = [] (bool assigned)
        {
            FuzzyFilterProcessor p;
            setParam (p, ModParamIds::lfo (1, "Rate"), 3.0f);
            setParam (p, ModParamIds::lfo (2, "Rate"), 0.7f);
            setParam (p, ModParamIds::lfo (2, "Wave"), 4.0f); // square
            if (assigned)
            {
                setParam (p, Mod::assignTargetIds[0], (float) (Mod::lfo1Wave - Mod::numFixedDestinations + 1));
                setParam (p, ModParamIds::amount (Mod::lfo2, Mod::numFixedDestinations), 1.0f);
            }
            p.prepareToPlay (48000.0, 512);
            std::vector<float> trace;
            bool finite = false;
            for (int b = 0; b < 200; ++b)
            {
                run (p, 1, finite);
                trace.push_back (p.getSourceValue (Mod::lfo1));
            }
            return std::make_pair (trace, p.getSlotLive (Mod::lfo1Wave));
        };
        const auto plain = lfo1Trace (false);
        const auto moved = lfo1Trace (true);
        double diff = 0.0;
        for (size_t i = 0; i < plain.first.size(); ++i)
            diff = std::max (diff, (double) std::abs (plain.first[i] - moved.first[i]));
        check (juce::String (Mod::slotNames[Mod::lfo1Wave]) == "LFO 1 Shape", "LFO shape is listed as an Assign target");
        check (moved.second >= 0.0f && plain.second < 0.0f, "LFO shape: live value shows only when assigned", moved.second);
        check (diff > 0.2, "LFO shape: the grid changes LFO 1's shape", diff);
    }

    // LFO restart with tempo sync while the host plays: each sidechain click
    // restarts LFO 1, and the restart sticks (the song position doesn't undo it).
    {
        struct Head : juce::AudioPlayHead
        {
            double ppq = 0.0;
            juce::Optional<PositionInfo> getPosition() const override
            {
                PositionInfo info;
                info.setBpm (120.0);
                info.setPpqPosition (ppq);
                info.setIsPlaying (true);
                return info;
            }
        } head;

        FuzzyFilterProcessor p;
        p.setPlayHead (&head);
        setParam (p, ModParamIds::lfo (1, "Sync"), 1.0f);       // tempo, 1/4 = 2 Hz at 120 bpm
        setParam (p, ModParamIds::lfo (1, "Retrigger"), 1.0f);  // when Env 1 fires
        setParam (p, ModParamIds::env (1, "Source"), 2.0f);     // listens to the sidechain
        setParam (p, ModParamIds::env (1, "Threshold"), -10.0f);
        p.prepareToPlay (48000.0, 512);

        bool finite = false, allLocked = true;
        int checked = 0;
        double worst = 0.0;
        for (int b = 0; b < 300; ++b)
        {
            run (p, 1, finite, b * 512); // the sidechain clicks every 12000 samples
            head.ppq += 512.0 / 48000.0 * 2.0;
            const int end = (b + 1) * 512;
            const int sinceClick = end % 12000;
            if (b > 30 && sinceClick > 1024 && sinceClick < 11000)
            {
                double expected = sinceClick * 2.0 / 48000.0;
                expected -= std::floor (expected);
                double diff = std::abs (p.getLfoPhase (0) - expected);
                diff = std::min (diff, 1.0 - diff);
                worst = std::max (worst, diff);
                allLocked = allLocked && diff < 0.02;
                ++checked;
            }
        }
        check (p.getLfoRestarts (0) >= 12, "LFO restart: each sidechain click restarts LFO 1", p.getLfoRestarts (0));
        check (checked > 100 && allLocked, "LFO restart: a tempo-synced LFO keeps its restart while the song plays", worst);
    }

    // Parameter count: 24 sound + 28 LFO + 22 envelope and follower + 3 assign + 112 grid.
    {
        FuzzyFilterProcessor p;
        const int count = p.getParameters().size();
        check (count == 24 + 28 + 22 + 3 + 112, "parameter count", count);
    }

    // Factory presets: each one loads, names itself, and plays finite audio.
    {
        FuzzyFilterProcessor p;
        p.prepareToPlay (48000.0, 512);
        auto& pm = p.presets;
        bool allOk = true;
        for (int i = 0; i < PresetManager::getNumFactoryPresets(); ++i)
        {
            pm.load (i);
            bool finite = false;
            const auto rms = run (p, 40, finite);
            const bool ok = finite && rms[0] < 4.0 && pm.getCurrentName() == PresetManager::getFactoryName (i);
            if (! ok) std::printf ("  preset %s: finite %d rms %.3f\n", PresetManager::getFactoryName (i).toRawUTF8(), (int) finite, rms[0]);
            allOk = allOk && ok;
        }
        check (allOk, "presets: every factory preset loads and plays finite audio", PresetManager::getNumFactoryPresets());

        pm.load (0);
        const float rezInit = p.apvts.getRawParameterValue (ParamIDs::q)->load();
        pm.step (1);
        check (pm.getCurrentIndex() == 1, "presets: next steps to the second preset", pm.getCurrentIndex());
        pm.step (-1);
        pm.step (-1);
        check (pm.getCurrentIndex() == pm.getNumEntries() - 1, "presets: previous wraps to the last", pm.getCurrentIndex());
        pm.load (0);
        check (std::abs (p.apvts.getRawParameterValue (ParamIDs::q)->load() - rezInit) < 1.0e-6f, "presets: Init restores defaults");
        check (p.apvts.getParameter (ParamIDs::q)->getName (16) == "Rez", "Q is shown as Rez");
    }

    // The editor opens, survives a few timer ticks with modulation running,
    // and can be drawn. FF_SNAPSHOT=<file.png> saves a picture of it.
    {
        FuzzyFilterProcessor p;
        p.prepareToPlay (48000.0, 512);
        p.presets.load (1); // Sputter Peaks: has modulation
        bool finite = false;
        run (p, 20, finite);
        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
        const bool sized = editor->getWidth() == FuzzyFilterEditor::designWidth && editor->getHeight() == FuzzyFilterEditor::designHeight;
        check (sized, "editor: opens at 1000 x 650", editor->getWidth());
        for (int i = 0; i < 5; ++i)
        {
            run (p, 4, finite);
            juce::Thread::sleep (40);
            juce::Timer::callPendingTimersSynchronously();
        }
        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        check (image.isValid() && image.getWidth() == 2000, "editor: draws", image.getWidth());

        editor->setSize (800, 520);
        check (editor->getWidth() == 800, "editor: resizes", editor->getWidth());

        // The touch-size mod grid opens over the editor and draws.
        // FF_SNAPSHOT_ZOOM=<file.png> saves a picture of it.
        ui::GridZoom* zoom = nullptr;
        std::function<void (juce::Component&)> find = [&] (juce::Component& c)
        {
            for (auto* child : c.getChildren())
            {
                if (auto* z = dynamic_cast<ui::GridZoom*> (child)) zoom = z;
                else find (*child);
            }
        };
        find (*editor);
        check (zoom != nullptr && ! zoom->isVisible(), "editor: big mod grid exists and starts hidden");
        juce::Image zoomImage;
        if (zoom != nullptr)
        {
            zoom->setVisible (true);
            juce::Timer::callPendingTimersSynchronously();
            zoomImage = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
            check (zoomImage.isValid(), "editor: big mod grid draws");
            zoom->setVisible (false);
        }

        auto save = [] (const char* var, const juce::Image& img)
        {
            if (auto path = juce::SystemStats::getEnvironmentVariable (var, {}); path.isNotEmpty() && img.isValid())
            {
                juce::File file (path);
                file.deleteFile();
                juce::FileOutputStream out (file);
                juce::PNGImageFormat().writeImageToStream (img, out);
            }
        };
        save ("FF_SNAPSHOT", image);
        save ("FF_SNAPSHOT_ZOOM", zoomImage);
    }

    // MIDI notes fire Env 2: it rises on a note, holds while the note is
    // held, falls after note-off, and the audio threshold is ignored.
    {
        FuzzyFilterProcessor p;
        setParam (p, ModParamIds::env (2, "Trigger"), (float) Choices::triggerMidi);
        setParam (p, ModParamIds::env (2, "Threshold"), -60.0f); // audio alone would fire it
        setParam (p, ModParamIds::env (2, "Attack"), 0.001f);
        setParam (p, ModParamIds::env (2, "Sustain"), 0.8f);
        setParam (p, ModParamIds::env (2, "Release"), 0.01f);
        p.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buf (juce::jmax (p.getTotalNumInputChannels(), p.getTotalNumOutputChannels()), 512);
        auto block = [&] (const juce::MidiBuffer& m)
        {
            buf.clear();
            for (int i = 0; i < 512; ++i) buf.setSample (0, i, (i % 64) < 32 ? 0.5f : -0.5f);
            juce::MidiBuffer copy (m);
            p.processBlock (buf, copy);
            return p.getSourceValue (Mod::env2);
        };
        juce::MidiBuffer none, on, off;
        on.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 10);
        off.addEvent (juce::MidiMessage::noteOff (1, 60), 10);
        float silentAudio = 0.0f;
        for (int b = 0; b < 10; ++b) silentAudio = std::max (silentAudio, block (none));
        block (on);
        float held = 0.0f;
        for (int b = 0; b < 20; ++b) held = block (none);
        block (off);
        float released = 1.0f;
        for (int b = 0; b < 20; ++b) released = block (none);
        check (silentAudio < 0.01f, "midi: loud audio does not fire a MIDI-gated envelope", silentAudio);
        check (held > 0.7f, "midi: a held note holds Env 2 at sustain", held);
        check (released < 0.01f, "midi: Env 2 falls after note-off", released);

        // A very short note (on and off one sample apart) still gives a hit.
        juce::MidiBuffer blip;
        blip.addEvent (juce::MidiMessage::noteOn (1, 62, (juce::uint8) 100), 440);
        blip.addEvent (juce::MidiMessage::noteOff (1, 62), 441);
        const float blipPeak = block (blip);
        check (blipPeak > 0.5f, "midi: a one-sample note still fires a full attack", blipPeak);

        // The follower keeps tracking the audio while MIDI fires the envelope.
        check (p.getSourceValue (Mod::follower2) > 0.05f, "midi: Follower 2 still tracks audio", p.getSourceValue (Mod::follower2));
    }

    std::printf ("\n%s: %d failed\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures);
    return failures == 0 ? 0 : 1;
}
